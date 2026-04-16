cmake_minimum_required(VERSION 3.16)

# Test executable
add_executable(OplLibraryServiceTests
    cpp/tst_opllibraryservice.cpp
    ../src-cpp/core/filesystem/filesystem_cache.cpp
    ../src-cpp/core/filesystem/filesystem_cache.h
    ../src-cpp/platforms/ps2/opl_library_service.cpp
    ../src-cpp/platforms/ps2/opl_library_service.h
    ../src-cpp/core/common/system_utils.cpp
    ../src-cpp/core/common/system_utils.h
)

# Swiss Test executable
add_executable(SwissLibraryServiceTests
    cpp/tst_swisslibraryservice.cpp
    ../src-cpp/platforms/ngc/swiss_library_service.cpp
    ../src-cpp/platforms/ngc/swiss_library_service.h
    ../src-cpp/core/filesystem/filesystem_cache.cpp
    ../src-cpp/core/filesystem/filesystem_cache.h
    ../src-cpp/platforms/ngc/rvz_native_converter.cpp
    ../src-cpp/platforms/ngc/rvz_native_converter.h
    ../src-cpp/core/common/system_utils.cpp
    ../src-cpp/core/common/system_utils.h
)

# Test dependencies
target_link_libraries(OplLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(OplLibraryServiceTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)

target_link_libraries(SwissLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Concurrent
)
target_link_libraries(SwissLibraryServiceTests PRIVATE libzstd_static)

target_include_directories(SwissLibraryServiceTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)

# FileSystemCache Component Testing Let
add_executable(FileSystemCacheTests
    cpp/tst_filesystemcache.cpp
    ../src-cpp/core/filesystem/filesystem_cache.cpp
    ../src-cpp/core/filesystem/filesystem_cache.h
)

target_link_libraries(FileSystemCacheTests PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(FileSystemCacheTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)

# Register with CTest
add_test(NAME OplLibraryServiceTests COMMAND OplLibraryServiceTests)
add_test(NAME FileSystemCacheTests COMMAND FileSystemCacheTests)
add_test(NAME SwissLibraryServiceTests COMMAND SwissLibraryServiceTests)

# SystemUtils Component Testing
add_executable(SystemUtilsTests
    cpp/tst_systemutils.cpp
    ../src-cpp/core/common/system_utils.cpp
    ../src-cpp/core/common/system_utils.h
)

target_link_libraries(SystemUtilsTests PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(SystemUtilsTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)
add_test(NAME SystemUtilsTests COMMAND SystemUtilsTests)

# XStation Test executable
add_executable(XStationLibraryServiceTests
    cpp/test_xstation.cpp
    ../src-cpp/platforms/psx/ps1_xstation_library_service.cpp
    ../src-cpp/platforms/psx/ps1_xstation_library_service.h
    ../src-cpp/core/common/system_utils.cpp
    ../src-cpp/core/common/system_utils.h
)

target_link_libraries(XStationLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(XStationLibraryServiceTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)
add_test(NAME XStationLibraryServiceTests COMMAND XStationLibraryServiceTests)

# RVZ Native Converter Tests
add_executable(RvzNativeConverterTests
    cpp/tst_rvznativeconverter.cpp
    ../src-cpp/platforms/ngc/rvz_native_converter.cpp
    ../src-cpp/platforms/ngc/rvz_native_converter.h
)
target_link_libraries(RvzNativeConverterTests PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(RvzNativeConverterTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)
add_test(NAME RvzNativeConverterTests COMMAND RvzNativeConverterTests)

# Translation Manager Tests
add_executable(TranslationManagerTests
    cpp/tst_translationmanager.cpp
    ../src-cpp/core/i18n/translation_manager.cpp
    ../src-cpp/core/i18n/translation_manager.h
)
target_link_libraries(TranslationManagerTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
)
target_include_directories(TranslationManagerTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx)
add_test(NAME TranslationManagerTests COMMAND TranslationManagerTests)

# Logger Tests
add_executable(LoggerTests
    cpp/tst_logger.cpp
    ../src-cpp/core/logging/logger.cpp
    ../src-cpp/core/logging/logger.h
)
target_link_libraries(LoggerTests PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(LoggerTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx ../src-cpp/platforms/dreamcast)
add_test(NAME LoggerTests COMMAND LoggerTests)

# Dreamcast Library Service Tests
add_executable(DreamcastLibraryServiceTests
    cpp/test_dreamcast_library_service.cpp
    ../src-cpp/platforms/dreamcast/dreamcast_library_service.cpp
    ../src-cpp/platforms/dreamcast/dreamcast_library_service.h
    ../src-cpp/core/filesystem/filesystem_cache.cpp
    ../src-cpp/core/filesystem/filesystem_cache.h
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.h
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.h
    ../src-cpp/core/common/system_utils.cpp
    ../src-cpp/core/common/system_utils.h
    ../src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.h
)
target_link_libraries(DreamcastLibraryServiceTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Network
    Qt6::Concurrent
    archive_static
)
target_include_directories(DreamcastLibraryServiceTests PRIVATE ../src-cpp/core ../src-cpp/core/filesystem ../src-cpp/core/common ../src-cpp/core/i18n ../src-cpp/core/logging ../src-cpp/platforms/ps2 ../src-cpp/platforms/ngc ../src-cpp/platforms/psx ../src-cpp/platforms/dreamcast ${libarchive_SOURCE_DIR}/libarchive)
add_test(NAME DreamcastLibraryServiceTests COMMAND DreamcastLibraryServiceTests)

# Gdrom Builder Tests
add_executable(GdromBuilderTests
    cpp/test_gdrom_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/gdrom_builder.h
    ../src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.h
)
target_link_libraries(GdromBuilderTests PRIVATE Qt6::Core Qt6::Test archive_static)
target_include_directories(GdromBuilderTests PRIVATE ../src-cpp/core ../src-cpp/platforms/dreamcast ../src-cpp/platforms/dreamcast/GDRomUtils ${libarchive_SOURCE_DIR}/libarchive)
add_test(NAME GdromBuilderTests COMMAND GdromBuilderTests)

# Dreamcast Iso Builder Tests
add_executable(DreamcastIsoBuilderTests
    cpp/test_dreamcast_iso_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.cpp
    ../src-cpp/platforms/dreamcast/GDRomUtils/dreamcast_iso_builder.h
)
target_link_libraries(DreamcastIsoBuilderTests PRIVATE Qt6::Core Qt6::Test)
target_include_directories(DreamcastIsoBuilderTests PRIVATE ../src-cpp/core ../src-cpp/platforms/dreamcast ../src-cpp/platforms/dreamcast/GDRomUtils)
add_test(NAME DreamcastIsoBuilderTests COMMAND DreamcastIsoBuilderTests)

# OpenMenu DAT Manager Tests
add_executable(OpenMenuDatManagerTests
    cpp/test_openmenu_dat_manager.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.h
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.h
)
target_link_libraries(OpenMenuDatManagerTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(OpenMenuDatManagerTests PRIVATE ../src-cpp/core ../src-cpp/platforms/dreamcast ../src-cpp/platforms/dreamcast/ArtNDataUtils)
add_test(NAME OpenMenuDatManagerTests COMMAND OpenMenuDatManagerTests)

# OpenMenu Image Provider Tests
add_executable(OpenMenuImageProviderTests
    cpp/test_openmenu_image_provider.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_image_provider.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_image_provider.h
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.h
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.h
)
target_link_libraries(OpenMenuImageProviderTests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_include_directories(OpenMenuImageProviderTests PRIVATE ../src-cpp/core ../src-cpp/platforms/dreamcast ../src-cpp/platforms/dreamcast/ArtNDataUtils)
add_test(NAME OpenMenuImageProviderTests COMMAND OpenMenuImageProviderTests)

# PVR Texture Coder Tests
add_executable(PvrTextureCoderTests
    cpp/test_pvr_texture_coder.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.cpp
    ../src-cpp/platforms/dreamcast/ArtNDataUtils/pvr_texture_coder.h
)
target_link_libraries(PvrTextureCoderTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(PvrTextureCoderTests PRIVATE ../src-cpp/core ../src-cpp/platforms/dreamcast ../src-cpp/platforms/dreamcast/ArtNDataUtils)
add_test(NAME PvrTextureCoderTests COMMAND PvrTextureCoderTests)

# QML UI Tests
add_executable(OdeRelicUITests
    cpp/tst_qmlui.cpp
)
target_link_libraries(OdeRelicUITests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTest
)
target_compile_definitions(OdeRelicUITests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
add_test(NAME OdeRelicUITests COMMAND OdeRelicUITests)
