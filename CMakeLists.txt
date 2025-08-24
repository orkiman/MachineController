cmake_minimum_required(VERSION 3.15)
project(MachineController LANGUAGES CXX)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Enable compile_commands.json (used by clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)

# Make sure the output directories exist
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Debug)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Release)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Enable Qt UI/MOC/RCC auto-processing
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# UI files
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/src/gui)
set(UI_FILES
    src/gui/MainWindow.ui
    src/gui/SettingsWindow.ui
)

qt6_wrap_ui(UI_HEADERS ${UI_FILES})
qt6_wrap_cpp(GENERATED_MOC_SOURCES include/gui/MainWindow.h)
qt6_wrap_cpp(GENERATED_MOC_SOURCES include/gui/SettingsWindow.h)

# Sources
set(SOURCES
    src/main.cpp
    src/io/windows/PCI7248IO.cpp
    src/Config.cpp
    src/Logic.cpp
    src/machine/DefaultMachineCore.cpp
    src/gui/MainWindow.cpp
    src/gui/SettingsWindow.cpp
    src/communication/RS232Communication.cpp
    src/communication/ArduinoProtocol.cpp
    src/dataFile/DataFile.cpp
    ${GENERATED_MOC_SOURCES}
    ${UI_HEADERS}
)

add_executable(MachineController ${SOURCES})
set_target_properties(MachineController PROPERTIES OUTPUT_NAME "machineController")

# Includes
target_include_directories(MachineController PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/nlohmann
    "C:/ADLINK/DASK/Include"
)

# spdlog
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(spdlog)

# Warnings and macros
add_definitions(
    -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    -DNOMINMAX
)

# DASK library directory
link_directories("C:/ADLINK/DASK/Lib")

# Link libraries
target_link_libraries(MachineController PRIVATE
    "C:/ADLINK/DASK/Lib/PCI-Dask64.lib"
    winmm
    spdlog::spdlog
    Qt6::Widgets
)

# MSVC-specific flags
if(MSVC)
    target_compile_options(MachineController PRIVATE
        $<$<CONFIG:Debug>:/W4 /Zi /RTC1>
        $<$<CONFIG:Release>:/W4 /O2>
    )
endif()

# Deploy Qt DLLs on Windows after build
if(WIN32)
    add_custom_command(TARGET MachineController POST_BUILD
        COMMAND "C:/Qt/6.9.1/msvc2022_64/bin/windeployqt.exe" "$<TARGET_FILE:MachineController>"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:MachineController>"
        COMMENT "Deploying Qt dependencies with windeployqt..."
    )
endif()
