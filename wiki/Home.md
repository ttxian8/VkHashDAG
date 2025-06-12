---
layout: default
title: Project Overview
nav_order: 2
description: "VkHashDAG project overview, features, and quick start guide"
---

# VkHashDAG Project Overview
{: .no_toc }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## About VkHashDAG

[![Linux GCC](https://github.com/ttxian8/VkHashDAG/actions/workflows/linux.yml/badge.svg)](https://github.com/ttxian8/VkHashDAG/actions/workflows/linux.yml)
[![Windows MSVC](https://github.com/ttxian8/VkHashDAG/actions/workflows/windows-msvc.yml/badge.svg)](https://github.com/ttxian8/VkHashDAG/actions/workflows/windows-msvc.yml)
[![Windows MinGW](https://github.com/ttxian8/VkHashDAG/actions/workflows/windows-mingw.yml/badge.svg)](https://github.com/ttxian8/VkHashDAG/actions/workflows/windows-mingw.yml)

VkHashDAG is a high-performance Vulkan implementation of [HashDAG](https://github.com/Phyronnaz/HashDAG), a memory-efficient data structure for representing sparse voxel data. This project provides real-time voxel editing capabilities with sophisticated memory management and GPU-accelerated rendering.

## Key Features

- **HashDAG Data Structure**: Memory-efficient sparse voxel representation with hash-based node deduplication
- **VBR Color System**: Variable Bit Rate color compression for efficient color storage
- **Real-time Editing**: Interactive voxel editing with sphere and AABB tools
- **Vulkan Rendering**: GPU-accelerated ray tracing with beam optimization
- **Threaded Operations**: Multi-threaded editing and garbage collection
- **Sparse Memory Management**: Efficient GPU memory allocation with paged buffers

## Screenshots

![VkHashDAG Screenshot](https://raw.githubusercontent.com/AdamYuan/VkHashDAG/master/screenshot/0.png)

## Quick Start

1. **Prerequisites**: Vulkan SDK, CMake 3.15+, C++20 compiler
2. **Build**: See [Build Instructions](Build-Instructions) for detailed setup
3. **Usage**: See [Usage Guide](Usage-Guide) for controls and editing modes

## Documentation

- **[Build Instructions](Build-Instructions)** - Complete build and installation guide
- **[Architecture Overview](Architecture-Overview)** - High-level system design
- **[HashDAG Implementation](HashDAG-Implementation)** - Core data structures and algorithms
- **[Memory Management](Memory-Management)** - DAGNodePool and DAGColorPool systems
- **[Rendering Pipeline](Rendering-Pipeline)** - Vulkan rendering and shaders
- **[API Reference](API-Reference)** - Key classes and interfaces
- **[Usage Guide](Usage-Guide)** - Application controls and workflows

## Project Structure

```
VkHashDAG/
├── src/                    # Core implementation
│   ├── main.cpp           # Application entry point
│   ├── DAGNodePool.*      # HashDAG node management
│   ├── DAGColorPool.*     # VBR color system
│   └── rg/                # Render graph passes
├── include/hashdag/       # HashDAG library headers
├── shader/src/            # GLSL shaders
├── dep/                   # Dependencies
└── CMakeLists.txt         # Build configuration
```

## Contributing

This project is sponsored by [Aidan-Sanders](https://github.com/Aidan-Sanders) and based on the original HashDAG research. For technical details, see the [Architecture Overview](Architecture-Overview) and [API Reference](API-Reference).

## License

See the repository for license information.
