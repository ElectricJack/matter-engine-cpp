CC = gcc
RAYLIB_PATH = ../Libraries/raylib
CFLAGS = -Wall -Wextra -O2 -I$(RAYLIB_PATH)/src -I./include

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -L$(RAYLIB_PATH)/src -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm
    LDLIBS = $(RAYLIB_PATH)/src/libraylib.a
endif
ifeq ($(UNAME_S),Linux)
    LDFLAGS = -L$(RAYLIB_PATH)/src -lGL -lm -lpthread -ldl -lrt -lX11
    LDLIBS = $(RAYLIB_PATH)/src/libraylib.a
endif
ifeq ($(OS),Windows_NT)
    LDFLAGS = -L$(RAYLIB_PATH)/src -lopengl32 -lgdi32 -lwinmm
    LDLIBS = $(RAYLIB_PATH)/src/libraylib.a
endif

# Source files
SRC = main.c src/scene.c src/bvh.c src/object_allocator.c
OBJ = main.o scene.o bvh.o object_allocator.o
BIN = gpu_raytrace

# Modular system files
MODULAR_SRC = main_modular.c src/bvh.c src/object_allocator.c src/blas_manager.c src/tlas_manager.c
MODULAR_OBJ = main_modular.o bvh.o object_allocator.o blas_manager.o tlas_manager.o
MODULAR_BIN = gpu_raytrace_modular

# Test files
TEST_SRC = test_bvh.c src/bvh.c src/object_allocator.c
TEST_OBJ = test_bvh.o bvh.o object_allocator.o
TEST_BIN = test_bvh

all: $(BIN) $(MODULAR_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(BIN): $(OBJ) raylib
	$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

$(MODULAR_BIN): $(MODULAR_OBJ) raylib
	$(CC) -o $@ $(MODULAR_OBJ) $(LDFLAGS) $(LDLIBS)

$(TEST_BIN): $(TEST_OBJ)
	$(CC) -o $@ $(TEST_OBJ) -lm

# Build rules for object files
scene.o: src/scene.c
	$(CC) -c $< $(CFLAGS) -o $@

bvh.o: src/bvh.c
	$(CC) -c $< $(CFLAGS) -o $@

object_allocator.o: src/object_allocator.c
	$(CC) -c $< $(CFLAGS) -o $@

blas_manager.o: src/blas_manager.c
	$(CC) -c $< $(CFLAGS) -o $@

tlas_manager.o: src/tlas_manager.c
	$(CC) -c $< $(CFLAGS) -o $@

raylib:
	$(MAKE) -C $(RAYLIB_PATH)/src PLATFORM=PLATFORM_DESKTOP

%.o: %.c
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f $(OBJ) $(BIN) $(MODULAR_OBJ) $(MODULAR_BIN) $(TEST_OBJ) $(TEST_BIN)

clean-all: clean
	$(MAKE) -C $(RAYLIB_PATH)/src clean

.PHONY: all clean clean-all raylib