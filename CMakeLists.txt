set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Suppress obsolete Zstandard policies implicitly")
cmake_minimum_required(VERSION 3.16)
project(OdeRelic VERSION 0.5.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickTest Network Test LinguistTools Concurrent REQUIRED)

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

FetchContent_Declare(
  libarchive
  GIT_REPOSITORY https://github.com/libarchive/libarchive.git
  GIT_TAG        v3.7.2
)
set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
set(ENABLE_UNZIP OFF CACHE BOOL "" FORCE)
set(ENABLE_BZip2 OFF CACHE BOOL "" FORCE)
set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
set(ENABLE_LZO OFF CACHE BOOL "" FORCE)
set(ENABLE_LZ4 OFF CACHE BOOL "" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_ZLIB OFF CACHE BOOL "" FORCE)
set(ENABLE_CRYPTO OFF CACHE BOOL "" FORCE)
set(ENABLE_EXPAT OFF CACHE BOOL "" FORCE)
set(ENABLE_PCREPOSIX OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBB2 OFF CACHE BOOL "" FORCE)
set(ENABLE_CACA OFF CACHE BOOL "" FORCE)
set(ENABLE_ICONV OFF CACHE BOOL "" FORCE)
set(ENABLE_ACL OFF CACHE BOOL "" FORCE)
set(ENABLE_XATTR OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libarchive)

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
    src-cpp/core/filesystem/filesystem_cache.cpp
    src-cpp/core/filesystem/filesystem_cache.h
    src-cpp/platforms/ps2/opl_library_service.cpp
    src-cpp/platforms/ps2/opl_library_service.h
    src-cpp/platforms/ngc/swiss_library_service.cpp
    src-cpp/platforms/ngc/swiss_library_service.h
    src-cpp/platforms/psx/ps1_xstation_library_service.cpp
    src-cpp/platforms/psx/ps1_xstation_library_service.h
    src-cpp/platforms/dreamcast/dreamcast_library_service.cpp
    src-cpp/platforms/dreamcast/dreamcast_library_service.h
    src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.cpp
    src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.h
    src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_image_provider.cpp
    src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_image_provider.h
    src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.cpp
    src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.cpp
    src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.h
    src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.cpp
    src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.h
    src-cpp/platforms/ngc/rvz_native_converter.cpp
    src-cpp/platforms/ngc/rvz_native_converter.h
    src-cpp/core/i18n/translation_manager.cpp
    src-cpp/core/i18n/translation_manager.h
    src-cpp/core/common/system_utils.cpp
    src-cpp/core/logging/logger.cpp
    src-cpp/qml.qrc
    src-cpp/HomeScreen.qml
    src-cpp/platforms/ps2/OplManager.qml
    src-cpp/platforms/ngc/SwissManager.qml
    src-cpp/platforms/psx/PS1Manager.qml
    src-cpp/platforms/dreamcast/DreamcastManager.qml
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
    Qt6::Concurrent
)

target_link_libraries(OdeRelic PRIVATE libzstd_static archive_static)
target_include_directories(OdeRelic PRIVATE ${libarchive_SOURCE_DIR}/libarchive)

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

if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(MAC_ARCH "arm64")
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(MAC_ARCH "intel")
    else()
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(MAC_ARCH "arm64")
        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(MAC_ARCH "intel")
        else()
            set(MAC_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
        endif()
    endif()

    if(BUILD_NUMBER)
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-b${BUILD_NUMBER}-${CMAKE_SYSTEM_NAME}-${MAC_ARCH}")
    else()
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${MAC_ARCH}")
    endif()
else()
    if(BUILD_NUMBER)
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-b${BUILD_NUMBER}-${CMAKE_SYSTEM_NAME}")
    else()
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
    endif()
endif()

if(WIN32)
    set(CPACK_GENERATOR "ZIP") # Safe, frictionless fallback generating native zip on Windows without needing NSIS natively installed on base github actions runners
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
endif()

include(CPack)

