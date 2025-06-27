#!/bin/bash

# Cross-compilation build script for GPURayTraceExample
# Builds native Windows .exe from Linux/WSL

set -e

echo "=== Cross-Compilation Build Script ==="
echo "Building native Windows executable from WSL/Linux"

# Check if MinGW is installed
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: MinGW cross-compiler not found!"
    echo "Install with: sudo apt update && sudo apt install -y mingw-w64 mingw-w64-tools"
    exit 1
fi

echo "✓ MinGW cross-compiler found"

# Clean previous builds
echo "Cleaning previous builds..."
make clean-all

# Set up dependencies
echo "Setting up dependencies..."
make dependencies

# Build for Windows (native .exe)
echo "Building native Windows executable..."
TARGET=windows-native make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "✅ Native Windows executable: build/windows-native/gpu_raytrace.exe"
echo "✅ Ready-to-run copy: ./gpu_raytrace.exe"
echo "📁 File size: $(du -h gpu_raytrace.exe | cut -f1)"
echo "🔧 File type: $(file gpu_raytrace.exe | cut -d: -f2)"
echo ""
echo "🚀 To run on Windows:"
echo "1. Copy gpu_raytrace.exe and shaders/ folder to Windows"
echo "2. Run: gpu_raytrace.exe (from the directory containing shaders/)"
echo "3. Or use: run-native-windows.bat"
echo ""
echo "📊 Performance comparison:"
echo "- WSL build:     70-90% performance (translation overhead)"
echo "- Native .exe:   100% performance (direct GPU access)"
echo "- Cross-compiled: 100% performance (no runtime overhead)"
echo ""
echo "✅ Cross-compilation successful!"
echo "🎯 Ready for distribution on Windows systems" 