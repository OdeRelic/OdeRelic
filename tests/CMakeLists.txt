cmake_minimum_required(VERSION 3.16)

# Test executable
add_executable(OplLibraryServiceTests
    tst_opllibraryservice.cpp
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

# Register with CTest
add_test(NAME OplLibraryServiceTests COMMAND OplLibraryServiceTests)
