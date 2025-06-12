---
layout: default
title: HashDAG Implementation
nav_order: 5
description: "Core data structures and algorithms for HashDAG sparse voxel representation"
---

# HashDAG Implementation
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

The HashDAG (Hash-based Directed Acyclic Graph) is the core data structure that enables memory-efficient sparse voxel representation. This document details the implementation of the HashDAG system in VkHashDAG.

## Overview

HashDAG is a tree-based data structure that represents sparse 3D voxel data with automatic deduplication of identical subtrees. This approach can achieve compression ratios of 10:1 to 100:1 for typical voxel datasets.

## Core Concepts

### 1. Hierarchical Structure

The HashDAG organizes voxels in a hierarchical tree structure:

```
Level 0 (Root)     [Single node covering entire space]
Level 1            [8 children, each covering 1/8 of space]
Level 2            [64 possible children]
...
Level N-1          [Leaf nodes containing actual voxel data]
Level N            [Individual voxels]
```

### 2. Hash-based Deduplication

Identical subtrees are automatically deduplicated using hash-based lookup:
- Each node is hashed based on its content
- Identical nodes share the same memory location
- Dramatically reduces memory usage for repetitive structures

### 3. Sparse Representation

Only non-empty regions are stored:
- Empty subtrees are represented by null pointers
- Filled subtrees can be represented by special "filled" pointers
- Partial subtrees are stored explicitly

## Data Structures

### Node Representation

#### Inner Nodes
```cpp
struct InnerNode {
    uint8_t child_mask;        // Bitmask of non-null children
    NodePointer children[8];   // Pointers to child nodes (variable count)
};
```

#### Leaf Nodes
```cpp
struct LeafNode {
    uint64_t voxel_data[2];    // 64 voxels packed into 128 bits
};
```

### Node Pointers

```cpp
template<typename Word>
struct NodePointer {
    Word pointer;
    
    static NodePointer Null();
    static NodePointer Filled();
    bool IsNull() const;
    bool IsFilled() const;
    Word GetAddress() const;
};
```

### Configuration System

```cpp
template<typename Word>
struct Config {
    Word word_bits_per_page;           // Page size in bits
    Word page_bits_per_bucket;         // Pages per bucket
    std::vector<Word> bucket_bits_each_level;  // Buckets per level
    
    Word GetResolution() const;        // Total voxel resolution
    Word GetNodeLevels() const;        // Number of tree levels
    Word GetTotalBuckets() const;      // Total hash buckets
};
```

## Memory Organization

### Bucket-based Storage

The HashDAG uses a bucket-based storage system:

```
Bucket 0: [Page 0][Page 1][Page 2][Page 3]
Bucket 1: [Page 4][Page 5][Page 6][Page 7]
...
```

Each bucket contains multiple pages, and nodes are allocated within pages.

### Hash Distribution

Nodes are distributed across buckets using hash functions:

```cpp
Word bucket_index = hash(node_content) % buckets_at_level;
Word global_bucket = level_base_bucket + bucket_index;
```

### Page Allocation

Within each bucket, nodes are allocated sequentially:

```cpp
struct BucketState {
    Word used_words;           // Words allocated in this bucket
    std::mutex mutex;          // Thread synchronization
};
```

## Core Algorithms

### 1. Node Lookup

Finding existing nodes uses hash-based lookup:

```cpp
template<typename NodeSpan>
NodePointer find_node(Word bucket_index, NodeSpan node_span) {
    // Calculate bucket and page locations
    Word page_start = bucket_index * pages_per_bucket;
    
    // Search through allocated pages in bucket
    for (Word page_offset = 0; page_offset < bucket_words; ) {
        Word page_id = page_start + (page_offset / words_per_page);
        const Word* page_data = read_page(page_id);
        
        // Search within page for matching node
        NodePointer result = find_node_in_page(page_data, node_span);
        if (result) return result;
        
        page_offset += words_per_page;
    }
    return NodePointer::Null();
}
```

### 2. Node Insertion

Adding new nodes with deduplication:

```cpp
template<typename NodeSpan>
NodePointer upsert_node(Word level, NodeSpan node_span) {
    Word bucket_index = calculate_bucket(level, node_span);
    
    // First, try to find existing node
    NodePointer existing = find_node(bucket_index, node_span);
    if (existing) return existing;
    
    // If not found, allocate new node
    std::lock_guard lock(bucket_mutex[bucket_index]);
    
    // Double-check after acquiring lock
    existing = find_node(bucket_index, node_span);
    if (existing) return existing;
    
    // Allocate new node
    return append_node(bucket_index, node_span);
}
```

### 3. Tree Traversal

Efficient traversal for ray tracing:

```cpp
template<typename Visitor>
void traverse_tree(NodePointer root, const Ray& ray, Visitor visitor) {
    struct StackEntry {
        NodePointer node;
        BoundingBox bounds;
        Word level;
    };
    
    std::stack<StackEntry> stack;
    stack.push({root, world_bounds, 0});
    
    while (!stack.empty()) {
        auto [node, bounds, level] = stack.top();
        stack.pop();
        
        if (!ray.intersects(bounds)) continue;
        
        if (level == max_level) {
            // Process leaf node
            visitor.visit_leaf(node, bounds);
        } else {
            // Process inner node - add children to stack
            auto children = get_node_children(node);
            for (Word i = 0; i < 8; ++i) {
                if (children[i]) {
                    BoundingBox child_bounds = bounds.get_child(i);
                    stack.push({children[i], child_bounds, level + 1});
                }
            }
        }
    }
}
```

## Thread Safety

### Bucket-level Locking

The system uses fine-grained locking at the bucket level:

```cpp
class ThreadSafeNodePool {
    std::array<std::mutex, 1024> bucket_mutexes;
    
    std::mutex& get_bucket_mutex(Word bucket_id) {
        return bucket_mutexes[bucket_id % bucket_mutexes.size()];
    }
};
```

### Lock-free Reads

Most read operations are lock-free:

```cpp
NodePointer read_node_atomic(Word address) {
    std::atomic_ref<Word> atomic_ref(bucket_words[bucket_id]);
    Word bucket_size = atomic_ref.load(std::memory_order_acquire);
    
    // Safe to read within this range without locks
    if (address < bucket_size) {
        return read_node_unsafe(address);
    }
    return NodePointer::Null();
}
```

## Editing Operations

### 1. Voxel Modification

Editing voxels requires tree reconstruction:

```cpp
template<typename Editor>
NodePointer edit_node(NodePointer node, const NodeCoord& coord, 
                     const Editor& editor) {
    if (coord.is_leaf_level()) {
        return edit_leaf(node, coord, editor);
    } else {
        return edit_inner_node(node, coord, editor);
    }
}
```

### 2. Leaf Editing

Modifying individual voxels:

```cpp
NodePointer edit_leaf(NodePointer leaf, const NodeCoord& coord,
                     const Editor& editor) {
    auto leaf_data = get_leaf_array(leaf);
    bool changed = false;
    
    // Edit individual voxels within leaf
    for (Word i = 0; i < 64; ++i) {
        NodeCoord voxel_coord = coord.get_voxel_coord(i);
        bool old_voxel = get_voxel_bit(leaf_data, i);
        bool new_voxel = editor.edit_voxel(voxel_coord, old_voxel);
        
        if (old_voxel != new_voxel) {
            set_voxel_bit(leaf_data, i, new_voxel);
            changed = true;
        }
    }
    
    if (!changed) return leaf;
    
    // Create new leaf with modified data
    return upsert_leaf(coord.level, leaf_data);
}
```

### 3. Inner Node Editing

Modifying tree structure:

```cpp
NodePointer edit_inner_node(NodePointer node, const NodeCoord& coord,
                           const Editor& editor) {
    auto children = get_unpacked_node_array(node);
    bool changed = false;
    
    // Edit each child recursively
    for (Word i = 0; i < 8; ++i) {
        NodeCoord child_coord = coord.get_child_coord(i);
        EditType edit_type = editor.edit_node(child_coord, children[i+1]);
        
        NodePointer new_child;
        switch (edit_type) {
            case EditType::kNotAffected:
                new_child = children[i+1];
                break;
            case EditType::kClear:
                new_child = NodePointer::Null();
                changed = true;
                break;
            case EditType::kFill:
                new_child = get_filled_node_pointer(child_coord.level + 1);
                changed = true;
                break;
            case EditType::kProceed:
                new_child = edit_node(children[i+1], child_coord, editor);
                changed |= (new_child != children[i+1]);
                break;
        }
        children[i+1] = new_child;
    }
    
    if (!changed) return node;
    
    // Pack and create new node
    auto packed_node = get_packed_node_inplace(children);
    return upsert_inner_node(coord.level, packed_node);
}
```

## Garbage Collection

### Reference Counting

The system tracks node references for garbage collection:

```cpp
template<typename HashMap, typename HashSet>
class GarbageCollector {
    HashMap<NodePointer, Word> reference_counts;
    HashSet<NodePointer> reachable_nodes;
    
    void mark_reachable(NodePointer root);
    void sweep_unreachable();
};
```

### Threaded GC

Garbage collection runs in parallel:

```cpp
NodePointer threaded_gc(ThreadPool* pool, NodePointer root) {
    // Phase 1: Mark all reachable nodes
    mark_phase(pool, root);
    
    // Phase 2: Sweep unreachable nodes
    sweep_phase(pool);
    
    // Phase 3: Compact memory
    return compact_phase(pool, root);
}
```

## Performance Optimizations

### 1. Memory Layout

Nodes are packed efficiently:
- Child masks use single bytes
- Pointers are compressed when possible
- Cache-friendly memory access patterns

### 2. Hash Functions

Fast hash functions for node content:

```cpp
struct MurmurHasher32 {
    uint32_t operator()(std::span<const uint32_t> data) const {
        return murmur_hash_32(data.data(), data.size() * 4, 0x12345678);
    }
};
```

### 3. SIMD Operations

Vectorized operations where possible:
- Parallel voxel bit manipulation
- Fast memory copying
- Vectorized hash computation

## Configuration Examples

### High-Resolution Configuration
```cpp
hashdag::DefaultConfig<uint32_t> high_res{
    .level_count = 20,              // 1M³ resolution
    .top_level_count = 12,
    .word_bits_per_page = 16,       // 64KB pages
    .page_bits_per_bucket = 3,      // 8 pages per bucket
    .bucket_bits_per_top_level = 8,
    .bucket_bits_per_bottom_level = 12
};
```

### Memory-Optimized Configuration
```cpp
hashdag::DefaultConfig<uint32_t> memory_opt{
    .level_count = 17,              // 128K³ resolution
    .top_level_count = 9,
    .word_bits_per_page = 12,       // 4KB pages
    .page_bits_per_bucket = 2,      // 4 pages per bucket
    .bucket_bits_per_top_level = 6,
    .bucket_bits_per_bottom_level = 10
};
```

## Related Documentation

- [Memory Management](Memory-Management) - Memory allocation and GPU integration
- [Architecture Overview](Architecture-Overview) - System-wide design
- [API Reference](API-Reference) - Complete API documentation
- [Usage Guide](Usage-Guide) - Practical usage examples
