# Rendering Pipeline

VkHashDAG implements a modern Vulkan-based rendering pipeline optimized for real-time ray tracing through HashDAG data structures. This document details the rendering architecture, shader pipeline, and GPU resource management.

## Overview

The rendering system uses a render graph architecture with multiple specialized passes:

1. **TracePass**: Primary ray tracing through the HashDAG
2. **BeamPass**: Beam optimization for improved performance
3. **CrosshairPass**: UI overlay rendering

## Render Graph Architecture

### DAGRenderGraph Structure

```cpp
class DAGRenderGraph : public myvk_rg::RenderGraphBase {
    myvk::Ptr<myvk::FrameManager> m_frame_manager_ptr;
    myvk::Ptr<Camera> m_camera_ptr;
    myvk::Ptr<DAGNodePool> m_node_pool_ptr;
    myvk::Ptr<DAGColorPool> m_color_pool_ptr;
    bool m_beam_opt{false};
};
```

### Resource Management

The render graph manages several key resources:

```cpp
void DAGRenderGraph::make_resources() {
    // Main render target
    auto image = CreateResource<myvk_rg::Image>("image");
    
    // Optional beam optimization buffer
    auto beam = CreateResource<myvk_rg::Image>("beam");
    
    // HashDAG node buffer
    auto dag_nodes = CreateResource<myvk_rg::Buffer>("dag_nodes");
    
    // Color system buffers
    auto color_nodes = CreateResource<myvk_rg::Buffer>("color_nodes");
    auto color_leaves = CreateResource<myvk_rg::Buffer>("color_leaves");
}
```

## TracePass - Primary Ray Tracing

### Pass Configuration

```cpp
class TracePass : public myvk_rg::GraphicsPassBase {
    struct Args {
        const myvk_rg::Image &image;
        const std::optional<myvk_rg::Image> &opt_beam;
        const myvk_rg::Buffer &dag_nodes, &color_nodes, &color_leaves;
        const myvk::Ptr<Camera> &camera;
        const myvk::Ptr<DAGNodePool> &node_pool;
        const myvk::Ptr<DAGColorPool> &color_pool;
    };
};
```

### Ray Tracing Algorithm

The TracePass implements hierarchical ray tracing through the HashDAG:

#### Shader Pipeline
```glsl
// trace.frag - Fragment shader for ray tracing
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

// Uniform buffers
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 camera_pos;
    vec3 camera_dir;
} camera;

// HashDAG node buffer
layout(set = 1, binding = 0) readonly buffer DAGNodeBuffer {
    uint dag_nodes[];
};

// Color buffers
layout(set = 1, binding = 1) readonly buffer ColorNodeBuffer {
    uint color_nodes[];
};

layout(set = 1, binding = 2) readonly buffer ColorLeafBuffer {
    uint color_leaves[];
};

void main() {
    // Generate ray from screen coordinates
    vec3 ray_origin = camera.camera_pos;
    vec3 ray_dir = generate_ray_direction(in_uv);
    
    // Trace through HashDAG
    vec4 color = trace_hashdag(ray_origin, ray_dir);
    out_color = color;
}
```

#### HashDAG Traversal
```glsl
vec4 trace_hashdag(vec3 ray_origin, vec3 ray_dir) {
    // Stack for hierarchical traversal
    struct StackEntry {
        uint node_ptr;
        vec3 box_min;
        vec3 box_max;
        uint level;
    };
    
    StackEntry stack[32];
    int stack_top = 0;
    
    // Initialize with root node
    stack[0] = StackEntry(
        get_root_node(),
        vec3(-1.0), vec3(1.0),
        0
    );
    
    vec4 accumulated_color = vec4(0.0);
    float accumulated_alpha = 0.0;
    
    while (stack_top >= 0 && accumulated_alpha < 0.99) {
        StackEntry entry = stack[stack_top--];
        
        // Ray-box intersection test
        vec2 t_range = ray_box_intersection(
            ray_origin, ray_dir, 
            entry.box_min, entry.box_max
        );
        
        if (t_range.x > t_range.y) continue;
        
        if (entry.level == MAX_LEVEL) {
            // Leaf node - sample voxels
            vec4 leaf_color = sample_leaf_node(
                entry.node_ptr, ray_origin, ray_dir, 
                entry.box_min, entry.box_max
            );
            
            // Alpha blending
            accumulated_color += leaf_color * (1.0 - accumulated_alpha);
            accumulated_alpha += leaf_color.a * (1.0 - accumulated_alpha);
        } else {
            // Inner node - add children to stack
            uint child_mask = get_node_child_mask(entry.node_ptr);
            vec3 box_center = (entry.box_min + entry.box_max) * 0.5;
            
            // Process children in front-to-back order
            for (int i = 0; i < 8; ++i) {
                if ((child_mask & (1u << i)) != 0) {
                    vec3 child_min, child_max;
                    calculate_child_bounds(
                        entry.box_min, entry.box_max, i,
                        child_min, child_max
                    );
                    
                    uint child_ptr = get_child_pointer(entry.node_ptr, i);
                    stack[++stack_top] = StackEntry(
                        child_ptr, child_min, child_max, entry.level + 1
                    );
                }
            }
        }
    }
    
    return accumulated_color;
}
```

### Performance Optimizations

#### Early Ray Termination
```glsl
// Terminate rays that have accumulated sufficient opacity
if (accumulated_alpha > 0.99) break;

// Skip empty space using beam optimization
if (beam_optimization_enabled) {
    vec2 beam_data = texture(beam_texture, in_uv).xy;
    if (beam_data.x > current_distance) {
        // Skip to beam intersection point
        current_distance = beam_data.x;
        continue;
    }
}
```

#### Memory Access Optimization
```glsl
// Coalesced memory access for node data
uint node_data = dag_nodes[node_ptr];
uint child_mask = node_data & 0xFF;
uint child_base = node_data >> 8;

// Cache-friendly color lookup
vec4 sample_color(uint color_ptr) {
    // Use texture cache for color data
    return texelFetch(color_texture, ivec2(color_ptr & 0xFFFF, color_ptr >> 16), 0);
}
```

## BeamPass - Performance Optimization

### Beam Optimization Concept

The BeamPass implements beam optimization to skip empty space during ray tracing:

```cpp
class BeamPass : public myvk_rg::GraphicsPassBase {
    // Pre-compute beam intersection distances
    // Store minimum and maximum intersection distances per pixel
};
```

### Beam Computation Shader
```glsl
// beam.frag - Compute beam optimization data
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec2 out_beam_data;

void main() {
    vec3 ray_origin = camera.camera_pos;
    vec3 ray_dir = generate_ray_direction(in_uv);
    
    // Find first and last intersections with non-empty space
    vec2 intersection_range = compute_beam_range(ray_origin, ray_dir);
    
    out_beam_data = intersection_range;
}
```

## CrosshairPass - UI Overlay

### UI Rendering

```cpp
class CrosshairPass : public myvk_rg::GraphicsPassBase {
    // Render crosshair and UI elements over the main image
};
```

### Crosshair Shader
```glsl
// crosshair.frag - Simple UI overlay
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 center = vec2(0.5);
    vec2 diff = abs(in_uv - center);
    
    // Draw crosshair lines
    if (diff.x < 0.001 || diff.y < 0.001) {
        if (length(diff) < 0.05) {
            out_color = vec4(1.0, 1.0, 1.0, 0.8);
            return;
        }
    }
    
    out_color = vec4(0.0);
}
```

## GPU Resource Management

### Buffer Management

#### Sparse Buffer Binding
```cpp
void DAGRenderGraph::PreExecute() const {
    // Update GPU buffers with latest HashDAG data
    m_node_pool_ptr->Flush(sparse_binder);
    m_color_pool_ptr->Flush(sparse_binder);
    
    // Ensure proper synchronization
    vkDeviceWaitIdle(device);
}
```

#### Descriptor Set Updates
```cpp
void TracePass::CmdExecute(const myvk::Ptr<myvk::CommandBuffer>& cmd_buf) const {
    // Bind descriptor sets
    vkCmdBindDescriptorSets(
        cmd_buf->GetHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout,
        0, 1, &camera_descriptor_set,
        0, nullptr
    );
    
    vkCmdBindDescriptorSets(
        cmd_buf->GetHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout,
        1, 1, &hashdag_descriptor_set,
        0, nullptr
    );
    
    // Draw fullscreen quad
    vkCmdDraw(cmd_buf->GetHandle(), 3, 1, 0, 0);
}
```

### Memory Synchronization

#### Pipeline Barriers
```cpp
void insert_memory_barriers(VkCommandBuffer cmd_buf) {
    VkMemoryBarrier memory_barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    
    vkCmdPipelineBarrier(
        cmd_buf,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &memory_barrier, 0, nullptr, 0, nullptr
    );
}
```

## Shader Compilation and Management

### Shader Loading System

```cpp
class ShaderManager {
    std::unordered_map<std::string, VkShaderModule> shader_modules;
    
    VkShaderModule load_shader(const std::string& filename) {
        // Load SPIR-V bytecode
        auto spirv_code = read_spirv_file(filename);
        
        VkShaderModuleCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv_code.size() * sizeof(uint32_t),
            .pCode = spirv_code.data()
        };
        
        VkShaderModule shader_module;
        vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
        
        return shader_module;
    }
};
```

### Pipeline Creation

```cpp
myvk::Ptr<myvk::GraphicsPipeline> TracePass::CreatePipeline() const {
    // Vertex shader (fullscreen quad)
    auto vert_shader = myvk::ShaderModule::Create(
        GetDevicePtr(), "quad.vert.spv"
    );
    
    // Fragment shader (ray tracing)
    auto frag_shader = myvk::ShaderModule::Create(
        GetDevicePtr(), "trace.frag.spv"
    );
    
    // Pipeline configuration
    auto pipeline = myvk::GraphicsPipeline::Create(
        GetRenderPassPtr(),
        {vert_shader, frag_shader},
        {GetDescriptorSetLayoutPtr()},
        GetSubpass()
    );
    
    return pipeline;
}
```

## Performance Characteristics

### Rendering Performance

#### Typical Frame Times
- **1080p**: 16-33ms (30-60 FPS)
- **1440p**: 25-50ms (20-40 FPS)
- **4K**: 50-100ms (10-20 FPS)

#### Memory Bandwidth
- **HashDAG Traversal**: ~50-100 GB/s
- **Color Lookups**: ~20-50 GB/s
- **Total GPU Memory**: 1-8 GB depending on scene complexity

### Optimization Techniques

#### Level-of-Detail (LOD)
```glsl
uint calculate_lod(vec3 ray_origin, vec3 box_center, float box_size) {
    float distance = length(ray_origin - box_center);
    float angular_size = box_size / distance;
    
    // Use lower detail for distant objects
    if (angular_size < 0.001) return MAX_LEVEL - 2;
    if (angular_size < 0.01) return MAX_LEVEL - 1;
    return MAX_LEVEL;
}
```

#### Adaptive Sampling
```glsl
// Reduce sampling rate for uniform regions
float calculate_step_size(vec3 position, vec3 direction) {
    float base_step = 0.001;
    
    // Larger steps in empty space
    if (is_empty_region(position)) {
        return base_step * 4.0;
    }
    
    // Smaller steps near surfaces
    float distance_to_surface = estimate_distance_to_surface(position);
    return base_step * max(1.0, distance_to_surface * 10.0);
}
```

## Debugging and Profiling

### Render Debug Modes

```cpp
enum class RenderType {
    kNormal = 0,
    kDepth = 1,
    kNormals = 2,
    kLevels = 3,
    kIterations = 4
};

void TracePass::SetRenderType(uint32_t type) {
    m_render_type = type;
    // Update shader uniforms
}
```

### Performance Profiling

```cpp
class RenderProfiler {
    std::vector<float> frame_times;
    std::vector<uint32_t> ray_counts;
    
    void begin_frame() {
        frame_start_time = std::chrono::high_resolution_clock::now();
    }
    
    void end_frame() {
        auto frame_end_time = std::chrono::high_resolution_clock::now();
        float frame_time = std::chrono::duration<float, std::milli>(
            frame_end_time - frame_start_time
        ).count();
        
        frame_times.push_back(frame_time);
    }
};
```

## Related Documentation

- [Architecture Overview](Architecture-Overview) - System-wide design
- [Memory Management](Memory-Management) - GPU resource management
- [HashDAG Implementation](HashDAG-Implementation) - Data structure details
- [Usage Guide](Usage-Guide) - Interactive controls and features
