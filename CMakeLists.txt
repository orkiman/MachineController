cmake_minimum_required(VERSION 3.10)
project(MachineController)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/windows/PCI7248IO.cpp
    ${CMAKE_SOURCE_DIR}/src/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/MainLogic.cpp
)

include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0  # Use the version you need
)
FetchContent_MakeAvailable(spdlog)
# supress spdlog deprication warnings
add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)


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

# ✅ Link with PCI-Dask64 and Winmm (needed for timeBeginPeriod)
target_link_libraries(MachineController PRIVATE "C:/ADLINK/DASK/Lib/PCI-Dask64.lib" winmm spdlog::spdlog)

# Remove MinGW-specific linking
# Remove:
# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
#     target_link_options(MachineController PRIVATE "-static-libgcc" "-static-libstdc++")
# endif()

# ✅ MSVC-specific optimizations
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
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi") # Use MSVC debug flags
endif()

add_definitions(-DNOMINMAX)