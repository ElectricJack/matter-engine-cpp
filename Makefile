CC = gcc
CXX = g++
RAYLIB_PATH = ../Libraries/raylib
ODE_PATH = ../Libraries/ode
CFLAGS = -Wall -Wextra -O2 -I$(RAYLIB_PATH)/src -I$(ODE_PATH)/include -I$(ODE_BUILD_DIR)/include -I./include
CXXFLAGS = -Wall -Wextra -O2 -std=c++14 -Wno-missing-field-initializers -I$(RAYLIB_PATH)/src -I$(ODE_PATH)/include -I$(ODE_BUILD_DIR)/include -I./include

# Build flags:
# TARGET=windows-native : Cross-compile for Windows from Linux using MinGW
# NO_MINGW=1           : Use alternative Windows toolchain instead of MinGW (Windows only)
# WSL_LINUX=1          : Build Linux executable from WSL (default is Windows cross-compile)
# 
# Examples:
#   make                     # Build for current platform (auto-detects WSL→Windows, Windows→MinGW)
#   make WSL_LINUX=1         # Build Linux executable from WSL
#   make NO_MINGW=1          # Build on Windows using alternative toolchain (MSVC, etc.)
#   TARGET=windows-native make # Cross-compile from Linux to Windows

# Cross-compilation and toolchain support
ifeq ($(TARGET),windows-native)
    # Cross-compile for Windows from Linux/WSL
    CC = x86_64-w64-mingw32-gcc
    CXX = x86_64-w64-mingw32-g++
    PLATFORM = windows-native
    PLATFORM_DEFINE = PLATFORM_DESKTOP
    LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -static-libgcc -static-libstdc++ -lopengl32 -lgdi32 -lwinmm -lode
    LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
    BIN_SUFFIX = .exe
else
    # Auto-detect OS and set platform-specific variables
    UNAME_S := $(shell uname -s)
    
    # WSL detection
    IS_WSL := 0
    ifneq ($(shell uname -r | grep -i microsoft),)
        IS_WSL := 1
    endif
    ifneq ($(shell uname -r | grep -i wsl),)
        IS_WSL := 1
    endif
    ifneq ($(wildcard /proc/version),)
        ifneq ($(shell grep -i microsoft /proc/version 2>/dev/null),)
            IS_WSL := 1
        endif
    endif
    
    # Windows detection (multiple methods for robustness)
    IS_WINDOWS := 0
    ifneq ($(OS),)
        ifeq ($(OS),Windows_NT)
            IS_WINDOWS := 1
        endif
    endif
    # Also check uname output for Windows patterns
    ifneq ($(findstring CYGWIN,$(UNAME_S)),)
        IS_WINDOWS := 1
    endif
    ifneq ($(findstring MINGW,$(UNAME_S)),)
        IS_WINDOWS := 1
    endif
    ifneq ($(findstring MSYS,$(UNAME_S)),)
        IS_WINDOWS := 1
    endif
    
    ifeq ($(UNAME_S),Darwin)
        PLATFORM = macos
        PLATFORM_DEFINE = PLATFORM_DESKTOP
        LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm -lode
        LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
    else ifeq ($(IS_WSL),1)
        # WSL detected - default to cross-compile to Windows unless WSL_LINUX=1
        ifeq ($(WSL_LINUX),1)
            # User wants Linux executable from WSL
            PLATFORM = linux
            PLATFORM_DEFINE = PLATFORM_DESKTOP
            LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -lGL -lm -lpthread -ldl -lrt -lX11 -lode
            LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
        else
            # Default: Cross-compile to Windows from WSL
            CC = x86_64-w64-mingw32-gcc
            CXX = x86_64-w64-mingw32-g++
            PLATFORM = windows-native
            PLATFORM_DEFINE = PLATFORM_DESKTOP
            LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -static-libgcc -static-libstdc++ -lopengl32 -lgdi32 -lwinmm -lode_doubles
            LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
            BIN_SUFFIX = .exe
        endif
    else ifeq ($(UNAME_S),Linux)
        PLATFORM = linux
        PLATFORM_DEFINE = PLATFORM_DESKTOP
        LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -lGL -lm -lpthread -ldl -lrt -lX11 -lode
        LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
    else ifeq ($(IS_WINDOWS),1)
        PLATFORM_DEFINE = PLATFORM_DESKTOP
        # On Windows, use MinGW by default for native .exe builds
        # Set NO_MINGW=1 to use default Windows toolchain instead
        ifeq ($(NO_MINGW),1)
            # Alternative Windows toolchain (MSVC, regular gcc, etc.)
            PLATFORM = windows
            LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -lopengl32 -lgdi32 -lwinmm -lode
        else
            # Default: Use MinGW toolchain for native Windows builds
            CC = x86_64-w64-mingw32-gcc
            CXX = x86_64-w64-mingw32-g++
            PLATFORM = windows-mingw
            LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -static-libgcc -static-libstdc++ -lopengl32 -lgdi32 -lwinmm -lode_doubles
        endif
        LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
        BIN_SUFFIX = .exe
    else
        # Fallback for unknown platforms - assume Unix-like
        PLATFORM = unknown
        PLATFORM_DEFINE = PLATFORM_DESKTOP
        LDFLAGS = -L$(RAYLIB_PATH)/build/$(PLATFORM) -L$(ODE_BUILD_DIR) -lGL -lm -lpthread -ldl -lrt -lX11 -lode
        LDLIBS = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
    endif
endif

# Platform-specific directories
BUILD_DIR = build/$(PLATFORM)
OBJ_DIR = $(BUILD_DIR)/obj

# Create platform-specific directories
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(RAYLIB_PATH)/build/$(PLATFORM))

# C++ main application
SRC = main.cpp src/particle_system.cpp src/object_allocator.c
OBJ = $(OBJ_DIR)/main.o $(OBJ_DIR)/particle_system.o $(OBJ_DIR)/object_allocator.o
BIN = $(BUILD_DIR)/particle_dynamics$(BIN_SUFFIX)

all: dependencies ode $(BIN)

dependencies:
	@echo "Setting up dependencies for $(PLATFORM)..."
	@mkdir -p src
	@cp ../ObjectAllocatorLib/src/object_allocator.c src/

# ODE library setup
ODE_BUILD_DIR = $(ODE_PATH)/build
ODE_LIB = $(ODE_BUILD_DIR)/libode.a
ODE_FORCE_FILE = $(ODE_PATH)/.ode_built

ode: $(ODE_LIB)

$(ODE_LIB): $(ODE_FORCE_FILE)
	@echo "Building ODE with CMake for $(PLATFORM)..."
	@mkdir -p $(ODE_BUILD_DIR)
	@cd $(ODE_BUILD_DIR) && cmake .. -DBUILD_SHARED_LIBS=OFF -DODE_WITH_DEMOS=OFF -DODE_WITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
	@cd $(ODE_BUILD_DIR) && cmake --build . --config Release
	@echo "ODE built successfully"

$(ODE_FORCE_FILE):
	@echo "Creating ODE build marker..."
	@mkdir -p $(ODE_PATH)
	@if [ ! -d "$(ODE_PATH)/include" ]; then echo "ODE library not found. Please download ODE first with 'make download-ode'"; false; fi
	@touch $(ODE_FORCE_FILE)

download-ode:
	@echo "Downloading Open Dynamics Engine..."
	@cd ../Libraries && git clone https://github.com/ode/ode.git

# Platform-specific raylib build with force rebuild check
RAYLIB_LIB = $(RAYLIB_PATH)/build/$(PLATFORM)/libraylib.a
RAYLIB_FORCE_FILE = $(RAYLIB_PATH)/build/$(PLATFORM)/.raylib_$(PLATFORM)

raylib: $(RAYLIB_LIB)

$(RAYLIB_LIB): $(RAYLIB_FORCE_FILE)
	@echo "Building raylib for $(PLATFORM)..."
	@mkdir -p $(RAYLIB_PATH)/build/$(PLATFORM)
ifeq ($(TARGET),windows-native)
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS clean
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS
else ifeq ($(PLATFORM),windows-native)
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS clean
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS
else ifeq ($(PLATFORM),windows-mingw)
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS clean
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) PLATFORM_OS=WINDOWS
else
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC) clean
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=$(PLATFORM_DEFINE) CC=$(CC)
endif
	@cp $(RAYLIB_PATH)/src/libraylib.a $(RAYLIB_LIB)
	@echo "Raylib built and copied to $(RAYLIB_LIB)"

$(RAYLIB_FORCE_FILE):
	@echo "Creating platform marker for $(PLATFORM)..."
	@mkdir -p $(RAYLIB_PATH)/build/$(PLATFORM)
	@rm -f $(RAYLIB_PATH)/build/*/.raylib_*
	@touch $(RAYLIB_FORCE_FILE)

$(BIN): $(OBJ) raylib
ifeq ($(TARGET),windows-native)
	# For Windows cross-compilation, link raylib first to avoid symbol conflicts
	$(CXX) -o $@ $(OBJ) $(LDLIBS) $(LDFLAGS)
else ifeq ($(PLATFORM),windows-native)
	# For WSL→Windows cross-compilation, link raylib first to avoid symbol conflicts
	$(CXX) -o $@ $(OBJ) $(LDLIBS) $(LDFLAGS)
else ifeq ($(PLATFORM),windows-mingw)
	# For Windows MinGW builds, link raylib first to avoid symbol conflicts
	$(CXX) -o $@ $(OBJ) $(LDLIBS) $(LDFLAGS)
else
	$(CXX) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)
endif

# Compile rules
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/

clean-all: clean
	rm -rf $(RAYLIB_PATH)/build/
	@if [ -d "$(ODE_PATH)" ]; then $(MAKE) -C $(ODE_PATH) clean; fi

.PHONY: all dependencies ode download-ode raylib clean clean-all
