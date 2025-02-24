cmake_minimum_required(VERSION 3.10)
project(PCI7224Program)

# Set C++ standard to C++14 (adjust if needed)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR}/include "C:/ADLINK/DASK/Include")

# Define source files (taken from your tasks.json)
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/io/windows/PCI7248IO.cpp
    ${CMAKE_SOURCE_DIR}/src/main.cpp
)

# Create the executable target; output will be pci7224_main.exe
add_executable(pci7224_main ${SOURCES})
set_target_properties(pci7224_main PROPERTIES OUTPUT_NAME "pci7224_main")

# Specify the library directory for ADLINK DASK libraries
link_directories("C:/ADLINK/DASK/Lib")

# Link with the PCI-Dask64 library (using full path to ensure linking)
target_link_libraries(pci7224_main PRIVATE "C:/ADLINK/DASK/Lib/PCI-Dask64.lib")

# For GNU compilers on Windows, add static linking flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_options(pci7224_main PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

# Add debugging symbols for Debug builds
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
