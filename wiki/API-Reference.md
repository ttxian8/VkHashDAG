---
layout: default
title: API Reference
nav_order: 8
description: "Complete class and interface documentation with examples"
---

# API Reference
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

This document provides a comprehensive reference for the key classes and interfaces in VkHashDAG.

## Core Classes

### DAGNodePool

The main class for managing HashDAG node storage and allocation.

```cpp
class DAGNodePool : public myvk::DeviceObjectBase,
                   public hashdag::NodePoolBase<DAGNodePool, uint32_t>,
                   public hashdag::NodePoolTraversal<DAGNodePool, uint32_t>,
                   public hashdag::NodePoolThreadedEdit<DAGNodePool, uint32_t>,
                   public hashdag::NodePoolThreadedGC<DAGNodePool, uint32_t>
```

#### Constructor
```cpp
DAGNodePool(const hashdag::Config<uint32_t>& config, 
           myvk::Ptr<VkPagedBuffer> buffer);
```

#### Static Factory Method
```cpp
static myvk::Ptr<DAGNodePool> Create(
    hashdag::Config<uint32_t> config,
    const std::vector<myvk::Ptr<myvk::Queue>>& queues
);
```

#### Key Methods

##### Memory Management
```cpp
// Read page data (thread-safe)
const uint32_t* ReadPage(uint32_t page_id) const;

// Write data to page
void WritePage(uint32_t page_id, uint32_t page_offset, 
               std::span<const uint32_t> word_span);

// Zero out page region
void ZeroPage(uint32_t page_id, uint32_t page_offset, uint32_t zero_words);

// Free page for reuse
void FreePage(uint32_t page_id);
```

##### Node Operations
```cpp
// Set root node pointer
void SetRoot(hashdag::NodePointer<uint32_t> root);

// Get root node pointer
hashdag::NodePointer<uint32_t> GetRoot() const;

// Flush changes to GPU
void Flush(const myvk::Ptr<VkSparseBinder>& binder);
```

##### Thread Safety
```cpp
// Get mutex for bucket-level locking
std::mutex& GetBucketRefMutex(uint32_t bucket_id);

// Get reference to bucket word count
uint32_t& GetBucketRefWords(uint32_t bucket_id);
```

### DAGColorPool

Manages VBR (Variable Bit Rate) color data with octree organization.

```cpp
class DAGColorPool : public myvk::DeviceObjectBase
```

#### Nested Types

##### Pointer
```cpp
struct Pointer {
    enum class Tag { kNode = 0, kColor, kLeaf, kNull };
    uint32_t pointer;
    
    Pointer();
    Pointer(Tag tag, uint32_t data);
    
    Tag GetTag() const;
    uint32_t GetData() const;
    bool operator==(const Pointer& r) const;
};
```

##### Configuration
```cpp
struct Config {
    uint32_t leaf_level;
    uint32_t node_bits_per_node_page;
    uint32_t word_bits_per_leaf_page;
    bool keep_history;
};
```

#### Constructor
```cpp
DAGColorPool(const Config& config, 
            myvk::Ptr<VkPagedBuffer> node_buffer,
            myvk::Ptr<VkPagedBuffer> leaf_buffer);
```

#### Static Factory Method
```cpp
static myvk::Ptr<DAGColorPool> Create(
    Config config, 
    const std::vector<myvk::Ptr<myvk::Queue>>& queues
);
```

#### Key Methods

##### Tree Operations
```cpp
// Get child pointer
Pointer GetChild(Pointer ptr, auto idx) const;

// Get fill color for pointer
static hashdag::VBRColor GetFill(Pointer ptr);

// Set node with child pointers
Pointer SetNode(Pointer ptr, std::span<const Pointer, 8> child_ptrs);

// Clear node (set to null)
static Pointer ClearNode(Pointer ptr);

// Fill node with solid color
static Pointer FillNode(Pointer ptr, hashdag::VBRColor color);
```

##### Leaf Operations
```cpp
// Get leaf data
hashdag::VBRChunk<uint32_t, SafeLeafSpan> GetLeaf(Pointer ptr) const;

// Set leaf data
Pointer SetLeaf(Pointer ptr, 
               hashdag::VBRChunk<uint32_t, hashdag::VBRWriterContainer>&& chunk);

// Get leaf level
uint32_t GetLeafLevel() const;
```

##### Root Management
```cpp
// Get root pointer
Pointer GetRoot() const;

// Set root pointer
void SetRoot(Pointer root);
```

##### GPU Integration
```cpp
// Flush changes to GPU
void Flush(const myvk::Ptr<VkSparseBinder>& binder);

// Get GPU buffers
const auto& GetNodeBuffer() const;
const auto& GetLeafBuffer() const;
```

## HashDAG Core Types

### Config Template

Configuration for HashDAG structure and memory layout.

```cpp
template<std::unsigned_integral Word>
struct Config {
    Word word_bits_per_page;
    Word page_bits_per_bucket;
    std::vector<Word> bucket_bits_each_level;
    
    // Computed properties
    Word GetWordsPerPage() const;
    Word GetPagesPerBucket() const;
    Word GetWordsPerBucket() const;
    Word GetBucketsAtLevel(Word level) const;
    Word GetNodeLevels() const;
    Word GetLeafLevel() const;
    Word GetVoxelLevel() const;
    Word GetResolution() const;
    Word GetTotalBuckets() const;
    Word GetTotalPages() const;
    Word GetTotalWords() const;
    
    // Validation
    static bool Validate(const Config& config);
};
```

### DefaultConfig Template

Provides sensible default configurations.

```cpp
template<std::unsigned_integral Word>
struct DefaultConfig {
    uint32_t level_count = 17;
    uint32_t top_level_count = 9;
    Word word_bits_per_page = 9;            // 512 words = 2KB
    Word page_bits_per_bucket = 2;          // 4 pages per bucket
    Word bucket_bits_per_top_level = 10;    // 1024 buckets
    Word bucket_bits_per_bottom_level = 16; // 65536 buckets
    
    Config<Word> operator()() const;
};
```

### NodePointer Template

Represents pointers to nodes in the HashDAG.

```cpp
template<std::unsigned_integral Word>
struct NodePointer {
    Word pointer;
    
    // Factory methods
    static NodePointer Null();
    static NodePointer Filled();
    
    // State queries
    bool IsNull() const;
    bool IsFilled() const;
    
    // Conversion
    explicit operator bool() const;
    Word operator*() const;
    
    // Comparison
    bool operator==(const NodePointer& r) const;
    bool operator!=(const NodePointer& r) const;
};
```

### NodeCoord Template

Represents coordinates within the HashDAG hierarchy.

```cpp
template<std::unsigned_integral Word>
struct NodeCoord {
    Word level;
    Word x, y, z;
    
    // Child operations
    NodeCoord GetChild(Word child_index) const;
    Word GetChildIndex() const;
    
    // Level queries
    bool IsLeafLevel(const Config<Word>& config) const;
    bool IsVoxelLevel(const Config<Word>& config) const;
    
    // Spatial operations
    Word GetSize(const Config<Word>& config) const;
    std::array<NodeCoord, 8> GetChildren() const;
};
```

## Editor Interfaces

### Editor Concept

Base concept for voxel editing operations.

```cpp
template<typename T, typename Word>
concept Editor = requires(const T ce) {
    { ce.EditNode(Config<Word>{}, NodeCoord<Word>{}, NodePointer<Word>{}) } 
        -> std::convertible_to<EditType>;
    { ce.EditVoxel(Config<Word>{}, NodeCoord<Word>{}, bool{}) } 
        -> std::convertible_to<bool>;
} && std::unsigned_integral<Word>;
```

### EditType Enumeration

```cpp
enum class EditType {
    kNotAffected,  // No change needed
    kClear,        // Clear this subtree
    kFill,         // Fill this subtree
    kProceed       // Continue to children
};
```

### VBREditor Concept

Extended editor concept for color-aware editing.

```cpp
template<typename T, typename Word>
concept VBREditor = requires(const T ce) {
    { ce.EditNode(Config<Word>{}, NodeCoord<Word>{}, NodePointer<Word>{}, 
                  std::declval<VBRColor&>()) } -> std::convertible_to<EditType>;
    { ce.EditVoxel(Config<Word>{}, NodeCoord<Word>{}, bool{}, 
                   std::declval<VBRColor&>()) } -> std::convertible_to<bool>;
} && std::unsigned_integral<Word>;
```

## Color System

### VBRColor

Variable Bit Rate color representation.

```cpp
class VBRColor {
    uint32_t data;
    
public:
    // Constructors
    VBRColor();
    VBRColor(uint32_t color_data);
    VBRColor(RGB8Color color);
    
    // Accessors
    uint32_t Get() const;
    bool IsEmpty() const;
    
    // Operators
    explicit operator bool() const;
    bool operator==(const VBRColor& other) const;
};
```

### RGB8Color

8-bit RGB color representation.

```cpp
struct RGB8Color {
    uint8_t r, g, b;
    
    RGB8Color();
    RGB8Color(uint8_t red, uint8_t green, uint8_t blue);
    RGB8Color(uint32_t packed);
    
    uint32_t GetData() const;
    uint32_t Pack() const;
};
```

### VBRChunk Template

Container for compressed color data.

```cpp
template<typename Word, template<typename> typename Container>
struct VBRChunk {
    Container<VBRMacroBlock> macro_blocks;
    Container<VBRBlockHeader> block_headers;
    VBRBitset<Word, Container> weight_bits;
    
    // Size queries
    size_t GetMacroBlockCount() const;
    size_t GetBlockHeaderCount() const;
    size_t GetBitWordCount() const;
    
    // Data access
    const auto& GetMacroBlocks() const;
    const auto& GetBlockHeaders() const;
    const auto& GetWeightBits() const;
};
```

## Rendering Classes

### DAGRenderGraph

Main render graph coordinating all rendering passes.

```cpp
class DAGRenderGraph : public myvk_rg::RenderGraphBase {
public:
    DAGRenderGraph(const myvk::Ptr<myvk::FrameManager>& frame_manager,
                   const myvk::Ptr<Camera>& camera,
                   const myvk::Ptr<DAGNodePool>& node_pool,
                   const myvk::Ptr<DAGColorPool>& color_pool,
                   bool beam_opt);
    
    // Configuration
    void SetRenderType(uint32_t type);
    void SetBeamOpt(bool enabled);
    
    // Execution
    void PreExecute() const override;
};
```

### TracePass

Primary ray tracing pass.

```cpp
class TracePass : public myvk_rg::GraphicsPassBase {
public:
    struct Args {
        const myvk_rg::Image& image;
        const std::optional<myvk_rg::Image>& opt_beam;
        const myvk_rg::Buffer& dag_nodes;
        const myvk_rg::Buffer& color_nodes;
        const myvk_rg::Buffer& color_leaves;
        const myvk::Ptr<Camera>& camera;
        const myvk::Ptr<DAGNodePool>& node_pool;
        const myvk::Ptr<DAGColorPool>& color_pool;
    };
    
    TracePass(myvk_rg::Parent parent, const Args& args);
    
    // Pipeline management
    myvk::Ptr<myvk::GraphicsPipeline> CreatePipeline() const override;
    void CmdExecute(const myvk::Ptr<myvk::CommandBuffer>& cmd_buf) const override;
    
    // Configuration
    void SetRenderType(uint32_t type);
    
    // Output
    auto GetImageOutput() const;
};
```

## Utility Classes

### Camera

Camera management for 3D navigation.

```cpp
class Camera : public myvk::DeviceObjectBase {
public:
    // View matrix operations
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    
    // Position and orientation
    glm::vec3 GetPosition() const;
    glm::vec3 GetDirection() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;
    
    // Movement
    void Move(const glm::vec3& delta);
    void Rotate(float yaw_delta, float pitch_delta);
    
    // Configuration
    void SetFOV(float fov);
    void SetAspectRatio(float aspect);
    void SetNearFar(float near_plane, float far_plane);
};
```

### VkPagedBuffer

Vulkan sparse buffer management.

```cpp
class VkPagedBuffer : public myvk::DeviceObjectBase {
public:
    // Factory method
    static myvk::Ptr<VkPagedBuffer> Create(
        const myvk::Ptr<myvk::Device>& device,
        VkBufferUsageFlags usage,
        uint32_t page_size,
        uint32_t page_total
    );
    
    // Properties
    VkBuffer GetBuffer() const;
    VkDeviceMemory GetMemory() const;
    uint32_t GetPageSize() const;
    uint32_t GetPageTotal() const;
    
    // Memory management
    void BindPage(uint32_t page_id, VkDeviceSize offset);
    void UnbindPage(uint32_t page_id);
};
```

## Thread Pool Integration

### ThreadPool

Parallel execution support using libfork.

```cpp
class ThreadPool {
public:
    ThreadPool(size_t thread_count);
    ~ThreadPool();
    
    // Task submission
    template<typename Func>
    auto submit(Func&& func) -> std::future<decltype(func())>;
    
    // Parallel algorithms
    template<typename Iterator, typename Func>
    void parallel_for(Iterator begin, Iterator end, Func func);
    
    // Configuration
    size_t GetThreadCount() const;
    void SetThreadCount(size_t count);
};
```

## Error Handling

### Exception Types

```cpp
// Base exception for VkHashDAG errors
class VkHashDAGException : public std::exception {
public:
    VkHashDAGException(const std::string& message);
    const char* what() const noexcept override;
};

// Memory allocation failures
class OutOfMemoryException : public VkHashDAGException {
public:
    OutOfMemoryException(const std::string& context);
};

// Vulkan API errors
class VulkanException : public VkHashDAGException {
public:
    VulkanException(VkResult result, const std::string& operation);
    VkResult GetResult() const;
};

// Configuration validation errors
class ConfigException : public VkHashDAGException {
public:
    ConfigException(const std::string& parameter, const std::string& issue);
};
```

## Usage Examples

### Basic Setup

```cpp
// Create configuration
hashdag::DefaultConfig<uint32_t> default_config;
auto config = default_config();

// Create queues
std::vector<myvk::Ptr<myvk::Queue>> queues = {graphics_queue, compute_queue};

// Create node pool
auto node_pool = DAGNodePool::Create(config, queues);

// Create color pool
DAGColorPool::Config color_config{
    .leaf_level = 10,
    .node_bits_per_node_page = 18,
    .word_bits_per_leaf_page = 24,
    .keep_history = false
};
auto color_pool = DAGColorPool::Create(color_config, queues);

// Create render graph
auto render_graph = std::make_shared<rg::DAGRenderGraph>(
    frame_manager, camera, node_pool, color_pool, true
);
```

### Voxel Editing

```cpp
// Define sphere editor
struct SphereEditor {
    glm::vec3 center;
    float radius;
    bool fill_mode;
    hashdag::VBRColor color;
    
    hashdag::EditType EditNode(const hashdag::Config<uint32_t>& config,
                              const hashdag::NodeCoord<uint32_t>& coord,
                              hashdag::NodePointer<uint32_t> node_ptr,
                              hashdag::VBRColor& out_color) const {
        // Calculate node bounds
        float node_size = coord.GetSize(config);
        glm::vec3 node_center = coord.GetWorldPosition(config);
        
        float distance = glm::length(node_center - center);
        
        if (distance + node_size * 0.5f < radius) {
            // Completely inside sphere
            out_color = fill_mode ? color : hashdag::VBRColor{};
            return fill_mode ? hashdag::EditType::kFill : hashdag::EditType::kClear;
        } else if (distance - node_size * 0.5f > radius) {
            // Completely outside sphere
            return hashdag::EditType::kNotAffected;
        } else {
            // Partially intersecting - need to recurse
            return hashdag::EditType::kProceed;
        }
    }
    
    bool EditVoxel(const hashdag::Config<uint32_t>& config,
                   const hashdag::NodeCoord<uint32_t>& coord,
                   bool current_voxel,
                   hashdag::VBRColor& out_color) const {
        glm::vec3 voxel_pos = coord.GetWorldPosition(config);
        float distance = glm::length(voxel_pos - center);
        
        if (distance <= radius) {
            out_color = color;
            return fill_mode;
        }
        
        return current_voxel;
    }
};

// Apply edit
SphereEditor editor{
    .center = {0.0f, 0.0f, 0.0f},
    .radius = 10.0f,
    .fill_mode = true,
    .color = hashdag::VBRColor{hashdag::RGB8Color{255, 0, 0}}
};

auto new_root = node_pool->Edit(editor, node_pool->GetRoot());
node_pool->SetRoot(new_root);
```

## Related Documentation

- [Architecture Overview](Architecture-Overview) - System design and component relationships
- [HashDAG Implementation](HashDAG-Implementation) - Core data structure details
- [Memory Management](Memory-Management) - Memory allocation and GPU integration
- [Rendering Pipeline](Rendering-Pipeline) - GPU rendering and shaders
- [Usage Guide](Usage-Guide) - Practical usage examples
