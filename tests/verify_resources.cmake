# verify_resources.cmake
# This script runs during ctest to verify the resources are correctly structured and installable.

message(STATUS "Verifying source resources directory...")
if(NOT IS_DIRECTORY "${SOURCE_DIR}/resources")
    message(FATAL_ERROR "Resources directory not found at: ${SOURCE_DIR}/resources")
endif()

set(REQUIRED_FILES
    "resources/dreamcast/openmenu/BOX.DAT"
    "resources/dreamcast/openmenu/ICON.DAT"
    "resources/dreamcast/openmenu/META.DAT"
)

foreach(FILE ${REQUIRED_FILES})
    if(NOT EXISTS "${SOURCE_DIR}/${FILE}")
        message(FATAL_ERROR "Required database file is missing: ${SOURCE_DIR}/${FILE}")
    endif()
    message(STATUS "Found: ${FILE}")
endforeach()

# Let's verify the installation layout by performing a test install
message(STATUS "Performing test install...")
set(TEST_INSTALL_DIR "${BINARY_DIR}/test_install_validate")

# Run cmake --install
execute_process(
    COMMAND ${CMAKE_COMMAND} --install ${BINARY_DIR} --prefix ${TEST_INSTALL_DIR}
    RESULT_VARIABLE INSTALL_RESULT
    OUTPUT_VARIABLE INSTALL_OUTPUT
    ERROR_VARIABLE INSTALL_ERROR
)

if(NOT INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to run test install:\nResult: ${INSTALL_RESULT}\nOutput: ${INSTALL_OUTPUT}\nError: ${INSTALL_ERROR}")
endif()

# Check that the installed resource folder has the correct files based on platform
if(APPLE)
    set(INSTALLED_BOX_DAT "${TEST_INSTALL_DIR}/OdeRelic.app/Contents/resources/dreamcast/openmenu/BOX.DAT")
else()
    set(INSTALLED_BOX_DAT "${TEST_INSTALL_DIR}/resources/dreamcast/openmenu/BOX.DAT")
endif()

if(NOT EXISTS "${INSTALLED_BOX_DAT}")
    # Cleanup before failing
    file(REMOVE_RECURSE "${TEST_INSTALL_DIR}")
    message(FATAL_ERROR "Validation failed: Installed database file not found at: ${INSTALLED_BOX_DAT}")
endif()

message(STATUS "Installed database file correctly verified at: ${INSTALLED_BOX_DAT}")

# Cleanup
file(REMOVE_RECURSE "${TEST_INSTALL_DIR}")
message(STATUS "Validation test install completed successfully and cleaned up.")
