set(CMAKE_POLICY_VERSION_MINIMUM 3.0 CACHE STRING "Suppress obsolete Zstandard policies implicitly")
cmake_minimum_required(VERSION 3.16)
project(OdeRelic VERSION 0.5.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick Network Test LinguistTools REQUIRED)

include(FetchContent)
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG        v1.5.6
  SOURCE_SUBDIR  build/cmake
)
# Force static build configuration securely
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Disable shared zstd" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "Enable static zstd" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Disable zstd CLI tools" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "Disable zstd tests" FORCE)


FetchContent_MakeAvailable(zstd)

enable_testing()

option(ENABLE_COVERAGE "Enable C++ GCC/Clang code coverage analysis profiles" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    message(STATUS "Code coverage instrumentation dynamically enabled.")
endif()
qt_add_executable(OdeRelic WIN32
    src-cpp/main.cpp
    src-cpp/core/filesystem_cache.cpp
    src-cpp/core/filesystem_cache.h
    src-cpp/core/opl_library_service.cpp
    src-cpp/core/opl_library_service.h
    src-cpp/core/swiss_library_service.cpp
    src-cpp/core/swiss_library_service.h
    src-cpp/core/ps1_xstation_library_service.cpp
    src-cpp/core/ps1_xstation_library_service.h
    src-cpp/core/rvz_native_converter.cpp
    src-cpp/core/rvz_native_converter.h
    src-cpp/core/translation_manager.cpp
    src-cpp/core/translation_manager.h
    src-cpp/core/system_utils.cpp
    src-cpp/core/logger.cpp
    src-cpp/qml.qrc
    src-cpp/HomeScreen.qml
    src-cpp/OplManager.qml
    src-cpp/SwissManager.qml
    src-cpp/PS1Manager.qml
)

qt_add_translations(OdeRelic
    TS_FILES
        i18n/OdeRelic_en.ts
        i18n/OdeRelic_pt_BR.ts
        i18n/OdeRelic_de.ts
        i18n/OdeRelic_ja.ts
    RESOURCE_PREFIX "/i18n"
)

target_link_libraries(OdeRelic PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
)

target_link_libraries(OdeRelic PRIVATE libzstd_static)

# Application metadata
if(APPLE)
    set_target_properties(OdeRelic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.oderelic.app
        MACOSX_BUNDLE_BUNDLE_NAME "OdeRelic"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE "icon_512x512.icns"
    )
    set(app_icon_macos "${CMAKE_CURRENT_SOURCE_DIR}/assets/applogo/icon_512x512.icns")
    set_source_files_properties(${app_icon_macos} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(OdeRelic PRIVATE ${app_icon_macos})
elseif(WIN32)
    target_sources(OdeRelic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets/applogo/app_icon.rc")
endif()

add_subdirectory(tests)

# --- Installation and CPack Directives ---

install(TARGETS OdeRelic
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

set(CPACK_PACKAGE_NAME "OdeRelic")
set(CPACK_PACKAGE_VENDOR "OdeRelic")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OdeRelic native runtime")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

if(BUILD_NUMBER)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-b${BUILD_NUMBER}-${CMAKE_SYSTEM_NAME}")
else()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
endif()

if(WIN32)
    set(CPACK_GENERATOR "ZIP") # Safe, frictionless fallback generating native zip on Windows without needing NSIS natively installed on base github actions runners
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
endif()

include(CPack)

qt_add_executable(TestXStation tests/test_xstation.cpp src-cpp/core/ps1_xstation_library_service.cpp src-cpp/core/system_utils.cpp src-cpp/core/logger.cpp)
target_link_libraries(TestXStation PRIVATE Qt6::Core Qt6::Gui Qt6::Test Qt6::Network)
target_include_directories(TestXStation PRIVATE src-cpp/core)
add_test(NAME test_xstation COMMAND TestXStation)
