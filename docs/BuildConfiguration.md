# MachineController Build and Debug Configuration

## VS Code Extensions

For the best development experience, install these VS Code extensions:

1. **CMake Tools** (ms-vscode.cmake-tools)
   - CMake support and project management

2. **CodeLLDB** (vadimcn.vscode-lldb)
   - LLDB debugger integration for VS Code

3. **Clang** (llvm-vs-code-extensions.vscode-clangd)
   - C/C++ language support and formatting

## Build System
- **Generator**: Ninja
- **Build Type**: Debug/Release configurations
- **Toolchain**: MSVC with Clang-based IntelliSense
- **C++ Standard**: C++17
- **Qt Version**: 6.9.1 (MSVC 2022 64-bit)

## Directory Structure
- **Build Directory**: `build-ninja/`
- **Output Structure**:
  - `build-ninja/bin/Debug/` - Debug executables
  - `build-ninja/bin/Release/` - Release executables
  - `build-ninja/lib/Debug/` - Debug libraries
  - `build-ninja/lib/Release/` - Release libraries

## VS Code Configuration

### Debug Configurations
1. **Debug with LLDB**
   - Type: `lldb`
   - Program: `build-ninja/bin/Debug/machineController.exe`
   - Pre-launch task: `Build (Debug)`

2. **Debug with MSVC**
   - Type: `cppvsdbg`
   - Program: `build-ninja/bin/Debug/machineController.exe`
   - Pre-launch task: `Build (Debug)`

3. **Release with MSVC**
   - Type: `cppvsdbg`
   - Program: `build-ninja/bin/Release/machineController.exe`
   - Pre-launch task: `Build (Release)`

### Build Tasks
- **Build (Debug)**: Builds the Debug configuration
- **Build (Release)**: Builds the Release configuration
- **Clean**: Cleans the build directory

## Key Features
- **Parallel Builds**: Utilizes Ninja's fast build system
- **IntelliSense Support**: `compile_commands.json` is generated for Clang-based IntelliSense
- **Qt Integration**: Properly configured for Qt 6.9.1
- **Multi-configuration**: Supports both Debug and Release builds
- **External Terminal**: Debug output appears in an external console window

## Build Commands
```bash
# Configure (from project root)
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.9.1\msvc2022_64

# Build Debug
cmake --build build-ninja --config Debug

# Build Release
cmake --build build-ninja --config Release

# Clean
cmake --build build-ninja --target clean
```

## Notes
- The build system automatically creates necessary output directories
- Debug and Release configurations are built to separate directories
- The project is set up for both LLDB and MSVC debuggers
- Qt UI files are automatically processed by CMake (MOC, UIC, RCC)
- `compile_commands.json` is automatically generated in the build directory for IDE integration
