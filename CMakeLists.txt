cmake_minimum_required(VERSION 3.28)
project(Sonora VERSION 1.0.0 LANGUAGES C CXX)

option(SONORA_BUILD_TESTS "Build automated unit tests" ON)
option(SONORA_ENABLE_ASAN "Enable AddressSanitizer/LeakSanitizer (Linux/Clang/GCC)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    QuickDialogs2
    LinguistTools
)

if(SONORA_BUILD_TESTS)
    find_package(Qt6 6.9 REQUIRED COMPONENTS Test)
endif()

find_package(FFmpeg REQUIRED)

qt_standard_project_setup(
    REQUIRES 6.9
    I18N_SOURCE_LANGUAGE en
    I18N_TRANSLATED_LANGUAGES zh_CN en_US
)

if(SONORA_ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---- Third-party lightweight static libraries ----
add_library(sonora_miniaudio STATIC
    third_party/miniaudio/miniaudio_impl.c
)
target_include_directories(sonora_miniaudio PUBLIC third_party/miniaudio)
if(UNIX AND NOT APPLE)
    target_link_libraries(sonora_miniaudio PUBLIC dl pthread m)
elseif(WIN32)
    target_link_libraries(sonora_miniaudio PUBLIC ole32 uuid)
endif()

add_library(sonora_sqlite3 STATIC
    third_party/sqlite3/sqlite3.c
)
target_include_directories(sonora_sqlite3 PUBLIC third_party/sqlite3)
target_compile_definitions(sonora_sqlite3 PRIVATE
    SQLITE_ENABLE_FTS5=1
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_DEPRECATED=1
)
if(UNIX AND NOT APPLE)
    target_link_libraries(sonora_sqlite3 PUBLIC dl pthread m)
endif()

# ---- Core C++ library (shared by app and automated unit tests) ----
set(SONORA_CORE_SOURCES
    src/audio/AudioTypes.h
    src/audio/AudioRingBuffer.h
    src/audio/FFmpegDecoder.h
    src/audio/FFmpegDecoder.cpp
    src/audio/IAudioOutput.h
    src/audio/MiniAudioOutput.h
    src/audio/MiniAudioOutput.cpp
    src/audio/PlaybackEngine.h
    src/audio/PlaybackEngine.cpp
    src/audio/dsp/BiquadEq.h
    src/audio/dsp/BiquadEq.cpp
    src/audio/dsp/ReplayGainProcessor.h
    src/audio/dsp/FftAnalyzer.h
    src/audio/dsp/FftAnalyzer.cpp
    src/core/TextEncodingHelper.h
    src/core/TextEncodingHelper.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/MemoryStats.h
    src/core/ThemeManager.h
    src/core/ThemeManager.cpp
    src/core/PlaybackController.h
    src/core/PlaybackController.cpp
    src/library/CueParser.h
    src/library/CueParser.cpp
    src/library/M3uPlaylistIO.h
    src/library/M3uPlaylistIO.cpp
    src/library/LibraryDatabase.h
    src/library/LibraryDatabase.cpp
    src/library/CoverArtProvider.h
    src/library/CoverArtProvider.cpp
    src/library/LibraryScanner.h
    src/library/LibraryScanner.cpp
    src/lyrics/LyricsParser.h
    src/lyrics/LyricsParser.cpp
    src/lyrics/LyricsController.h
    src/lyrics/LyricsController.cpp
    src/models/LibraryModels.h
    src/models/LibraryModels.cpp
    src/ui/SpectrumVisualizerItem.h
    src/ui/SpectrumVisualizerItem.cpp
    src/ui/EqCurveItem.h
    src/ui/EqCurveItem.cpp
    src/platform/PlatformBridge.h
    src/platform/PlatformBridge.cpp
)

if(WIN32)
    list(APPEND SONORA_CORE_SOURCES
        src/platform/win32/Win32WindowHelper.h
        src/platform/win32/Win32WindowHelper.cpp
        src/platform/win32/Win32TrayIcon.h
        src/platform/win32/Win32TrayIcon.cpp
        src/platform/win32/Win32GlobalHotkey.h
        src/platform/win32/Win32GlobalHotkey.cpp
        src/platform/win32/Win32SmtcController.h
        src/platform/win32/Win32SmtcController.cpp
    )
endif()

add_library(sonora_core STATIC ${SONORA_CORE_SOURCES})
target_include_directories(sonora_core PUBLIC src)
target_link_libraries(sonora_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swresample
    sonora_miniaudio
    sonora_sqlite3
)

if(WIN32)
    target_link_libraries(sonora_core PUBLIC
        dwmapi
        shell32
        user32
        psapi
        ole32
    )
    if(MSVC)
        target_link_libraries(sonora_core PUBLIC runtimeobject)
    endif()
    target_compile_definitions(sonora_core PUBLIC UNICODE _UNICODE NOMINMAX)
    set_target_properties(sonora_core PROPERTIES AUTOMOC_MOC_OPTIONS "-D_WIN32")
endif()

if(MSVC)
    target_compile_options(sonora_core PRIVATE /W4 /wd4324 /permissive- /utf-8)
else()
    target_compile_options(sonora_core PRIVATE -Wall -Wextra)
endif()

# ---- Application executable + QML module ----
set(SONORA_APP_SOURCES src/main.cpp)
if(WIN32)
    list(APPEND SONORA_APP_SOURCES resources/sonora.rc)
    set_property(SOURCE resources/sonora.rc APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/sonora.ico"
    )
endif()

qt_add_executable(sonora
    ${SONORA_APP_SOURCES}
)

qt_add_resources(sonora "sonora_app_icon"
    PREFIX "/"
    FILES resources/sonora.ico
)

set_source_files_properties(qml/theme/DesignTokens.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(sonora
    URI Sonora.UI
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/DesignTokens.qml
        qml/components/IconGlyph.qml
        qml/components/AmbientBackdrop.qml
        qml/components/FormatBadge.qml
        qml/components/GlassPanel.qml
        qml/components/SonoraButton.qml
        qml/components/SonoraSwitch.qml
        qml/components/SonoraSlider.qml
        qml/components/SonoraComboBox.qml
        qml/components/NowPlayingBars.qml
        qml/components/TitleBar.qml
        qml/components/Sidebar.qml
        qml/components/PlaybackBar.qml
        qml/components/QueueDrawer.qml
        qml/components/TrackInfoDialog.qml
        qml/views/TracksView.qml
        qml/views/AlbumsView.qml
        qml/views/ArtistsView.qml
        qml/views/FoldersView.qml
        qml/views/NowPlayingOverlay.qml
        qml/views/EqDrawer.qml
        qml/views/SettingsView.qml
        qml/windows/MiniPlayerWindow.qml
)

qt_add_translations(sonora
    TS_FILES
        translations/sonora_zh_CN.ts
        translations/sonora_en_US.ts
    RESOURCE_PREFIX "/i18n"
)

target_link_libraries(sonora PRIVATE
    sonora_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
)

set_target_properties(sonora PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)
if(WIN32)
    set_target_properties(sonora PROPERTIES OUTPUT_NAME "Sonora")
endif()

if(UNIX AND NOT APPLE AND FFMPEG_ROOT)
    set_target_properties(sonora PROPERTIES
        BUILD_RPATH "${FFMPEG_ROOT}/lib"
    )
endif()

if(WIN32 AND FFMPEG_RUNTIME_DLLS)
    foreach(_dll ${FFMPEG_RUNTIME_DLLS})
        add_custom_command(TARGET sonora POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:sonora>"
        )
    endforeach()
endif()

# ---- Automated Unit Tests ----
if(SONORA_BUILD_TESTS)
    enable_testing()

    function(sonora_add_test test_name test_src)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE sonora_core Qt6::Test)
        if(UNIX AND NOT APPLE AND FFMPEG_ROOT)
            set_target_properties(${test_name} PROPERTIES BUILD_RPATH "${FFMPEG_ROOT}/lib")
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
        if(WIN32)
            set(_test_path_mods "")
            if(FFMPEG_BIN_DIR)
                list(APPEND _test_path_mods "PATH=path_list_prepend:${FFMPEG_BIN_DIR}")
            endif()
            list(APPEND _test_path_mods "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
            set_tests_properties(${test_name} PROPERTIES ENVIRONMENT_MODIFICATION "${_test_path_mods}")
        endif()
    endfunction()

    sonora_add_test(test_ring_buffer tests/test_ring_buffer.cpp)
    sonora_add_test(test_text_encoding tests/test_text_encoding.cpp)
    sonora_add_test(test_ffmpeg_decoder tests/test_ffmpeg_decoder.cpp)
    if(FFMPEG_EXECUTABLE)
        target_compile_definitions(test_ffmpeg_decoder PRIVATE
            SONORA_FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}"
        )
    endif()
    if(FFMPEG_BIN_DIR)
        target_compile_definitions(test_ffmpeg_decoder PRIVATE
            SONORA_FFMPEG_BIN_DIR="${FFMPEG_BIN_DIR}"
        )
    endif()
    if(WIN32 AND FFMPEG_RUNTIME_DLLS)
        foreach(_dll ${FFMPEG_RUNTIME_DLLS})
            add_custom_command(TARGET test_ffmpeg_decoder POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:test_ffmpeg_decoder>"
            )
        endforeach()
    endif()
    sonora_add_test(test_library_and_parsers tests/test_library_and_parsers.cpp)
    sonora_add_test(test_dsp_and_fft tests/test_dsp_and_fft.cpp)
endif()
