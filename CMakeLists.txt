cmake_minimum_required(VERSION 3.15)
project(MachineController LANGUAGES CXX)

# ✅ Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ Find Qt Widgets
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ✅ Enable automatic processing for Qt UI, MOC, and RCC
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ✅ Ensure CMake knows where to find .ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/src/gui)

# ✅ Ensure `MainWindow.h` is processed for Qt Meta-Object System (MOC)
qt6_wrap_cpp(GENERATED_MOC_SOURCES include/gui/MainWindow.h)

# ✅ Ensure `MainWindow.ui` is processed for Qt UI generation
qt6_wrap_ui(GENERATED_UI_HEADERS src/gui/MainWindow.ui)

# ✅ Define source files
set(SOURCES
    src/main.cpp
    src/io/windows/PCI7248IO.cpp
    src/Config.cpp
    src/Logic.cpp
    src/gui/MainWindow.cpp
)

# ✅ Create the executable target (including auto-generated headers)
add_executable(MachineController 
    ${SOURCES} 
    ${GENERATED_MOC_SOURCES} 
    ${GENERATED_UI_HEADERS}
)

# ✅ Set executable output name
set_target_properties(MachineController PROPERTIES OUTPUT_NAME "machineController")

# ✅ Ensure UI headers and project includes are properly set
target_include_directories(MachineController PRIVATE
    ${CMAKE_BINARY_DIR}  # UI headers are auto-generated here
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/nlohmann
    "C:/ADLINK/DASK/Include"
)

# ✅ Include FetchContent for external libraries
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

# ✅ Suppress spdlog deprecation warnings
add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

# ✅ Specify library directory for ADLINK DASK
link_directories("C:/ADLINK/DASK/Lib")

# ✅ Link with required libraries
target_link_libraries(MachineController PRIVATE
    "C:/ADLINK/DASK/Lib/PCI-Dask64.lib"
    winmm
    spdlog::spdlog
    Qt6::Widgets
)

# ✅ MSVC-specific optimizations and warnings
if(MSVC)
    message(STATUS "MSVC Compiler detected")
    target_compile_options(MachineController PRIVATE
        $<$<CONFIG:Debug>:/W4>
        $<$<CONFIG:Debug>:/RTC1>
        $<$<CONFIG:Release>:/W4>
        $<$<CONFIG:Release>:/O2>
    )
endif()

# ✅ Debug symbols for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
endif()

# ✅ Additional compiler definitions
add_definitions(-DNOMINMAX)

# this is not good enough fix it
# ✅ Post-build tasks for Windows:
# if(WIN32)
#     add_custom_command(TARGET MachineController POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -DTARGET_PATH="$<TARGET_FILE_DIR:MachineController>" -P ${CMAKE_SOURCE_DIR}/postBuildTasks.cmake
#         COMMENT "Running post-build tasks (checking and deploying missing Qt dependencies)..."
#     )
# endif()


# this isnt working:
# qt_generate_deploy_app_script(
#     TARGET MachineController
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
# )
# install(SCRIPT ${deploy_script})
