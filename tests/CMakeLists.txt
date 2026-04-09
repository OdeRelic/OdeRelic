cmake_minimum_required(VERSION 3.16)

# Test executable
add_executable(OplLibraryServiceTests
    tst_opllibraryservice.cpp
    ../src-cpp/core/filesystem_cache.cpp
    ../src-cpp/core/filesystem_cache.h
    ../src-cpp/core/opl_library_service.cpp
    ../src-cpp/core/opl_library_service.h
)

# Test dependencies
target_link_libraries(OplLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
)

target_include_directories(OplLibraryServiceTests PRIVATE ../src-cpp/core)

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
