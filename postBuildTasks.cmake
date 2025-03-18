# Ensure TARGET_PATH is set
if(NOT DEFINED TARGET_PATH OR TARGET_PATH STREQUAL "")
    message(FATAL_ERROR "TARGET_PATH is not defined or is empty.")
endif()

# Set windeployqt path explicitly
set(WINDEPLOYQT_PATH "C:/Qt/6.8.2/msvc2022_64/bin/windeployqt.exe")

# Ensure windeployqt.exe exists
if(NOT EXISTS "${WINDEPLOYQT_PATH}")
    message(FATAL_ERROR "windeployqt.exe not found at: ${WINDEPLOYQT_PATH}")
endif()

# Define the Qt DLL to check
set(QT_DLL "Qt6Widgets.dll")
set(EXECUTABLE_PATH "${TARGET_PATH}")

# Check if Qt DLLs already exist
if(NOT EXISTS "${EXECUTABLE_PATH}/${QT_DLL}")
    message(STATUS "Qt DLLs are missing, running windeployqt...")
    execute_process(
        COMMAND "${WINDEPLOYQT_PATH}" --release "${EXECUTABLE_PATH}/machineController.exe"
        WORKING_DIRECTORY "${EXECUTABLE_PATH}"
        RESULT_VARIABLE DEPLOY_RESULT
    )
    if(DEPLOY_RESULT AND NOT DEPLOY_RESULT EQUAL 0)
        message(FATAL_ERROR "windeployqt failed with error code: ${DEPLOY_RESULT}")
    endif()
else()
    message(STATUS "Qt DLLs already exist, skipping windeployqt.")
endif()
