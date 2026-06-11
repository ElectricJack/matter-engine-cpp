# MatterEngine2 Project Structure

This document describes the multi-project architecture for MatterEngine2, explaining how individual projects are organized and how code is shared between them.

## Project Philosophy

MatterEngine2 follows a modular architecture where:

1. Each project is a standalone application that can be built and run independently
2. Projects build on each other by sharing code via symlinks
3. Compilation is fast because only the necessary code is compiled for each project
4. Testing is simplified with self-contained examples

## Project Structure

The root directory contains:

- `Libraries/` - Third-party dependencies (like raylib)
- Individual project directories (e.g., `BasicWindowApp`, `SurfaceLib`)
- This documentation file

Each project follows this general structure:

```
ProjectName/
├── Makefile        # Project-specific build configuration
├── README.md       # Project documentation
├── main.c          # Main application entry point
├── include/        # Public API headers (for library projects)
│   └── *.h         # Header files defining the project's public API
└── src/            # Implementation files (for library projects)
    └── *.c         # Source code implementing the library
```

## Code Sharing via Symlinks

To share code between projects while maintaining independence:

1. Library projects (like `SurfaceLib`) organize reusable code in `include/` and `src/` directories
2. Consumer projects create symlinks to the specific files or directories they want to use
3. Makefiles in consumer projects include the appropriate include paths

Example:
```bash
# From a new project that wants to use SurfaceLib
ln -s ../SurfaceLib/include/surface.h include/surface.h
ln -s ../SurfaceLib/src/surface.c src/surface.c
```

### Benefits of this approach:

- Clear dependency graph between projects
- Each project can be built and run independently
- Easy to add, remove, or modify dependencies
- Fast incremental builds
- Self-documenting structure

## Adding a New Project

To create a new project that builds on existing ones:

1. Create a new directory with the project name
2. Copy the basic structure (Makefile, main.c, etc.) from a similar project
3. Create symlinks to code from other projects you want to reuse
4. Update the Makefile to include the necessary dependencies
5. Build and test your new project independently

## Building Projects

Each project has its own Makefile and can be built independently:

```bash
cd ProjectName
make
./project_executable  # Run the compiled application
```

The build system ensures that:
- Only the code needed for the specific project is compiled
- Dependencies are correctly handled
- Each project can use different compiler flags if needed

## Project Relationships

Current projects and their relationships:

1. **BasicWindowApp** - Simple raylib application showing a rotating cube
   - Dependencies: raylib

2. **SurfaceLib** - Isosurface geometry library with visualization
   - Dependencies: raylib
   - Provides: Isosurface generation algorithms

Future projects can build on these existing components by creating the appropriate symlinks.