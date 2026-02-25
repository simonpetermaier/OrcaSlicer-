# OrcaSlicer Build Guide

## Quick Start

The space in your directory name was causing the build to fail. I've renamed "Cone Slicing" to "Cone-Slicing" to fix this issue.

### Build Everything (First Time)

```bash
cd /Users/simonpetermaier/Documents/Code\ Projekte/Nonplanar\ Slicer/Cone-Slicing
./build_orca.sh
```

This will:
1. Build all dependencies (GMP, MPFR, CGAL, wxWidgets, etc.)
2. Build OrcaSlicer application
3. Package the .app bundle

**Estimated time:** 30-60 minutes (dependencies take the longest)

## Build Script Options

```bash
./build_orca.sh [options]

Options:
  -d           Build dependencies only
  -s           Build slicer only (skip dependencies)
  -c           Clean build (remove build directories first)
  -x           Use Ninja Multi-Config generator (default: Xcode)
  -a <arch>    Architecture: arm64, x86_64, or universal
  -j <n>       Number of parallel jobs
  -h           Show help message
```

## Common Build Scenarios

### 1. First Build (Everything)
```bash
./build_orca.sh
```

### 2. Rebuild After Code Changes (Skip Deps)
```bash
./build_orca.sh -s
```

### 3. Clean Rebuild
```bash
./build_orca.sh -c
```

### 4. Fast Build with Ninja (8 parallel jobs)
```bash
./build_orca.sh -x -j 8
```

### 5. Build Dependencies Only (After Clean)
```bash
./build_orca.sh -d
```

### 6. Build Universal Binary (arm64 + x86_64)
```bash
./build_orca.sh -a universal
```

## Build Output Locations

- **Dependencies:** `deps/build/<arch>/`
- **Slicer build files:** `build/<arch>/`
- **Packaged app:** `build/<arch>/OrcaSlicer/OrcaSlicer.app`

## Running OrcaSlicer After Build

```bash
# For arm64 Mac (Apple Silicon)
open build/arm64/OrcaSlicer/OrcaSlicer.app

# For x86_64 Mac (Intel)
open build/x86_64/OrcaSlicer/OrcaSlicer.app

# For universal build
open build/universal/OrcaSlicer/OrcaSlicer.app
```

## Troubleshooting

### Error 2 (make: *** No rule to make target `Makefile')

**Cause:** Space in directory path was breaking the build
**Solution:** Already fixed - directory renamed to "Cone-Slicing"

### Dependencies Build Fails

**Solution:** Clean rebuild of dependencies
```bash
rm -rf deps/build
./build_orca.sh -d
```

### Slicer Build Fails But Deps OK

**Solution:** Clean rebuild of slicer only
```bash
rm -rf build
./build_orca.sh -s
```

### Need to Start Completely Fresh

```bash
# Remove all build artifacts
rm -rf deps/build build

# Full rebuild
./build_orca.sh
```

### CMake Can't Find Dependencies

Make sure dependencies are built first:
```bash
./build_orca.sh -d  # Build deps
./build_orca.sh -s  # Then build slicer
```

## Manual CMake Commands

If you prefer to run CMake manually:

### 1. Configure Dependencies
```bash
cd deps/build/arm64
cmake ../../ \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3
```

### 2. Build Dependencies
```bash
cmake --build . --config Release --target deps
```

### 3. Configure OrcaSlicer
```bash
cd ../../../build/arm64
cmake ../../ \
    -G "Xcode" \
    -DORCA_TOOLS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3
```

### 4. Build OrcaSlicer
```bash
cmake --build . --config Release --target ALL_BUILD
```

## Adding Conical Slicing Feature

After you have a successful build, here's how to add your conical slicing feature:

### 1. Core Implementation Files

Create new files in `src/libslic3r/`:
- `ConicalSlicer.hpp` - Header for conical slicing algorithm
- `ConicalSlicer.cpp` - Implementation

### 2. Add to CMake Build

Edit `src/libslic3r/CMakeLists.txt` and add:
```cmake
add_library(libslic3r STATIC
    # ... existing files ...
    ConicalSlicer.cpp
    ConicalSlicer.hpp
)
```

### 3. Add GUI Controls

Edit `src/slic3r/GUI/Tab.cpp` to add menu items for:
- Slicing type dropdown (traditional/nonplanar)
- Cone angle parameter input
- Other cone-specific settings

### 4. Add Configuration Options

Edit `src/libslic3r/PrintConfig.cpp` to define:
- `slicing_type` enum (traditional, conical, etc.)
- `cone_angle` float parameter
- Default values and bounds

### 5. Rebuild After Changes

```bash
./build_orca.sh -s  # Fast rebuild (skip deps)
```

## Development Workflow

1. **Make code changes**
2. **Quick rebuild:** `./build_orca.sh -s`
3. **Test:** `open build/arm64/OrcaSlicer/OrcaSlicer.app`
4. **Repeat**

For faster iteration, use Ninja:
```bash
./build_orca.sh -x -s -j 8
```

## Build Logs

Build output is displayed in terminal with color-coded sections:
- 🔵 Blue: Information
- 🟡 Yellow: Commands being executed
- 🟢 Green: Success messages
- 🔴 Red: Errors

To save build log to file:
```bash
./build_orca.sh 2>&1 | tee build.log
```

## System Requirements

- **macOS:** 11.3 or later
- **Xcode Command Line Tools:** Required
- **CMake:** 3.13 or later
- **Disk Space:** ~10GB for full build
- **RAM:** 8GB minimum, 16GB recommended
- **Time:** 30-60 minutes for first build, 5-10 min for incremental

## Additional Resources

- OrcaSlicer Repo: https://github.com/OrcaSlicer/OrcaSlicer
- Original build script: `build_release_macos.sh`
- CMake documentation: https://cmake.org/documentation/
