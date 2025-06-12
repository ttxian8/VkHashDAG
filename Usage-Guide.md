# Usage Guide

This guide provides practical information for using VkHashDAG, including application controls, editing modes, and workflow examples.

## Getting Started

### First Launch

After building VkHashDAG, launch the application:

```bash
./VkHashDAG
```

The application will:
1. Initialize Vulkan and create the rendering context
2. Load the default HashDAG configuration
3. Create an empty voxel world
4. Display the main 3D viewport with ImGui interface

### System Requirements

- **GPU**: Vulkan 1.1+ compatible graphics card
- **Memory**: 4GB+ RAM, 2GB+ VRAM recommended
- **OS**: Windows 10+, Linux (Ubuntu 18.04+)

## User Interface

### Main Window Layout

```
┌─────────────────────────────────────────────────────────────┐
│ Menu Bar                                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                                                             │
│                3D Viewport                                  │
│                                                             │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ Tool Panel          │ Properties Panel │ Performance Panel │
└─────────────────────────────────────────────────────────────┘
```

### ImGui Panels

#### Tool Panel
- **Edit Mode**: Select between Fill, Dig, and Paint modes
- **Shape**: Choose Sphere or AABB (Axis-Aligned Bounding Box)
- **Size**: Adjust tool size/radius
- **Color**: Select paint color (RGB picker)

#### Properties Panel
- **HashDAG Stats**: Node count, memory usage, compression ratio
- **Render Settings**: Render mode, beam optimization toggle
- **Camera Info**: Position, orientation, FOV

#### Performance Panel
- **Frame Rate**: Current FPS and frame time
- **Memory Usage**: CPU and GPU memory consumption
- **Edit Performance**: Time taken for editing operations

## Camera Controls

### Mouse Navigation

| Action | Control |
|--------|---------|
| **Rotate View** | Left Mouse Button + Drag |
| **Pan View** | Middle Mouse Button + Drag |
| **Zoom** | Mouse Wheel |
| **Edit Voxels** | Right Mouse Button + Drag |

### Keyboard Navigation

| Key | Action |
|-----|--------|
| **W, A, S, D** | Move forward, left, backward, right |
| **Q, E** | Move down, up |
| **Shift** | Move faster |
| **Ctrl** | Move slower |
| **Space** | Reset camera to origin |
| **Tab** | Toggle UI visibility |

### Camera Settings

```cpp
// Typical camera configuration
Camera camera_config{
    .position = {0.0f, 0.0f, 10.0f},
    .target = {0.0f, 0.0f, 0.0f},
    .up = {0.0f, 1.0f, 0.0f},
    .fov = 60.0f,
    .near_plane = 0.1f,
    .far_plane = 1000.0f
};
```

## Editing Modes

### Fill Mode

Add voxels to the world.

**Usage:**
1. Select "Fill" in the Tool Panel
2. Choose shape (Sphere or AABB)
3. Set size and color
4. Right-click and drag in the viewport to add voxels

**Example Workflow:**
```
1. Set tool to Sphere, radius 5.0
2. Choose red color (255, 0, 0)
3. Right-click at position (0, 0, 0)
4. Drag to create filled sphere
```

### Dig Mode

Remove voxels from the world.

**Usage:**
1. Select "Dig" in the Tool Panel
2. Choose shape and size
3. Right-click and drag to remove voxels

**Tips:**
- Use smaller tools for precise removal
- Combine with Fill mode for sculpting
- Hold Shift for continuous digging

### Paint Mode

Change colors of existing voxels without affecting geometry.

**Usage:**
1. Select "Paint" in the Tool Panel
2. Choose new color
3. Right-click on existing voxels to repaint

**Color Blending:**
- Paint mode supports color blending
- Multiple paint operations create smooth color transitions
- VBR compression automatically optimizes color storage

## Tool Shapes

### Sphere Tool

Creates spherical editing regions.

**Parameters:**
- **Radius**: Size of the sphere (0.1 - 100.0)
- **Center**: Automatically positioned at mouse cursor
- **Falloff**: Optional soft edges for smooth blending

**Mathematical Definition:**
```cpp
bool is_inside_sphere(vec3 position, vec3 center, float radius) {
    return length(position - center) <= radius;
}
```

### AABB Tool

Creates box-shaped editing regions.

**Parameters:**
- **Size**: Width, height, depth of the box
- **Orientation**: Aligned with world axes
- **Corner vs Center**: Position reference point

**Mathematical Definition:**
```cpp
bool is_inside_aabb(vec3 position, vec3 min_corner, vec3 max_corner) {
    return all(greaterThanEqual(position, min_corner)) && 
           all(lessThanEqual(position, max_corner));
}
```

## Rendering Modes

### Normal Rendering

Standard ray-traced rendering with full lighting and colors.

**Features:**
- Accurate ray tracing through HashDAG
- VBR color decompression
- Ambient occlusion
- Soft shadows

### Debug Rendering Modes

#### Depth Mode
Visualizes distance from camera as grayscale.

#### Normal Mode
Shows surface normals as RGB colors.

#### Level Mode
Color-codes HashDAG tree levels:
- Red: High-detail levels
- Green: Medium-detail levels  
- Blue: Low-detail levels

#### Iteration Mode
Shows ray tracing complexity:
- Dark: Few iterations (simple geometry)
- Bright: Many iterations (complex geometry)

## Performance Optimization

### Frame Rate Optimization

#### Graphics Settings
- **Beam Optimization**: Enable for 20-50% performance improvement
- **Resolution Scaling**: Reduce render resolution for better performance
- **LOD Distance**: Adjust level-of-detail switching distance

#### Memory Settings
```cpp
// Performance-oriented configuration
hashdag::DefaultConfig<uint32_t> perf_config{
    .level_count = 15,              // Reduce tree depth
    .top_level_count = 8,
    .word_bits_per_page = 14,       // Larger pages
    .page_bits_per_bucket = 3,      // More pages per bucket
    .bucket_bits_per_top_level = 8,
    .bucket_bits_per_bottom_level = 10
};
```

### Memory Usage Optimization

#### Reducing Memory Footprint
1. **Lower Resolution**: Use smaller voxel grids
2. **Aggressive GC**: Run garbage collection more frequently
3. **Color Compression**: Enable maximum VBR compression
4. **History Disabled**: Turn off edit history for color pool

#### Monitoring Memory Usage
- Check "Performance Panel" for real-time memory statistics
- Monitor GPU memory usage through graphics drivers
- Use built-in profiling tools for detailed analysis

## Workflow Examples

### Creating a Simple Scene

1. **Start with Empty World**
   ```
   Launch VkHashDAG → Empty voxel world loads
   ```

2. **Add Basic Geometry**
   ```
   Tool: Sphere, Size: 10.0, Color: Red
   Position: (0, 0, 0)
   Action: Right-click and drag to create sphere
   ```

3. **Add Details**
   ```
   Tool: Sphere, Size: 2.0, Color: Blue
   Position: (5, 5, 5)
   Action: Create smaller detail spheres
   ```

4. **Sculpt with Dig Tool**
   ```
   Tool: Sphere, Size: 3.0, Mode: Dig
   Action: Remove material to create cavities
   ```

### Advanced Sculpting Workflow

1. **Rough Shape Creation**
   ```
   Use large AABB tools to block out basic forms
   Size: 20x20x20 units
   Mode: Fill
   ```

2. **Detail Addition**
   ```
   Switch to smaller sphere tools (radius 1-5)
   Add surface details and features
   Use varying colors for different materials
   ```

3. **Surface Refinement**
   ```
   Use Paint mode to adjust colors
   Apply different materials to different regions
   Blend colors for smooth transitions
   ```

4. **Final Polish**
   ```
   Use very small tools (radius 0.5-1.0)
   Add fine details and texture
   Adjust lighting and camera for final presentation
   ```

### Collaborative Editing

#### Multi-User Considerations
- **Save/Load**: Export scenes for sharing
- **Version Control**: Use external tools for scene versioning
- **Performance**: Coordinate editing to avoid conflicts

#### Best Practices
- **Regular Saves**: Save work frequently
- **Incremental Changes**: Make small, focused edits
- **Documentation**: Keep notes on editing decisions

## Troubleshooting

### Common Issues

#### Performance Problems

**Symptom**: Low frame rate, stuttering
**Solutions**:
1. Enable beam optimization
2. Reduce render resolution
3. Lower voxel resolution
4. Close other GPU-intensive applications

**Symptom**: High memory usage
**Solutions**:
1. Run garbage collection manually
2. Reduce scene complexity
3. Disable edit history
4. Use memory-optimized configuration

#### Rendering Issues

**Symptom**: Black screen or no rendering
**Solutions**:
1. Check Vulkan driver installation
2. Verify GPU compatibility
3. Update graphics drivers
4. Check validation layer messages

**Symptom**: Incorrect colors or artifacts
**Solutions**:
1. Verify VBR color settings
2. Check for memory corruption
3. Restart application
4. Reduce color complexity

#### Editing Problems

**Symptom**: Edits not appearing
**Solutions**:
1. Check edit mode selection
2. Verify tool size settings
3. Ensure proper mouse interaction
4. Check for memory limitations

**Symptom**: Slow editing performance
**Solutions**:
1. Use smaller edit tools
2. Reduce thread count if CPU-limited
3. Enable edit optimizations
4. Clear edit history

### Debug Information

#### Console Output
Monitor console for:
- Vulkan validation messages
- Memory allocation warnings
- Performance statistics
- Error messages

#### Log Files
Check application logs for:
- Detailed error information
- Performance profiling data
- Memory usage patterns
- GPU driver messages

## Advanced Features

### Scripting and Automation

#### Batch Operations
```cpp
// Example: Create a grid of spheres
for (int x = -10; x <= 10; x += 5) {
    for (int y = -10; y <= 10; y += 5) {
        for (int z = -10; z <= 10; z += 5) {
            SphereEditor editor{
                .center = {x, y, z},
                .radius = 2.0f,
                .fill_mode = true,
                .color = random_color()
            };
            apply_edit(editor);
        }
    }
}
```

#### Custom Editors
```cpp
// Example: Noise-based terrain editor
struct TerrainEditor {
    float noise_scale = 0.1f;
    float height_scale = 20.0f;
    
    bool EditVoxel(const NodeCoord& coord, bool current) const {
        float height = noise(coord.x * noise_scale, coord.z * noise_scale) * height_scale;
        return coord.y <= height;
    }
};
```

### Import/Export

#### Supported Formats
- **Native**: VkHashDAG binary format
- **Voxel**: VOX, VXL formats (via conversion)
- **Mesh**: OBJ, PLY (via voxelization)
- **Images**: PNG, JPG (as heightmaps)

#### Export Workflow
1. Select export format
2. Configure export parameters
3. Choose output location
4. Monitor export progress

### Integration with Other Tools

#### 3D Modeling Software
- Export meshes for voxelization
- Use as reference for manual sculpting
- Import textures for color guidance

#### Game Engines
- Export voxel data for game integration
- Use as level geometry
- Implement custom importers

## Related Documentation

- [Build Instructions](Build-Instructions) - Setup and installation
- [Architecture Overview](Architecture-Overview) - System design
- [API Reference](API-Reference) - Programming interface
- [Rendering Pipeline](Rendering-Pipeline) - Graphics implementation
