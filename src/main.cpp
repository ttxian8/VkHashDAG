#include <myvk/FrameManager.hpp>
#include <myvk/GLFWHelper.hpp>
#include <myvk/ImGuiHelper.hpp>
#include <myvk/Instance.hpp>
#include <myvk/Queue.hpp>

#include <hashdag/VBREditor.hpp>

#include "Camera.hpp"
#include "DAGColorPool.hpp"
#include "DAGNodePool.hpp"
#include "GPSQueueSelector.hpp"
#include "rg/DAGRenderGraph.hpp"

#include <ThreadPool.h>
#include <chrono>
#include <glm/gtc/type_ptr.hpp>
#include <libfork/schedule/busy_pool.hpp>
#include <cstring>
#include <array>

constexpr uint32_t kFrameCount = 3;

bool cursor_captured = false;
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if (action != GLFW_PRESS)
		return;
	if (key == GLFW_KEY_ESCAPE) {
		cursor_captured ^= 1;
		glfwSetInputMode(window, GLFW_CURSOR, cursor_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}
}

struct AABBEditor {
	glm::u32vec3 aabb_min, aabb_max;
	hashdag::VBRColor color;
	inline hashdag::EditType EditNode(const hashdag::Config<uint32_t> &config,
	                                  const hashdag::NodeCoord<uint32_t> &coord, hashdag::NodePointer<uint32_t>) const {
		auto lb = coord.GetLowerBoundAtLevel(config.GetVoxelLevel()),
		     ub = coord.GetUpperBoundAtLevel(config.GetVoxelLevel());
		/* printf("(%d %d %d), (%d, %d, %d) -> %d\n", lb.x, lb.y, lb.z, ub.x, ub.y, ub.z,
		       !ub.Any(std::less_equal<uint32_t>{}, aabb_min) && !lb.Any(std::greater_equal<uint32_t>{}, aabb_max)); */
		if (glm::any(glm::lessThanEqual(ub, aabb_min)) || glm::any(glm::greaterThanEqual(lb, aabb_max)))
			return hashdag::EditType::kNotAffected;
		if (glm::all(glm::greaterThanEqual(lb, aabb_min)) && glm::all(glm::lessThanEqual(ub, aabb_max)))
			return hashdag::EditType::kFill;
		return hashdag::EditType::kProceed;
	}
	inline hashdag::EditType EditNode(const hashdag::Config<uint32_t> &config,
	                                  const hashdag::NodeCoord<uint32_t> &coord, hashdag::NodePointer<uint32_t> ptr,
	                                  hashdag::VBRColor &final_color) const {
		auto edit_type = EditNode(config, coord, {});
		if (edit_type == hashdag::EditType::kFill || !ptr || final_color == this->color)
			final_color = this->color;
		else
			final_color = {};
		return edit_type;
	}
	inline bool VoxelInRange(const hashdag::NodeCoord<uint32_t> &coord) const {
		return glm::all(glm::greaterThanEqual(coord.pos, aabb_min)) && glm::all(glm::lessThan(coord.pos, aabb_max));
	}
	inline bool EditVoxel(const hashdag::Config<uint32_t> &config, const hashdag::NodeCoord<uint32_t> &coord,
	                      bool voxel) const {
		return voxel || VoxelInRange(coord);
	}
	inline bool EditVoxel(const hashdag::Config<uint32_t> &config, const hashdag::NodeCoord<uint32_t> &coord,
	                      bool voxel, hashdag::VBRColor &color) const {
		bool in_range = VoxelInRange(coord);
		color = in_range || !voxel ? this->color : color;
		return voxel || in_range;
	}
};

enum class EditMode { kFill, kDig, kPaint };
template <EditMode Mode = EditMode::kFill> struct SphereEditor {
	glm::u32vec3 center{};
	uint64_t r2{};
	hashdag::VBRColor color;
	inline hashdag::EditType EditNode(const hashdag::Config<uint32_t> &config,
	                                  const hashdag::NodeCoord<uint32_t> &coord, hashdag::NodePointer<uint32_t>) const {
		auto lb = coord.GetLowerBoundAtLevel(config.GetVoxelLevel()),
		     ub = coord.GetUpperBoundAtLevel(config.GetVoxelLevel());
		glm::i64vec3 lb_dist = glm::i64vec3{lb} - glm::i64vec3(center);
		glm::i64vec3 ub_dist = glm::i64vec3{ub} - glm::i64vec3(center);
		glm::u64vec3 lb_dist_2 = lb_dist * lb_dist;
		glm::u64vec3 ub_dist_2 = ub_dist * ub_dist;

		glm::u64vec3 max_dist_2 = glm::max(lb_dist_2, ub_dist_2);
		uint64_t max_n2 = max_dist_2.x + max_dist_2.y + max_dist_2.z;
		if (max_n2 <= r2)
			return Mode == EditMode::kDig ? hashdag::EditType::kClear : hashdag::EditType::kFill;

		uint64_t min_n2 = 0;
		if (lb_dist.x > 0)
			min_n2 += lb_dist_2.x;
		if (ub_dist.x < 0)
			min_n2 += ub_dist_2.x;
		if (lb_dist.y > 0)
			min_n2 += lb_dist_2.y;
		if (ub_dist.y < 0)
			min_n2 += ub_dist_2.y;
		if (lb_dist.z > 0)
			min_n2 += lb_dist_2.z;
		if (ub_dist.z < 0)
			min_n2 += ub_dist_2.z;

		return min_n2 > r2 ? hashdag::EditType::kNotAffected : hashdag::EditType::kProceed;
	}
	inline hashdag::EditType EditNode(const hashdag::Config<uint32_t> &config,
	                                  const hashdag::NodeCoord<uint32_t> &coord, hashdag::NodePointer<uint32_t> ptr,
	                                  hashdag::VBRColor &final_color) const {
		static_assert(Mode != EditMode::kDig);
		auto edit_type = EditNode(config, coord, {});
		if (edit_type == hashdag::EditType::kFill) {
			final_color = this->color;
			if constexpr (Mode == EditMode::kPaint) {
				edit_type = hashdag::EditType::kNotAffected;
			}
		} else if (!ptr || final_color == this->color) {
			final_color = this->color;
		} else
			final_color = {};
		if constexpr (Mode == EditMode::kPaint) {
			if (!ptr)
				edit_type = hashdag::EditType::kNotAffected;
		}
		return edit_type;
	}
	inline bool VoxelInRange(const hashdag::NodeCoord<uint32_t> &coord) const {
		auto p = coord.pos;
		glm::i64vec3 p_dist = glm::i64vec3{p.x, p.y, p.z} - glm::i64vec3(center);
		uint64_t p_n2 = p_dist.x * p_dist.x + p_dist.y * p_dist.y + p_dist.z * p_dist.z;
		return p_n2 <= r2;
	}
	inline bool EditVoxel(const hashdag::Config<uint32_t> &config, const hashdag::NodeCoord<uint32_t> &coord,
	                      bool voxel) const {
		if constexpr (Mode == EditMode::kPaint)
			return voxel;
		bool in_range = VoxelInRange(coord);
		if constexpr (Mode == EditMode::kFill)
			return voxel || in_range;
		else
			return voxel && !in_range;
	}
	inline bool EditVoxel(const hashdag::Config<uint32_t> &config, const hashdag::NodeCoord<uint32_t> &coord,
	                      bool voxel, hashdag::VBRColor &color) const {
		static_assert(Mode != EditMode::kDig);
		bool in_range = VoxelInRange(coord);
		color = in_range || !voxel ? this->color : color;
		return Mode == EditMode::kFill ? voxel || in_range : voxel;
	}
};

struct VoxImporter {
	struct VoxelData {
		uint8_t x, y, z, color_index;
	};
	
	glm::u32vec3 size{0};
	std::vector<VoxelData> voxels;
	std::array<uint32_t, 256> palette;
	glm::u32vec3 offset{0};
	
	bool LoadFromFile(const std::string& filename) {
		FILE* file = fopen(filename.c_str(), "rb");
		if (!file) return false;
		
		char magic[4];
		if (fread(magic, 1, 4, file) != 4 || memcmp(magic, "VOX ", 4) != 0) {
			fclose(file);
			return false;
		}
		
		uint32_t version;
		if (fread(&version, 4, 1, file) != 1) {
			fclose(file);
			return false;
		}
		
		InitDefaultPalette();
		
		while (!feof(file)) {
			char chunk_id[4];
			uint32_t chunk_size, child_size;
			
			if (fread(chunk_id, 1, 4, file) != 4) break;
			if (fread(&chunk_size, 4, 1, file) != 1) break;
			if (fread(&child_size, 4, 1, file) != 1) break;
			
			if (memcmp(chunk_id, "SIZE", 4) == 0) {
				fread(&size.x, 4, 1, file);
				fread(&size.y, 4, 1, file);
				fread(&size.z, 4, 1, file);
			} else if (memcmp(chunk_id, "XYZI", 4) == 0) {
				uint32_t num_voxels;
				fread(&num_voxels, 4, 1, file);
				voxels.resize(num_voxels);
				fread(voxels.data(), sizeof(VoxelData), num_voxels, file);
			} else if (memcmp(chunk_id, "RGBA", 4) == 0) {
				fread(palette.data(), 4, 256, file);
			} else {
				fseek(file, chunk_size, SEEK_CUR);
			}
		}
		
		fclose(file);
		return !voxels.empty();
	}
	
	void InitDefaultPalette() {
		const uint32_t default_palette[256] = {
			0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff,
			0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
			0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff,
			0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
			0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc,
			0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
			0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc,
			0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
			0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc,
			0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
			0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999,
			0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
			0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099,
			0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
			0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66,
			0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
			0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366,
			0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
			0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33,
			0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
			0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633,
			0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
			0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00,
			0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
			0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600,
			0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
			0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
			0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
			0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700,
			0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
			0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd,
			0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
		};
		std::copy(std::begin(default_palette), std::end(default_palette), palette.begin());
	}
	
	inline hashdag::EditType EditNode(const hashdag::Config<uint32_t> &config,
	                                  const hashdag::NodeCoord<uint32_t> &coord, hashdag::NodePointer<uint32_t> ptr,
	                                  hashdag::VBRColor &color) const {
		auto lb = coord.GetLowerBoundAtLevel(config.GetVoxelLevel());
		auto ub = coord.GetUpperBoundAtLevel(config.GetVoxelLevel());
		
		for (const auto& voxel : voxels) {
			glm::u32vec3 vpos = glm::u32vec3(voxel.x, voxel.y, voxel.z) + offset;
			if (glm::all(glm::greaterThanEqual(vpos, lb)) && glm::all(glm::lessThan(vpos, ub))) {
				return hashdag::EditType::kProceed;
			}
		}
		return hashdag::EditType::kNotAffected;
	}
	
	inline bool EditVoxel(const hashdag::Config<uint32_t> &config, const hashdag::NodeCoord<uint32_t> &coord,
	                      bool voxel, hashdag::VBRColor &color) const {
		glm::u32vec3 coord_pos = coord.pos;
		
		for (const auto& v : voxels) {
			glm::u32vec3 vpos = glm::u32vec3(v.x, v.y, v.z) + offset;
			if (coord_pos == vpos) {
				uint32_t rgba = palette[v.color_index];
				color = hashdag::VBRColor{hashdag::RGB8Color{rgba}};
				return true;
			}
		}
		return voxel;
	}
};

template <typename Func> inline long ns(Func &&func) {
	auto begin = std::chrono::high_resolution_clock::now();
	func();
	auto end = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
}

lf::busy_pool busy_pool(12);

struct EditResult {
	hashdag::NodePointer<uint32_t> node_ptr;
	std::optional<DAGColorPool::Pointer> opt_color_ptr;
};

progschj::ThreadPool edit_pool(1);
std::future<EditResult> edit_future;

float edit_radius = 128.0f;
int render_type = 0;
bool paint = false, beam_opt = false;
glm::vec3 color = {1.f, 0.0f, 0.0f};

int main() {
	GLFWwindow *window = myvk::GLFWCreateWindow("Test", 1280, 720, true);
	glfwSetKeyCallback(window, key_callback);

	myvk::Ptr<myvk::Device> device;
	myvk::Ptr<myvk::Queue> generic_queue, sparse_queue;
	myvk::Ptr<myvk::PresentQueue> present_queue;
	{
		auto instance = myvk::Instance::CreateWithGlfwExtensions();
		auto surface = myvk::Surface::Create(instance, window);
		auto physical_device = myvk::PhysicalDevice::Fetch(instance)[0];
		auto features = physical_device->GetDefaultFeatures();
		features.vk12.samplerFilterMinmax = VK_TRUE;
		device = myvk::Device::Create(physical_device,
		                              GPSQueueSelector{&generic_queue, &sparse_queue, surface, &present_queue},
		                              features, {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_EXTENSION_NAME});
	}

	auto frame_manager = myvk::FrameManager::Create(generic_queue, present_queue, false, kFrameCount);

	auto dag_node_pool = DAGNodePool::Create(
	    hashdag::DefaultConfig<uint32_t>{
	        .level_count = 17,
	        .top_level_count = 9,
	        .word_bits_per_page = 14,
	        .page_bits_per_bucket = 2,
	        .bucket_bits_per_top_level = 7,
	        .bucket_bits_per_bottom_level = 11,
	    }(),
	    {generic_queue, sparse_queue});
	auto dag_color_pool = DAGColorPool::Create(
	    DAGColorPool::Config{
	        .leaf_level = 10,
	        .node_bits_per_node_page = 18,
	        .word_bits_per_leaf_page = 24,
	        .keep_history = false,
	    },
	    {generic_queue, sparse_queue});
	auto sparse_binder = myvk::MakePtr<VkSparseBinder>(sparse_queue);

	const auto edit = [&]<hashdag::Editor<uint32_t> Editor_T>(Editor_T &&editor) -> EditResult {
		return dag_node_pool->ThreadedEdit(&busy_pool, dag_node_pool->GetRoot(), std::forward<Editor_T>(editor),
		                                   dag_color_pool->GetLeafLevel(),
		                                   [&](hashdag::NodePointer<uint32_t> root_ptr, auto &&state) -> EditResult {
			                                   if constexpr (requires { state.octree_node; })
				                                   return {root_ptr, state.octree_node};
			                                   else
				                                   return {root_ptr, std::nullopt};
		                                   });
	};
	const auto vbr_edit = [&]<hashdag::VBREditor<uint32_t> VBREditor_T>(VBREditor_T &&vbr_editor) {
		return edit(hashdag::VBREditorWrapper<uint32_t, VBREditor_T, DAGColorPool>{
		    .editor = std::forward<VBREditor_T>(vbr_editor),
		    .p_octree = dag_color_pool.get(),
		    .octree_root = dag_color_pool->GetRoot(),
		});
	};
	const auto stateless_edit = [&]<hashdag::StatelessEditor<uint32_t> StatelessEditor_T>(StatelessEditor_T &&editor) {
		return edit(hashdag::StatelessEditorWrapper<uint32_t, StatelessEditor_T>{
		    .editor = std::forward<StatelessEditor_T>(editor)});
	};
	const auto gc = [&]() -> EditResult {
		return {.node_ptr = dag_node_pool->ThreadedGC(&busy_pool, dag_node_pool->GetRoot()),
		        .opt_color_ptr = std::nullopt};
	};
	const auto set_root = [&](const EditResult &edit_result) {
		dag_node_pool->SetRoot(edit_result.node_ptr);
		if (edit_result.opt_color_ptr)
			dag_color_pool->SetRoot(*edit_result.opt_color_ptr);
	};
	const auto flush = [&]() {
		dag_node_pool->Flush(sparse_binder);
		dag_color_pool->Flush(sparse_binder);
		auto fence = myvk::Fence::Create(device);
		if (sparse_binder->QueueBind({}, {}, fence) == VK_SUCCESS)
			fence->Wait();
	};

	{
		auto edit_ns = ns([&]() {
			set_root(vbr_edit(AABBEditor{
			    .aabb_min = {1001, 1000, 1000},
			    .aabb_max = {10000, 10000, 10000},
			    .color = hashdag::RGB8Color{0xFFFFFF},
			}));
			set_root(vbr_edit(AABBEditor{
			    .aabb_min = {0, 0, 0},
			    .aabb_max = {5000, 5000, 5000},
			    .color = hashdag::RGB8Color{0x00FFFF},
			}));
			set_root(vbr_edit(SphereEditor<EditMode::kPaint>{
			    .center = {5005, 5000, 5000},
			    .r2 = 2000 * 2000,
			    .color = hashdag::RGB8Color{0x007FFF},
			}));
			set_root(stateless_edit(SphereEditor<EditMode::kDig>{
			    .center = {10000, 10000, 10000},
			    .r2 = 4000 * 4000,
			    .color = {},
			}));
		});
		printf("edit cost %lf ms\n", (double)edit_ns / 1000000.0);
		printf("root = %d\n", dag_color_pool->GetRoot().GetData());
		auto flush_ns = ns([&]() { flush(); });
		printf("flush cost %lf ms\n", (double)flush_ns / 1000000.0);
	}

	const auto pop_edit_result = [&]() {
		if (edit_future.valid() && edit_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			set_root(edit_future.get());
	};
	const auto push_edit = [&]<typename... Args>(auto &&edit_func, Args &&...args) {
		if (edit_future.valid())
			return;
		edit_future = edit_pool.enqueue([&]() {
			EditResult result;
			auto edit_ns = ns([&]() { result = edit_func(std::forward<Args>(args)...); });
			printf("edit cost %lf ms\n", (double)edit_ns / 1000000.0);
			auto flush_ns = ns([&]() { flush(); });
			printf("flush cost %lf ms\n", (double)flush_ns / 1000000.0);
			return result;
		});
	};

	auto camera = myvk::MakePtr<Camera>();
	camera->m_speed = 0.01f;

	myvk::ImGuiInit(window, myvk::CommandPool::Create(generic_queue));

	std::array<myvk::Ptr<rg::DAGRenderGraph>, kFrameCount> render_graphs;
	for (auto &rg : render_graphs)
		rg = myvk::MakePtr<rg::DAGRenderGraph>(frame_manager, camera, dag_node_pool, dag_color_pool, beam_opt);

	double prev_time = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		double time = glfwGetTime(), delta = time - prev_time;
		prev_time = time;

		glfwPollEvents();

		pop_edit_result();

		if (cursor_captured) {
			camera->MoveControl(window, float(delta));

			std::optional<glm::vec3> p =
			    dag_node_pool->Traversal<float>(dag_node_pool->GetRoot(), camera->m_position, camera->GetLook());
			if (p) {
				glm::u32vec3 up = *p * glm::vec3((float)dag_node_pool->GetConfig().GetResolution());
				auto r2 = uint64_t(edit_radius * edit_radius);

				if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
					push_edit(stateless_edit, SphereEditor<EditMode::kDig>{
					                              .center = up,
					                              .r2 = r2,
					                          });
				} else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
					if (paint)
						push_edit(vbr_edit, SphereEditor<EditMode::kPaint>{
						                        .center = up,
						                        .r2 = r2,
						                        .color = hashdag::VBRColor{color},
						                    });
					else
						push_edit(vbr_edit, SphereEditor<EditMode::kFill>{
						                        .center = up,
						                        .r2 = r2,
						                        .color = hashdag::VBRColor{color},
						                    });

					// A Simple Test for Weighted VBR Color
					/* if (paint)
					    push_edit(vbr_edit, SphereEditor<EditMode::kPaint>{
					                            .center = up,
					                            .r2 = r2,
					                            .color = hashdag::VBRColor{hashdag::R5G6B5Color{color},
					                                                       hashdag::R5G6B5Color{}, 1u, 2u},
					                        });
					else
					    push_edit(vbr_edit, SphereEditor<EditMode::kFill>{
					                            .center = up,
					                            .r2 = r2,
					                            .color = hashdag::VBRColor{hashdag::R5G6B5Color{color},
					                                                       hashdag::R5G6B5Color{}, 0u, 2u},
					                        }); */
				}
			}
		}

		myvk::ImGuiNewFrame();
		ImGui::Begin("Test");
		ImGui::Text("FPS %f", ImGui::GetIO().Framerate);
		ImGui::DragFloat("Radius", &edit_radius, 1.0f, 0.0f, 2048.0f);
		ImGui::DragFloat("Speed", &camera->m_speed, 0.0001f, 0.0001f, 0.25f);
		ImGui::Checkbox("Beam Optimization", &beam_opt);
		ImGui::Combo("Type", &render_type, "Diffuse\0Normal\0Iteration\0");
		ImGui::ColorEdit3("Color", glm::value_ptr(color));
		ImGui::Checkbox("Paint", &paint);
		if (ImGui::Button("GC")) {
			auto gc_ns = ns([&]() { set_root(gc()); });
			printf("GC cost %lf ms\n", (double)gc_ns / 1000000.0);
			auto flush_ns = ns([&]() { flush(); });
			printf("flush cost %lf ms\n", (double)flush_ns / 1000000.0);
		}
		
		if (ImGui::Button("Import VOX")) {
			static char file_path[256] = "";
			if (ImGui::InputText("VOX File Path", file_path, sizeof(file_path), ImGuiInputTextFlags_EnterReturnsTrue)) {
				static VoxImporter vox_importer;
				if (vox_importer.LoadFromFile(std::string(file_path))) {
					vox_importer.offset = {20000, 20000, 20000};
					push_edit(vbr_edit, vox_importer);
					printf("VOX file imported: %s\n", file_path);
				} else {
					printf("Failed to load VOX file: %s\n", file_path);
				}
			}
		}
		const auto imgui_paged_buffer_info = [](const char *name, const myvk::Ptr<VkPagedBuffer> &buffer) {
			ImGui::Text("%s: %u / %u Page, %.2lf MiB", name, buffer->GetExistPageTotal(), buffer->GetPageTotal(),
			            double(buffer->GetExistPageTotal() * buffer->GetPageSize()) / 1024.0 / 1024.0);
		};
		imgui_paged_buffer_info("Node", dag_node_pool->GetBuffer());
		imgui_paged_buffer_info("Color Node", dag_color_pool->GetNodeBuffer());
		imgui_paged_buffer_info("Color Leaf", dag_color_pool->GetLeafBuffer());
		ImGui::End();
		ImGui::Render();

		if (frame_manager->NewFrame()) {
			uint32_t current_frame = frame_manager->GetCurrentFrame();
			auto &render_graph = render_graphs[frame_manager->GetCurrentFrame()];

			const auto &command_buffer = frame_manager->GetCurrentCommandBuffer();

			command_buffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			render_graph->SetRenderType(render_type);
			render_graph->SetBeamOpt(beam_opt);
			render_graph->SetCanvasSize(frame_manager->GetExtent());
			render_graph->CmdExecute(command_buffer);
			command_buffer->End();

			frame_manager->Render();
		}
	}

	frame_manager->WaitIdle();
	glfwTerminate();
	return 0;
}
