cmake_minimum_required(VERSION 3.10)
project(MachineController)

# Set C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/windows/PCI7248IO.cpp
    ${CMAKE_SOURCE_DIR}/src/Config.cpp
)

# Create the executable target named MachineController
add_executable(MachineController ${SOURCES})

# Set the final executable output name to "machineController.exe"
set_target_properties(MachineController PROPERTIES OUTPUT_NAME "machineController")

# Add include directories
target_include_directories(MachineController PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/nlohmann
    "C:/ADLINK/DASK/Include"
)

# Specify the library directory for ADLINK DASK libraries
link_directories("C:/ADLINK/DASK/Lib")

# Link with the PCI-Dask64 library using its full path
target_link_libraries(MachineController PRIVATE "C:/ADLINK/DASK/Lib/PCI-Dask64.lib")

# For GNU compilers on Windows, add static linking flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(MachineController PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

# Optionally add debug symbols for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()


