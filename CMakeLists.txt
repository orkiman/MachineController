cmake_minimum_required(VERSION 3.10)
project(MachineController)

# Set C++ standard to C++14 (or adjust as needed)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add project include directories
include_directories(${CMAKE_SOURCE_DIR}/include "C:/ADLINK/DASK/Include")

# Platform-specific source files
if(WIN32)
    set(SOURCES
        ${CMAKE_SOURCE_DIR}/src/main.cpp
        ${CMAKE_SOURCE_DIR}/src/io/windows/PCI7248IO.cpp
    )
else()
    message(FATAL_ERROR "This project is configured for Windows only.")
endif()

# Create the executable target
add_executable(MachineController ${SOURCES})

# Specify the library directory for the ADLINK DASK libraries
link_directories("C:/ADLINK/DASK/Lib")

# Link the executable with the PCI-Dask64 library
target_link_libraries(MachineController PRIVATE "PCI-Dask64.lib")

# Optionally, if you're using MinGW and want static linking for the standard libraries:
if(MINGW)
    target_link_options(MachineController PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

# Add debugging symbols (if building in Debug configuration)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
