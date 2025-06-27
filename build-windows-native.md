# Building Native Windows Executable

For maximum performance on Windows, build a native .exe using Visual Studio.

## Option 1: Visual Studio 2022 (Recommended)

### Prerequisites
1. **Install Visual Studio 2022 Community** (free)
   - Include "Desktop development with C++" workload
   - Include Windows 10/11 SDK
   - Include CMake tools

### Setup Steps

1. **Clone/Copy Project to Windows**:
   ```cmd
   # Copy the project to a Windows path (not WSL)
   xcopy "\\wsl$\Ubuntu\mnt\d\Shared With Desktop\AI\MatterEngine2\GPURayTraceExample" "C:\Dev\GPURayTraceExample" /E /I
   ```

2. **Create CMakeLists.txt**:
   ```cmake
   cmake_minimum_required(VERSION 3.20)
   project(GPURayTraceExample)

   set(CMAKE_CXX_STANDARD 14)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)

   # Add raylib
   add_subdirectory(../Libraries/raylib)

   # Include directories
   include_directories(include)
   include_directories(../Libraries/raylib/src)

   # Source files
   set(SOURCES
       main.cpp
       src/bvh_new.cpp
       src/blas_manager.cpp
       src/tlas_manager.cpp
       src/bvh_visualizer.cpp
       src/object_allocator.c
   )

   # Create executable
   add_executable(${PROJECT_NAME} ${SOURCES})

   # Link libraries
   target_link_libraries(${PROJECT_NAME} raylib)

   # Windows-specific settings
   if(WIN32)
       target_link_libraries(${PROJECT_NAME} opengl32 gdi32 winmm)
       set_target_properties(${PROJECT_NAME} PROPERTIES
           WIN32_EXECUTABLE TRUE
       )
   endif()
   ```

3. **Build with Visual Studio**:
   ```cmd
   cd C:\Dev\GPURayTraceExample
   mkdir build-vs
   cd build-vs
   cmake .. -G "Visual Studio 17 2022"
   cmake --build . --config Release
   ```

## Option 2: MinGW-w64 (Cross-compile from WSL)

### Install MinGW in WSL
```bash
sudo apt update
sudo apt install mingw-w64 mingw-w64-tools
```

### Create Windows Makefile
Add to existing Makefile:
```makefile
# Windows cross-compilation
ifeq ($(TARGET),windows-native)
    CC = x86_64-w64-mingw32-gcc
    CXX = x86_64-w64-mingw32-g++
    PLATFORM = windows-native
    PLATFORM_DEFINE = PLATFORM_DESKTOP
    LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++
    LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
    BIN = $(BUILD_DIR)/gpu_raytrace.exe
endif
```

### Build Windows .exe
```bash
# Build raylib for Windows
cd ../Libraries/raylib/src
make PLATFORM=PLATFORM_DESKTOP CC=x86_64-w64-mingw32-gcc

# Build your project
cd ../../../GPURayTraceExample
TARGET=windows-native make
```

## Option 3: MSYS2/MinGW (Native Windows)

### Install MSYS2
1. Download from https://www.msys2.org/
2. Install and update:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
   ```

3. Build in MSYS2 terminal:
   ```bash
   cd /c/Dev/GPURayTraceExample
   make
   ```

## Performance Comparison

| Build Method | Performance | Setup Complexity | GPU Access |
|--------------|-------------|------------------|-------------|
| WSL          | 70-90%      | Easy            | Indirect    |
| Visual Studio| 100%        | Medium          | Direct      |
| MinGW Cross  | 95-100%     | Medium          | Direct      |
| MSYS2        | 95-100%     | Easy            | Direct      |

## Recommended Approach

1. **Development**: Continue using WSL for convenience
2. **Performance Testing**: Build with Visual Studio Release mode
3. **Distribution**: Use Visual Studio for final builds

The native Windows build will give you:
- Direct GPU driver access
- No translation layer overhead  
- Better debugging tools
- Optimal compiler optimizations
- Access to Windows-specific GPU features 