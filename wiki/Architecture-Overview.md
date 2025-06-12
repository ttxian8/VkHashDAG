# Architecture Overview

VkHashDAG implements a sophisticated memory-efficient voxel rendering system using HashDAG data structures and Vulkan GPU acceleration. This document provides a high-level overview of the system architecture and component relationships.

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│  Camera Control  │  Input Handling  │  ImGui Interface     │
├─────────────────────────────────────────────────────────────┤
│                    Editing System                           │
├─────────────────────────────────────────────────────────────┤
│  Sphere Editor   │  AABB Editor     │  VBR Color Editor    │
├─────────────────────────────────────────────────────────────┤
│                   Memory Management                         │
├─────────────────────────────────────────────────────────────┤
│   DAGNodePool    │  DAGColorPool    │  VkPagedBuffer       │
├─────────────────────────────────────────────────────────────┤
│                   HashDAG Core                              │
├─────────────────────────────────────────────────────────────┤
│  Node Storage    │  Hash Dedup      │  Threaded Ops       │
├─────────────────────────────────────────────────────────────┤
│                  Rendering Pipeline                         │
├─────────────────────────────────────────────────────────────┤
│   TracePass      │   BeamPass       │  CrosshairPass       │
├─────────────────────────────────────────────────────────────┤
│                    Vulkan Layer                             │
├─────────────────────────────────────────────────────────────┤
│  Command Buffers │  Memory Binding  │  Shader Pipeline     │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. HashDAG Data Structure

The HashDAG (Hash-based Directed Acyclic Graph) is the foundation of the system:

- **Sparse Representation**: Only stores non-empty voxel regions
- **Hash-based Deduplication**: Identical subtrees share the same memory
- **Hierarchical Structure**: Multi-level tree with configurable depth
- **Memory Efficiency**: Dramatically reduces memory usage for sparse data

#### Key Classes:
- `hashdag::NodePool`: Abstract base for node storage
- `hashdag::Config`: Configuration parameters for tree structure
- `hashdag::NodeCoord`: Spatial coordinates within the tree

### 2. Memory Management System

#### DAGNodePool
- **Purpose**: Manages HashDAG node storage and allocation
- **Features**: 
  - Paged memory allocation
  - Thread-safe operations with bucket-level locking
  - Hash-based node deduplication
  - Garbage collection support
- **Vulkan Integration**: Uses sparse buffer binding for GPU memory

#### DAGColorPool  
- **Purpose**: Manages VBR (Variable Bit Rate) color data
- **Features**:
  - Compressed color storage
  - Octree-based color organization
  - Efficient color blending and editing
  - GPU-accessible color buffers

#### VkPagedBuffer
- **Purpose**: Vulkan sparse buffer management
- **Features**:
  - On-demand page allocation
  - Sparse memory binding
  - Efficient GPU memory usage

### 3. Rendering Pipeline

#### Render Graph Architecture
The system uses a modern render graph approach with three main passes:

1. **TracePass**: Primary ray tracing through the HashDAG
2. **BeamPass**: Beam optimization for performance
3. **CrosshairPass**: UI overlay rendering

#### GPU Resource Management
- **Descriptor Sets**: Efficient resource binding
- **Command Buffer Management**: Frame-based command recording
- **Memory Barriers**: Proper synchronization between passes

### 4. Editing System

#### Editor Interfaces
- **StatelessEditor**: Simple voxel modification operations
- **VBREditor**: Color-aware editing with VBR compression
- **SphereEditor**: Spherical editing tools (fill, dig, paint)
- **AABBEditor**: Axis-aligned bounding box editing

#### Threading Model
- **Main Thread**: UI and command submission
- **Edit Thread Pool**: Asynchronous editing operations
- **Worker Pool**: Parallel HashDAG operations using libfork

## Data Flow

### 1. Voxel Editing Flow
```
User Input → Editor → DAGNodePool → Hash Deduplication → GPU Upload
                 ↓
            DAGColorPool → VBR Compression → Color Buffer Update
```

### 2. Rendering Flow
```
Camera Update → Uniform Buffers → TracePass → BeamPass → CrosshairPass → Present
                                      ↓
                              HashDAG Traversal → Color Lookup → Fragment Output
```

### 3. Memory Management Flow
```
Edit Operation → Page Allocation → Sparse Binding → GPU Memory → Garbage Collection
```

## Performance Characteristics

### Memory Efficiency
- **Deduplication Ratio**: Typically 10:1 to 100:1 compression for structured data
- **Sparse Allocation**: Only allocates memory for non-empty regions
- **VBR Compression**: Additional color compression reduces memory usage

### Threading Performance
- **Lock-free Operations**: Most read operations are lock-free
- **Bucket-level Locking**: Fine-grained locking for write operations
- **Parallel Algorithms**: Multi-threaded editing and garbage collection

### GPU Performance
- **Sparse Memory**: Efficient GPU memory usage
- **Coherent Access**: Optimized memory access patterns
- **Beam Optimization**: Reduces ray tracing overhead

## Configuration System

### HashDAG Configuration
```cpp
hashdag::DefaultConfig<uint32_t> config{
    .level_count = 17,              // Tree depth
    .top_level_count = 9,           // High-resolution levels
    .word_bits_per_page = 14,       // Page size (16KB)
    .page_bits_per_bucket = 2,      // Pages per bucket
    .bucket_bits_per_top_level = 7, // Top-level bucket count
    .bucket_bits_per_bottom_level = 11 // Bottom-level bucket count
};
```

### Color Pool Configuration
```cpp
DAGColorPool::Config color_config{
    .leaf_level = 10,               // Color tree depth
    .node_bits_per_node_page = 18,  // Node page size
    .word_bits_per_leaf_page = 24,  // Leaf page size
    .keep_history = false           // History management
};
```

## Synchronization and Threading

### Thread Safety
- **Reader-Writer Locks**: Protect shared data structures
- **Atomic Operations**: Lock-free reference counting
- **Memory Ordering**: Careful memory ordering for performance

### GPU Synchronization
- **Fences**: CPU-GPU synchronization
- **Semaphores**: GPU-GPU synchronization
- **Memory Barriers**: Proper memory visibility

## Error Handling and Validation

### Vulkan Validation
- **Debug Layers**: Comprehensive validation during development
- **Error Checking**: All Vulkan calls are checked for errors
- **Resource Tracking**: Automatic resource lifetime management

### Memory Safety
- **RAII**: Automatic resource management
- **Smart Pointers**: Prevent memory leaks
- **Bounds Checking**: Debug-mode bounds checking

## Extensibility

### Plugin Architecture
The system is designed for extensibility:
- **Editor Interface**: Custom editing tools
- **Render Passes**: Additional rendering effects
- **Memory Allocators**: Custom allocation strategies

### Future Enhancements
- **Multi-GPU Support**: Distribute workload across GPUs
- **Network Synchronization**: Multi-user editing
- **Advanced Compression**: Better compression algorithms

## Related Documentation

- [HashDAG Implementation](HashDAG-Implementation) - Detailed data structure information
- [Memory Management](Memory-Management) - In-depth memory system analysis
- [Rendering Pipeline](Rendering-Pipeline) - Vulkan rendering details
- [API Reference](API-Reference) - Complete API documentation
