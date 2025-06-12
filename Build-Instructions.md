# Build Instructions

This guide provides complete instructions for building VkHashDAG on different platforms.

## Prerequisites

### Required Dependencies

- **CMake 3.15+**
- **C++20 compatible compiler** (GCC 10+, Clang 12+, MSVC 2019+)
- **Vulkan SDK** (latest version recommended)
- **Git** (for submodule management)

### Platform-Specific Dependencies

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install ninja-build xorg-dev gcc g++ cmake git
```

#### Windows
- **Visual Studio 2019+** with C++ development tools
- **MinGW-w64** (alternative to Visual Studio)
- **Ninja** build system (recommended)

### Vulkan SDK Installation

1. Download from [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
2. Install following platform-specific instructions
3. Verify installation:
   ```bash
   vulkaninfo
   ```

## Building from Source

### 1. Clone Repository

```bash
git clone https://github.com/ttxian8/VkHashDAG.git
cd VkHashDAG
git submodule update --init --recursive
```

### 2. Configure Build

#### Linux
```bash
mkdir build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja
```

#### Windows (Visual Studio)
```bash
mkdir build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -G "Visual Studio 16 2019" \
    -A x64
```

#### Windows (MinGW)
```bash
mkdir build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -G Ninja
```

### 3. Build Project

```bash
cmake --build build --target VkHashDAG --config Release
```

### 4. Install (Optional)

```bash
cmake --install build --strip
```

## Build Configuration Options

### CMake Variables

- `CMAKE_BUILD_TYPE`: `Debug`, `Release`, `RelWithDebInfo`
- `CMAKE_CXX_STANDARD`: Set to 20 (required)
- `CMAKE_INTERPROCEDURAL_OPTIMIZATION`: Enable LTO for Release builds

### Compiler-Specific Optimizations

#### GCC/Clang Release Flags
```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native"
```

#### MSVC Release Configuration
The project automatically configures optimal MSVC settings including:
- `/SUBSYSTEM:WINDOWS` for release builds
- Interprocedural optimization when supported

## Dependencies Overview

### Core Dependencies (Included as Submodules)

- **MyVK**: Vulkan abstraction library
- **GLM**: Mathematics library for graphics
- **libfork**: Lock-free parallel computing
- **parallel-hashmap**: High-performance hash maps
- **ThreadPool**: Thread pool implementation
- **GLFW**: Window and input management
- **ImGui**: Immediate mode GUI

### System Dependencies

- **Vulkan SDK**: Graphics API
- **X11 libraries** (Linux only)
- **Windows SDK** (Windows only)

## Troubleshooting

### Common Build Issues

#### Vulkan SDK Not Found
```
Error: Could not find Vulkan SDK
```
**Solution**: Ensure Vulkan SDK is installed and `VULKAN_SDK` environment variable is set.

#### C++20 Compiler Issues
```
Error: C++20 features not supported
```
**Solution**: Update to a compatible compiler version:
- GCC 10+
- Clang 12+
- MSVC 2019 16.8+

#### Submodule Issues
```
Error: Missing dependencies in dep/ folder
```
**Solution**: Initialize submodules:
```bash
git submodule update --init --recursive
```

#### Memory Issues During Build
**Solution**: Use fewer parallel jobs:
```bash
cmake --build build --parallel 2
```

### Platform-Specific Issues

#### Linux: X11 Development Headers Missing
```bash
sudo apt-get install xorg-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

#### Windows: MinGW Path Issues
Ensure MinGW-w64 is in your PATH and use the correct architecture (32-bit vs 64-bit).

#### macOS: Not Officially Supported
While the code may compile on macOS with MoltenVK, it's not officially tested or supported.

## Performance Optimization

### Release Build Recommendations

1. **Use Release build type**:
   ```bash
   -DCMAKE_BUILD_TYPE=Release
   ```

2. **Enable native optimizations**:
   ```bash
   -DCMAKE_CXX_FLAGS="-march=native -mtune=native"
   ```

3. **Use Ninja generator** for faster builds:
   ```bash
   -G Ninja
   ```

### Runtime Performance

- Ensure GPU drivers are up to date
- Use discrete GPU if available
- Close unnecessary applications to free GPU memory

## Verification

After successful build, verify the installation:

1. **Check executable**:
   ```bash
   ./build/VkHashDAG --help
   ```

2. **Test Vulkan functionality**:
   - Run the application
   - Check for Vulkan validation layer messages
   - Verify rendering performance

3. **Memory usage**:
   - Monitor GPU memory usage during operation
   - Check for memory leaks in debug builds

## Next Steps

- See [Usage Guide](Usage-Guide) for application controls
- Read [Architecture Overview](Architecture-Overview) for implementation details
- Check [API Reference](API-Reference) for development information
