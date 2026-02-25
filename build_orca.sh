#!/bin/bash

# OrcaSlicer Build Script with CMake Commands
# This script builds OrcaSlicer with all dependencies on macOS
# Usage: ./build_orca.sh [options]
#   -d: Build dependencies only
#   -s: Build slicer only (skip deps)
#   -c: Clean build (remove build directories first)
#   -x: Use Ninja instead of Xcode generator
#   -a <arch>: Specify architecture (arm64, x86_64, or universal)
#   -j <n>: Number of parallel jobs (default: auto)

set -e  # Exit on error
set -o pipefail  # Catch errors in pipes

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
BUILD_DEPS=true
BUILD_SLICER=true
CLEAN_BUILD=false
USE_NINJA=false
ARCH="$(uname -m)"
PARALLEL_JOBS=""

while getopts ":dscxa:j:h" opt; do
  case "${opt}" in
    d )
      BUILD_SLICER=false
      ;;
    s )
      BUILD_DEPS=false
      ;;
    c )
      CLEAN_BUILD=true
      ;;
    x )
      USE_NINJA=true
      ;;
    a )
      ARCH="$OPTARG"
      ;;
    j )
      PARALLEL_JOBS="$OPTARG"
      ;;
    h )
      echo "OrcaSlicer Build Script"
      echo ""
      echo "Usage: $0 [options]"
      echo ""
      echo "Options:"
      echo "  -d           Build dependencies only"
      echo "  -s           Build slicer only (skip dependencies)"
      echo "  -c           Clean build (remove build directories first)"
      echo "  -x           Use Ninja Multi-Config generator (default: Xcode)"
      echo "  -a <arch>    Architecture: arm64, x86_64, or universal (default: current)"
      echo "  -j <n>       Number of parallel jobs"
      echo "  -h           Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0              # Full build with defaults"
      echo "  $0 -d           # Build dependencies only"
      echo "  $0 -s           # Build slicer only (assumes deps are built)"
      echo "  $0 -c           # Clean and full rebuild"
      echo "  $0 -x -j 8      # Use Ninja with 8 parallel jobs"
      echo "  $0 -a universal # Build universal binary (arm64 + x86_64)"
      exit 0
      ;;
    * )
      echo "Invalid option: -$OPTARG" 1>&2
      echo "Use -h for help"
      exit 1
      ;;
  esac
done

# Verify we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Are you in the OrcaSlicer directory?${NC}"
    exit 1
fi

# Set CMake generator based on flags
# CMake 4.x has issues with Xcode generator, force Ninja
CMAKE_VERSION=$(cmake --version | head -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/')
if [ "$CMAKE_VERSION" -ge 4 ] 2>/dev/null; then
    echo -e "${YELLOW}CMake 4.x detected - forcing Ninja generator (Xcode not compatible)${NC}"
    SLICER_CMAKE_GENERATOR="Ninja Multi-Config"
    SLICER_BUILD_TARGET="all"
    DEPS_CMAKE_GENERATOR="Ninja"
elif [ "$USE_NINJA" = true ]; then
    SLICER_CMAKE_GENERATOR="Ninja Multi-Config"
    SLICER_BUILD_TARGET="all"
    DEPS_CMAKE_GENERATOR="Ninja"
else
    SLICER_CMAKE_GENERATOR="Xcode"
    SLICER_BUILD_TARGET="ALL_BUILD"
    DEPS_CMAKE_GENERATOR="Unix Makefiles"
fi

# Set build configuration
BUILD_CONFIG="Release"
OSX_DEPLOYMENT_TARGET="11.3"

# CMake 4.x compatibility
if [ "$CMAKE_VERSION" -ge 4 ] 2>/dev/null; then
  export CMAKE_POLICY_VERSION_MINIMUM=3.5
  CMAKE_POLICY_COMPAT="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
else
  CMAKE_POLICY_COMPAT=""
fi

# Set parallel jobs if not specified
if [ -n "$PARALLEL_JOBS" ]; then
    export CMAKE_BUILD_PARALLEL_LEVEL=$PARALLEL_JOBS
fi

# Print build configuration
echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║${NC}          ${GREEN}OrcaSlicer Build Configuration${NC}                      ${BLUE}║${NC}"
echo -e "${BLUE}╠════════════════════════════════════════════════════════════════╣${NC}"
echo -e "${BLUE}║${NC} Architecture:          ${YELLOW}$ARCH${NC}"
echo -e "${BLUE}║${NC} Build Config:          ${YELLOW}$BUILD_CONFIG${NC}"
echo -e "${BLUE}║${NC} Slicer Generator:      ${YELLOW}$SLICER_CMAKE_GENERATOR${NC}"
echo -e "${BLUE}║${NC} Deps Generator:        ${YELLOW}$DEPS_CMAKE_GENERATOR${NC}"
echo -e "${BLUE}║${NC} macOS Target:          ${YELLOW}$OSX_DEPLOYMENT_TARGET${NC}"
echo -e "${BLUE}║${NC} Build Dependencies:    ${YELLOW}$BUILD_DEPS${NC}"
echo -e "${BLUE}║${NC} Build Slicer:          ${YELLOW}$BUILD_SLICER${NC}"
echo -e "${BLUE}║${NC} Clean Build:           ${YELLOW}$CLEAN_BUILD${NC}"
if [ -n "$PARALLEL_JOBS" ]; then
    echo -e "${BLUE}║${NC} Parallel Jobs:         ${YELLOW}$PARALLEL_JOBS${NC}"
fi
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Function to build dependencies
build_dependencies() {
    local _ARCH=$1

    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Building Dependencies for $_ARCH${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    DEPS_BUILD_DIR="$SCRIPT_DIR/deps/build/$_ARCH"
    DEPS_DIR="$SCRIPT_DIR/deps"

    # Clean if requested
    if [ "$CLEAN_BUILD" = true ] && [ -d "$DEPS_BUILD_DIR" ]; then
        echo -e "${YELLOW}Cleaning dependencies build directory...${NC}"
        rm -rf "$DEPS_BUILD_DIR"
    fi

    mkdir -p "$DEPS_BUILD_DIR"
    cd "$DEPS_BUILD_DIR"

    echo -e "${BLUE}Step 1: Configuring dependencies with CMake...${NC}"
    echo -e "${YELLOW}Command:${NC} cmake ${DEPS_DIR} -G \"${DEPS_CMAKE_GENERATOR}\" \\"
    echo "         -DCMAKE_BUILD_TYPE=\"$BUILD_CONFIG\" \\"
    echo "         -DCMAKE_OSX_ARCHITECTURES=\"${_ARCH}\" \\"
    echo "         -DCMAKE_OSX_DEPLOYMENT_TARGET=\"${OSX_DEPLOYMENT_TARGET}\" \\"
    echo "         ${CMAKE_POLICY_COMPAT}"
    echo ""

    cmake "${DEPS_DIR}" \
        -G "${DEPS_CMAKE_GENERATOR}" \
        -DCMAKE_BUILD_TYPE="$BUILD_CONFIG" \
        -DCMAKE_OSX_ARCHITECTURES="${_ARCH}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
        ${CMAKE_POLICY_COMPAT}

    echo ""
    echo -e "${BLUE}Step 2: Building dependencies...${NC}"
    echo -e "${YELLOW}Command:${NC} cmake --build . --config $BUILD_CONFIG --target deps"
    echo ""

    cmake --build . --config "$BUILD_CONFIG" --target deps

    echo -e "${GREEN}✓ Dependencies built successfully for $_ARCH${NC}"
    echo ""
}

# Function to build slicer
build_slicer() {
    local _ARCH=$1

    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Building OrcaSlicer for $_ARCH${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    PROJECT_BUILD_DIR="$SCRIPT_DIR/build/$_ARCH"

    # Clean if requested
    if [ "$CLEAN_BUILD" = true ] && [ -d "$PROJECT_BUILD_DIR" ]; then
        echo -e "${YELLOW}Cleaning slicer build directory...${NC}"
        rm -rf "$PROJECT_BUILD_DIR"
    fi

    mkdir -p "$PROJECT_BUILD_DIR"
    cd "$PROJECT_BUILD_DIR"

    echo -e "${BLUE}Step 1: Configuring OrcaSlicer with CMake...${NC}"
    echo -e "${YELLOW}Command:${NC} cmake ${SCRIPT_DIR} -G \"${SLICER_CMAKE_GENERATOR}\" \\"
    echo "         -DORCA_TOOLS=ON \\"
    echo "         -DCMAKE_BUILD_TYPE=\"$BUILD_CONFIG\" \\"
    echo "         -DCMAKE_OSX_ARCHITECTURES=\"${_ARCH}\" \\"
    echo "         -DCMAKE_OSX_DEPLOYMENT_TARGET=\"${OSX_DEPLOYMENT_TARGET}\" \\"
    echo "         ${CMAKE_POLICY_COMPAT}"
    echo ""

    cmake "${SCRIPT_DIR}" \
        -G "${SLICER_CMAKE_GENERATOR}" \
        -DORCA_TOOLS=ON \
        -DCMAKE_BUILD_TYPE="$BUILD_CONFIG" \
        -DCMAKE_OSX_ARCHITECTURES="${_ARCH}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
        ${CMAKE_POLICY_COMPAT}

    echo ""
    echo -e "${BLUE}Step 2: Building OrcaSlicer...${NC}"
    echo -e "${YELLOW}Command:${NC} cmake --build . --config $BUILD_CONFIG --target $SLICER_BUILD_TARGET"
    echo ""

    cmake --build . --config "$BUILD_CONFIG" --target "$SLICER_BUILD_TARGET"

    echo -e "${GREEN}✓ OrcaSlicer built successfully for $_ARCH${NC}"
    echo ""

    # Package the app
    package_app "$_ARCH"
}

# Function to package the app
package_app() {
    local _ARCH=$1

    echo -e "${BLUE}Packaging OrcaSlicer.app...${NC}"

    PROJECT_BUILD_DIR="$SCRIPT_DIR/build/$_ARCH"
    cd "$PROJECT_BUILD_DIR"

    # Determine the build directory suffix
    if [ "$SLICER_CMAKE_GENERATOR" = "Xcode" ]; then
        BUILD_DIR_SUFFIX="/$BUILD_CONFIG"
    else
        BUILD_DIR_SUFFIX="/$BUILD_CONFIG"
    fi

    mkdir -p OrcaSlicer
    cd OrcaSlicer

    # Remove old app and copy new one
    rm -rf ./OrcaSlicer.app
    cp -pR "../src${BUILD_DIR_SUFFIX}/OrcaSlicer.app" ./OrcaSlicer.app

    # Fix resources (resolve symlink)
    resources_path=$(readlink ./OrcaSlicer.app/Contents/Resources || echo "")
    if [ -n "$resources_path" ]; then
        rm ./OrcaSlicer.app/Contents/Resources
        cp -R "$resources_path" ./OrcaSlicer.app/Contents/Resources
    fi

    # Clean up .DS_Store files
    find ./OrcaSlicer.app/ -name '.DS_Store' -delete

    echo -e "${GREEN}✓ OrcaSlicer.app packaged at: $PROJECT_BUILD_DIR/OrcaSlicer/OrcaSlicer.app${NC}"
}

# Function to build universal binary
build_universal() {
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Creating Universal Binary${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    PROJECT_BUILD_DIR="$SCRIPT_DIR/build/$ARCH"
    mkdir -p "$PROJECT_BUILD_DIR/OrcaSlicer"

    UNIVERSAL_APP="$PROJECT_BUILD_DIR/OrcaSlicer/OrcaSlicer.app"
    rm -rf "$UNIVERSAL_APP"
    cp -R "$SCRIPT_DIR/build/arm64/OrcaSlicer/OrcaSlicer.app" "$UNIVERSAL_APP"

    # Get the binary path
    BINARY_PATH="Contents/MacOS/OrcaSlicer"

    echo -e "${BLUE}Creating universal binary with lipo...${NC}"
    lipo -create \
        "$SCRIPT_DIR/build/x86_64/OrcaSlicer/OrcaSlicer.app/$BINARY_PATH" \
        "$SCRIPT_DIR/build/arm64/OrcaSlicer/OrcaSlicer.app/$BINARY_PATH" \
        -output "$UNIVERSAL_APP/$BINARY_PATH"

    echo -e "${GREEN}✓ Universal binary created at: $UNIVERSAL_APP${NC}"
}

# Main build logic
START_TIME=$(date +%s)

if [ "$ARCH" = "universal" ]; then
    # Build for both architectures
    for _ARCH in x86_64 arm64; do
        if [ "$BUILD_DEPS" = true ]; then
            build_dependencies "$_ARCH"
        fi

        if [ "$BUILD_SLICER" = true ]; then
            build_slicer "$_ARCH"
        fi
    done

    # Create universal binary
    if [ "$BUILD_SLICER" = true ]; then
        build_universal
    fi
else
    # Build for single architecture
    if [ "$BUILD_DEPS" = true ]; then
        build_dependencies "$ARCH"
    fi

    if [ "$BUILD_SLICER" = true ]; then
        build_slicer "$ARCH"
    fi
fi

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
MINUTES=$((DURATION / 60))
SECONDS=$((DURATION % 60))

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║${NC}                  ${GREEN}✓ Build Complete!${NC}                          ${GREEN}║${NC}"
echo -e "${GREEN}╠════════════════════════════════════════════════════════════════╣${NC}"
echo -e "${GREEN}║${NC} Build time: ${YELLOW}${MINUTES}m ${SECONDS}s${NC}"
if [ "$BUILD_SLICER" = true ]; then
    if [ "$ARCH" = "universal" ]; then
        APP_PATH="$SCRIPT_DIR/build/universal/OrcaSlicer/OrcaSlicer.app"
    else
        APP_PATH="$SCRIPT_DIR/build/$ARCH/OrcaSlicer/OrcaSlicer.app"
    fi
    echo -e "${GREEN}║${NC} Application: ${YELLOW}$APP_PATH${NC}"
fi
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ "$BUILD_SLICER" = true ]; then
    echo -e "${BLUE}To run OrcaSlicer:${NC}"
    echo -e "  open \"$APP_PATH\""
    echo ""
fi
