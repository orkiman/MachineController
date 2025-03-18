cmake_minimum_required(VERSION 3.15)
project(MachineController LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ Find Qt Widgets
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Automatic handling of Qt UI files and moc
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

# Define source files
set(SOURCES
    src/main.cpp
    src/io/windows/PCI7248IO.cpp
    src/Config.cpp
    src/Logic.cpp
    src/gui/MainWindow.cpp  # GUI cpp added
    src/gui/MainWindow.ui   # GUI ui added
)

# Include FetchContent for external libraries
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

# Suppress spdlog deprecation warnings
add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

# Create the executable target
add_executable(MachineController ${SOURCES})

# Set executable output name
set_target_properties(MachineController PROPERTIES OUTPUT_NAME "machineController")

# Add include directories
target_include_directories(MachineController PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/nlohmann
    "C:/ADLINK/DASK/Include"
)

# Specify library directory for ADLINK DASK
link_directories("C:/ADLINK/DASK/Lib")

# Link with required libraries
target_link_libraries(MachineController PRIVATE
    "C:/ADLINK/DASK/Lib/PCI-Dask64.lib"
    winmm
    spdlog::spdlog
    Qt6::Widgets
)

# MSVC-specific optimizations and warnings
if(MSVC)
    message(STATUS "MSVC Compiler detected")
    target_compile_options(MachineController PRIVATE
        $<$<CONFIG:Debug>:/W4>
        $<$<CONFIG:Debug>:/RTC1>
        $<$<CONFIG:Release>:/W4>
        $<$<CONFIG:Release>:/O2>
    )
endif()

# Debug symbols for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
endif()

# Definitions
add_definitions(-DNOMINMAX)
