# Memory Management

VkHashDAG implements a sophisticated memory management system that efficiently handles both CPU and GPU memory for the HashDAG data structure and VBR color system. This document details the memory architecture and allocation strategies.

## Overview

The memory management system consists of three main components:

1. **DAGNodePool**: Manages HashDAG node storage with paged allocation
2. **DAGColorPool**: Handles VBR color data with octree organization  
3. **VkPagedBuffer**: Provides Vulkan sparse buffer management

## DAGNodePool Memory Management

### Paged Memory Architecture

The DAGNodePool uses a hierarchical paged memory system:

```
Memory Layout:
┌─────────────────────────────────────────────────────────┐
│                    Total Memory Space                   │
├─────────────────────────────────────────────────────────┤
│  Bucket 0   │  Bucket 1   │  Bucket 2   │  ...         │
├─────────────────────────────────────────────────────────┤
│ Page│Page │ Page│Page │ Page│Page │                     │
│  0  │ 1   │  2  │ 3   │  4  │ 5   │                     │
└─────────────────────────────────────────────────────────┘
```

### Memory Configuration

```cpp
struct MemoryConfig {
    uint32_t word_bits_per_page;      // Page size: 2^14 = 16KB typical
    uint32_t page_bits_per_bucket;    // Pages per bucket: 2^2 = 4 typical
    uint32_t total_buckets;           // Calculated from level configuration
    uint32_t total_pages;             // total_buckets * pages_per_bucket
};
```

### Page Allocation Strategy

#### On-Demand Allocation
```cpp
class DAGNodePool {
    std::unique_ptr<uint32_t[]> m_bucket_words;           // Bucket usage tracking
    std::unique_ptr<std::unique_ptr<uint32_t[]>[]> m_pages; // Page storage
    
    void WritePage(uint32_t page_id, uint32_t offset, 
                   std::span<const uint32_t> data) {
        if (!m_pages[page_id]) {
            // Allocate page on first write
            m_pages[page_id] = std::make_unique_for_overwrite<uint32_t[]>(
                GetConfig().GetWordsPerPage());
        }
        std::copy(data.begin(), data.end(), 
                  m_pages[page_id].get() + offset);
    }
};
```

#### Memory Tracking
```cpp
// Track dirty page ranges for GPU synchronization
phmap::parallel_flat_hash_map<uint32_t, Range<uint32_t>> m_page_write_ranges;

// Track freed pages for reuse
phmap::parallel_flat_hash_set<uint32_t> m_page_frees;
```

### Thread-Safe Memory Access

#### Bucket-Level Locking
```cpp
class DAGNodePool {
    std::array<std::mutex, 1024> m_edit_mutexes;
    
    std::mutex& GetBucketRefMutex(uint32_t bucket_id) {
        return m_edit_mutexes[bucket_id % m_edit_mutexes.size()];
    }
    
    // Atomic bucket word tracking
    uint32_t& GetBucketRefWords(uint32_t bucket_id) {
        return m_bucket_words[bucket_id];
    }
};
```

#### Lock-Free Read Operations
```cpp
template<bool ThreadSafe>
NodePointer upsert_node(auto&& get_node_words, Word level,
                       std::span<const Word> node_span) {
    if constexpr (ThreadSafe) {
        std::atomic_ref<Word> atomic_bucket_words{ref_bucket_words};
        
        // Lock-free read attempt
        Word shared_bucket_words = atomic_bucket_words.load(
            std::memory_order_acquire);
        
        NodePointer existing = find_node(bucket_index, shared_bucket_words, 
                                        0, node_span);
        if (existing) return existing;
        
        // Fall back to locked write
        std::unique_lock lock{get_bucket_ref_mutex(bucket_index)};
        // ... locked allocation logic
    }
}
```

## DAGColorPool Memory Management

### VBR Color System

The DAGColorPool manages Variable Bit Rate (VBR) color data using a sophisticated compression system:

```cpp
class DAGColorPool {
    SafePagedVector<Node> m_nodes;        // Color octree nodes
    SafePagedVector<uint32_t> m_leaves;   // Compressed color data
    
    struct Pointer {
        enum class Tag { kNode, kColor, kLeaf, kNull };
        uint32_t pointer;  // Tag + data packed into 32 bits
    };
};
```

### Color Data Layout

#### VBR Chunk Structure
```cpp
struct VBRChunk {
    std::vector<VBRMacroBlock> macro_blocks;    // High-level structure
    std::vector<VBRBlockHeader> block_headers;  // Block metadata
    VBRBitset weight_bits;                      // Compressed weights
};
```

#### Memory Layout in Leaves
```
Leaf Memory Layout:
┌─────────────────────────────────────────────────────────┐
│ Block Size │ Macro Count │ Header Count │ Bit Count    │
├─────────────────────────────────────────────────────────┤
│ MacroBlock │ MacroBlock  │ ...          │              │
│ Data       │ Data        │              │              │
├─────────────────────────────────────────────────────────┤
│ BlockHeader│ BlockHeader │ ...          │              │
│ Data       │ Data        │              │              │
├─────────────────────────────────────────────────────────┤
│ Weight Bits│ Weight Bits │ ...          │              │
│ Data       │ Data        │              │              │
└─────────────────────────────────────────────────────────┘
```

### Dynamic Memory Management

#### Adaptive Leaf Allocation
```cpp
Pointer SetLeaf(Pointer ptr, VBRChunk&& chunk) {
    size_t data_size = calculate_chunk_size(chunk);
    size_t append_size = (data_size & 1) ? data_size + 1 : data_size;
    
    // Try to reuse existing space
    if (!m_config.keep_history && ptr.GetTag() == Pointer::Tag::kLeaf) {
        size_t existing_size = m_leaves.Read(ptr.GetData(), std::identity{});
        if (data_size <= existing_size) {
            // Reuse existing allocation
            write_leaf_chunk(ptr.GetData() + 1, chunk);
            return ptr;
        }
        // Double the space for growth
        append_size = std::max(existing_size << 1, append_size);
    }
    
    // Allocate new space
    auto opt_idx = m_leaves.Append(append_size, [](auto&&...) {});
    if (!opt_idx) return ptr;  // Out of memory
    
    write_leaf_chunk(*opt_idx + 1, chunk);
    return Pointer{Pointer::Tag::kLeaf, (uint32_t)*opt_idx};
}
```

### Color Compression

#### VBR Color Encoding
```cpp
class VBRColor {
    uint32_t data;  // Packed color + weight information
    
    // Support multiple color formats
    VBRColor(RGB8Color color);
    VBRColor(R5G6B5Color color);
    VBRColor(RGB8Color color1, RGB8Color color2, uint32_t weight1, uint32_t weight2);
};
```

#### Compression Statistics
- **Typical Compression**: 4:1 to 8:1 for color data
- **Memory Overhead**: ~10% for metadata
- **Access Performance**: O(1) color lookup with decompression

## VkPagedBuffer - GPU Memory Management

### Sparse Buffer Architecture

VkPagedBuffer implements Vulkan sparse buffer functionality:

```cpp
class VkPagedBuffer {
    VkBuffer m_buffer;                    // Sparse Vulkan buffer
    VkDeviceMemory m_memory;              // Backing memory
    std::vector<VkSparseMemoryBind> m_binds; // Active memory bindings
    
    uint32_t m_page_size;                 // Page size in bytes
    uint32_t m_page_total;                // Total available pages
};
```

### Memory Binding Strategy

#### On-Demand GPU Allocation
```cpp
void VkSparseBinder::QueueBind(const std::vector<VkSparseMemoryBind>& binds) {
    VkSparseBufferMemoryBindInfo buffer_bind_info{
        .sType = VK_STRUCTURE_TYPE_SPARSE_BUFFER_MEMORY_BIND_INFO,
        .buffer = buffer,
        .bindCount = static_cast<uint32_t>(binds.size()),
        .pBinds = binds.data()
    };
    
    VkBindSparseInfo bind_info{
        .sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
        .bufferBindCount = 1,
        .pBufferBinds = &buffer_bind_info
    };
    
    vkQueueBindSparse(sparse_queue, 1, &bind_info, fence);
}
```

#### Memory Synchronization
```cpp
void DAGNodePool::Flush(const myvk::Ptr<VkSparseBinder>& binder) {
    // Collect all dirty page ranges
    std::vector<VkSparseMemoryBind> binds;
    
    for (auto& [page_id, range] : m_page_write_ranges) {
        if (m_pages[page_id]) {
            VkSparseMemoryBind bind{
                .resourceOffset = page_id * m_buffer->GetPageSize(),
                .size = range.Size() * sizeof(uint32_t),
                .memory = m_buffer->GetMemory(),
                .memoryOffset = calculate_memory_offset(page_id, range.begin)
            };
            binds.push_back(bind);
        }
    }
    
    // Handle page frees
    for (uint32_t page_id : m_page_frees) {
        VkSparseMemoryBind bind{
            .resourceOffset = page_id * m_buffer->GetPageSize(),
            .size = m_buffer->GetPageSize(),
            .memory = VK_NULL_HANDLE  // Unbind memory
        };
        binds.push_back(bind);
    }
    
    binder->QueueBind(binds);
    m_page_write_ranges.clear();
    m_page_frees.clear();
}
```

## Memory Performance Optimization

### Cache-Friendly Access Patterns

#### Sequential Page Access
```cpp
template<typename Func>
void PagedVector::ForeachPage(size_t start_idx, size_t count, Func func) {
    size_t current_idx = start_idx;
    
    while (current_idx < start_idx + count) {
        uint32_t page_id = current_idx >> m_page_bits;
        uint32_t page_offset = current_idx & m_page_mask;
        uint32_t inpage_count = std::min(
            m_page_size - page_offset,
            static_cast<uint32_t>(start_idx + count - current_idx)
        );
        
        func(current_idx, page_id, page_offset, inpage_count);
        current_idx += inpage_count;
    }
}
```

#### Memory Prefetching
```cpp
const uint32_t* ReadPage(uint32_t page_id) const {
    const uint32_t* page_data = m_pages[page_id].get();
    
    // Prefetch next likely pages
    if (page_id + 1 < m_pages.size() && m_pages[page_id + 1]) {
        __builtin_prefetch(m_pages[page_id + 1].get(), 0, 3);
    }
    
    return page_data;
}
```

### Memory Usage Statistics

#### Runtime Monitoring
```cpp
struct MemoryStats {
    size_t total_allocated_pages;
    size_t total_used_pages;
    size_t total_memory_bytes;
    size_t compression_ratio;
    
    double fragmentation_ratio() const {
        return 1.0 - (double)total_used_pages / total_allocated_pages;
    }
};
```

#### Typical Memory Usage
- **HashDAG Nodes**: 10-100 MB for large scenes
- **VBR Colors**: 5-50 MB depending on color complexity
- **GPU Buffers**: Matches CPU allocation with sparse binding
- **Compression Ratio**: 10:1 to 100:1 vs. dense voxel arrays

## Garbage Collection

### Reference Tracking
```cpp
template<typename HashMap, typename HashSet>
class NodePoolThreadedGC {
    HashMap<NodePointer, uint32_t> reference_counts;
    HashSet<NodePointer> reachable_nodes;
    
    void mark_reachable_nodes(NodePointer root);
    void sweep_unreachable_nodes();
    NodePointer compact_memory(NodePointer root);
};
```

### Memory Compaction
```cpp
NodePointer ThreadedGC(ThreadPool* pool, NodePointer root) {
    // Phase 1: Mark all reachable nodes
    std::atomic<size_t> marked_count{0};
    mark_phase_parallel(pool, root, marked_count);
    
    // Phase 2: Sweep unreachable nodes
    std::atomic<size_t> freed_count{0};
    sweep_phase_parallel(pool, freed_count);
    
    // Phase 3: Compact and rebuild tree
    return compact_phase_parallel(pool, root);
}
```

## Error Handling and Recovery

### Out-of-Memory Handling
```cpp
std::optional<size_t> SafePagedVector::Append(size_t count, auto&& initializer) {
    if (m_used_pages >= m_total_pages) {
        return std::nullopt;  // Out of pages
    }
    
    size_t required_pages = (count + m_page_size - 1) / m_page_size;
    if (m_used_pages + required_pages > m_total_pages) {
        return std::nullopt;  // Not enough contiguous space
    }
    
    // Proceed with allocation
    return allocate_pages(required_pages, initializer);
}
```

### Memory Leak Detection
```cpp
#ifdef DEBUG
class MemoryTracker {
    std::unordered_map<void*, size_t> allocations;
    std::mutex allocation_mutex;
    
public:
    void track_allocation(void* ptr, size_t size);
    void track_deallocation(void* ptr);
    void report_leaks();
};
#endif
```

## Configuration Guidelines

### Memory-Constrained Systems
```cpp
// Optimize for low memory usage
DAGNodePool::Config low_memory{
    .word_bits_per_page = 12,      // 4KB pages
    .page_bits_per_bucket = 1,     // 2 pages per bucket
    .bucket_bits_per_top_level = 6,
    .bucket_bits_per_bottom_level = 8
};
```

### High-Performance Systems
```cpp
// Optimize for performance
DAGNodePool::Config high_performance{
    .word_bits_per_page = 16,      // 64KB pages
    .page_bits_per_bucket = 3,     // 8 pages per bucket
    .bucket_bits_per_top_level = 10,
    .bucket_bits_per_bottom_level = 14
};
```

## Related Documentation

- [HashDAG Implementation](HashDAG-Implementation) - Core data structure details
- [Architecture Overview](Architecture-Overview) - System-wide design
- [Rendering Pipeline](Rendering-Pipeline) - GPU resource management
- [API Reference](API-Reference) - Memory management APIs
