---
layout: default
title: Home
nav_order: 1
description: "VkHashDAG - High-performance Vulkan implementation of HashDAG for sparse voxel rendering"
permalink: /
---

# VkHashDAG Documentation
{: .fs-9 }

A high-performance Vulkan implementation of HashDAG for sparse voxel rendering with real-time editing capabilities.
{: .fs-6 .fw-300 }

[Get started now](#getting-started){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/ttxian8/VkHashDAG){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## Getting started

VkHashDAG is a sophisticated implementation of the HashDAG data structure using Vulkan for GPU-accelerated sparse voxel rendering. This documentation covers everything from basic setup to advanced development topics.

### Key Features

- **HashDAG Data Structure**: Memory-efficient sparse voxel representation with hash-based node deduplication
- **VBR Color System**: Variable Bit Rate color compression for efficient color storage
- **Real-time Editing**: Interactive voxel editing with sphere and AABB tools
- **Vulkan Rendering**: GPU-accelerated ray tracing with beam optimization
- **Multi-threaded Operations**: Parallel editing and garbage collection
- **Sparse Memory Management**: Efficient GPU memory allocation with paged buffers

### Quick Start

1. **Prerequisites**: Vulkan SDK, CMake 3.15+, C++20 compiler
2. **Build**: Follow the [Build Instructions](wiki/Build-Instructions) for detailed setup
3. **Usage**: See [Usage Guide](wiki/Usage-Guide) for controls and editing modes

---

## About the project

VkHashDAG is sponsored by [Aidan-Sanders](https://github.com/Aidan-Sanders) and based on the original HashDAG research. The project provides a complete implementation of sparse voxel data structures with modern GPU acceleration.

### License

VkHashDAG is distributed by an [MIT license](https://github.com/ttxian8/VkHashDAG/tree/main/LICENSE).

### Contributing

When contributing to this repository, please first discuss the change you wish to make via issue, email, or any other method with the owners of this repository before making a change.
