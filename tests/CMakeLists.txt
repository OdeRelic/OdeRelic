cmake_minimum_required(VERSION 3.16)

# Test executable
add_executable(OplLibraryServiceTests
    tst_opllibraryservice.cpp
    ../src-cpp/core/filesystem_cache.cpp
    ../src-cpp/core/filesystem_cache.h
    ../src-cpp/core/opl_library_service.cpp
    ../src-cpp/core/opl_library_service.h
    ../src-cpp/core/system_utils.cpp
    ../src-cpp/core/system_utils.h
)

# Swiss Test executable
add_executable(SwissLibraryServiceTests
    tst_swisslibraryservice.cpp
    ../src-cpp/core/swiss_library_service.cpp
    ../src-cpp/core/swiss_library_service.h
    ../src-cpp/core/filesystem_cache.cpp
    ../src-cpp/core/filesystem_cache.h
    ../src-cpp/core/rvz_native_converter.cpp
    ../src-cpp/core/rvz_native_converter.h
    ../src-cpp/core/system_utils.cpp
    ../src-cpp/core/system_utils.h
)

# Test dependencies
target_link_libraries(OplLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

target_include_directories(OplLibraryServiceTests PRIVATE ../src-cpp/core)

target_link_libraries(SwissLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)
target_link_libraries(SwissLibraryServiceTests PRIVATE libzstd_static)

target_include_directories(SwissLibraryServiceTests PRIVATE ../src-cpp/core)

# FileSystemCache Component Testing Let
add_executable(FileSystemCacheTests
    tst_filesystemcache.cpp
    ../src-cpp/core/filesystem_cache.cpp
    ../src-cpp/core/filesystem_cache.h
)

target_link_libraries(FileSystemCacheTests PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(FileSystemCacheTests PRIVATE ../src-cpp/core)

# Register with CTest
add_test(NAME OplLibraryServiceTests COMMAND OplLibraryServiceTests)
add_test(NAME FileSystemCacheTests COMMAND FileSystemCacheTests)
add_test(NAME SwissLibraryServiceTests COMMAND SwissLibraryServiceTests)

# SystemUtils Component Testing
add_executable(SystemUtilsTests
    tst_systemutils.cpp
    ../src-cpp/core/system_utils.cpp
    ../src-cpp/core/system_utils.h
)

target_link_libraries(SystemUtilsTests PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(SystemUtilsTests PRIVATE ../src-cpp/core)
add_test(NAME SystemUtilsTests COMMAND SystemUtilsTests)

# XStation Test executable
add_executable(XStationLibraryServiceTests
    test_xstation.cpp
    ../src-cpp/core/ps1_xstation_library_service.cpp
    ../src-cpp/core/ps1_xstation_library_service.h
    ../src-cpp/core/system_utils.cpp
    ../src-cpp/core/system_utils.h
)

target_link_libraries(XStationLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Network
)

target_include_directories(XStationLibraryServiceTests PRIVATE ../src-cpp/core)
add_test(NAME XStationLibraryServiceTests COMMAND XStationLibraryServiceTests)
