# FindFFmpeg.cmake — Locates FFmpeg 9.0.2 (avformat, avcodec, avutil, swresample)
# Supports FFMPEG_ROOT / $ENV{FFMPEG_ROOT} (Windows gyan.dev/BtbN or custom prefix)
# and pkg-config on Linux/macOS.

include(FindPackageHandleStandardArgs)

set(_ffmpeg_hints "")
if(DEFINED FFMPEG_ROOT AND NOT FFMPEG_ROOT STREQUAL "")
    file(TO_CMAKE_PATH "${FFMPEG_ROOT}" _ffmpeg_root_norm)
    list(APPEND _ffmpeg_hints "${_ffmpeg_root_norm}")
endif()
if(DEFINED ENV{FFMPEG_ROOT} AND NOT "$ENV{FFMPEG_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{FFMPEG_ROOT}" _ffmpeg_env_norm)
    list(APPEND _ffmpeg_hints "${_ffmpeg_env_norm}")
endif()
if(WIN32 AND NOT _ffmpeg_hints AND EXISTS "D:/Setup/ffmpeg-9.0.2-full_build-shared/include/libavcodec/avcodec.h")
    list(APPEND _ffmpeg_hints "D:/Setup/ffmpeg-9.0.2-full_build-shared")
endif()
list(REMOVE_DUPLICATES _ffmpeg_hints)

find_package(PkgConfig QUIET)

function(_virela_find_ffmpeg_component comp header libname)
    if(PKG_CONFIG_FOUND AND NOT _ffmpeg_hints)
        pkg_check_modules(PC_${comp} QUIET lib${libname})
    endif()

    if(_ffmpeg_hints)
        # Search explicit FFMPEG_ROOT hints exclusively first so older FFmpeg installs on PATH
        # cannot shadow the configured FFmpeg 9.0.2 SDK.
        find_path(FFmpeg_${comp}_INCLUDE_DIR
            NAMES ${header}
            PATHS ${_ffmpeg_hints}
            PATH_SUFFIXES include
            NO_DEFAULT_PATH
        )
        find_library(FFmpeg_${comp}_LIBRARY
            NAMES ${libname} lib${libname} lib${libname}.dll.a ${libname}.dll.a
            PATHS ${_ffmpeg_hints}
            PATH_SUFFIXES lib lib/x64 bin
            NO_DEFAULT_PATH
        )
    else()
        find_path(FFmpeg_${comp}_INCLUDE_DIR
            NAMES ${header}
            HINTS ${PC_${comp}_INCLUDE_DIRS}
            PATH_SUFFIXES include
        )
        find_library(FFmpeg_${comp}_LIBRARY
            NAMES ${libname} lib${libname} lib${libname}.dll.a ${libname}.dll.a
            HINTS ${PC_${comp}_LIBRARY_DIRS}
            PATH_SUFFIXES lib lib/x64 bin
        )
    endif()

    if(FFmpeg_${comp}_INCLUDE_DIR AND FFmpeg_${comp}_LIBRARY)
        set(FFmpeg_${comp}_FOUND TRUE PARENT_SCOPE)
        if(NOT TARGET FFmpeg::${comp})
            add_library(FFmpeg::${comp} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${comp} PROPERTIES
                IMPORTED_LOCATION "${FFmpeg_${comp}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_${comp}_INCLUDE_DIR}"
            )
        endif()
    else()
        set(FFmpeg_${comp}_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

_virela_find_ffmpeg_component(avutil     libavutil/avutil.h         avutil)
_virela_find_ffmpeg_component(swresample libswresample/swresample.h swresample)
_virela_find_ffmpeg_component(avcodec    libavcodec/avcodec.h       avcodec)
_virela_find_ffmpeg_component(avformat   libavformat/avformat.h     avformat)

if(FFmpeg_avutil_FOUND AND FFmpeg_swresample_FOUND)
    set_property(TARGET FFmpeg::swresample APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avutil)
endif()
if(FFmpeg_avutil_FOUND AND FFmpeg_avcodec_FOUND)
    set_property(TARGET FFmpeg::avcodec APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avutil)
endif()
if(FFmpeg_avcodec_FOUND AND FFmpeg_avformat_FOUND)
    set_property(TARGET FFmpeg::avformat APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avcodec FFmpeg::avutil)
endif()

# Locate runtime DLLs and ffmpeg CLI binary from the matched prefix
set(_ffmpeg_runtime_dlls "")
set(_ffmpeg_bin_dir "")
foreach(_hint ${_ffmpeg_hints})
    if(WIN32)
        file(GLOB _candidate_dlls
            "${_hint}/bin/avcodec*.dll"
            "${_hint}/bin/avformat*.dll"
            "${_hint}/bin/avutil*.dll"
            "${_hint}/bin/swresample*.dll"
        )
        if(_candidate_dlls)
            set(_ffmpeg_runtime_dlls "${_candidate_dlls}")
            set(_ffmpeg_bin_dir "${_hint}/bin")
            break()
        endif()
    elseif(EXISTS "${_hint}/bin")
        set(_ffmpeg_bin_dir "${_hint}/bin")
        break()
    endif()
endforeach()

if(WIN32)
    set(FFMPEG_RUNTIME_DLLS "${_ffmpeg_runtime_dlls}" CACHE INTERNAL "FFmpeg runtime DLLs")
endif()
set(FFMPEG_BIN_DIR "${_ffmpeg_bin_dir}" CACHE INTERNAL "FFmpeg binary directory")

if(_ffmpeg_bin_dir)
    find_program(FFMPEG_EXECUTABLE
        NAMES ffmpeg
        PATHS "${_ffmpeg_bin_dir}"
        NO_DEFAULT_PATH
    )
else()
    find_program(FFMPEG_EXECUTABLE NAMES ffmpeg)
endif()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS
        FFmpeg_avformat_LIBRARY FFmpeg_avformat_INCLUDE_DIR
        FFmpeg_avcodec_LIBRARY  FFmpeg_avcodec_INCLUDE_DIR
        FFmpeg_avutil_LIBRARY   FFmpeg_avutil_INCLUDE_DIR
        FFmpeg_swresample_LIBRARY FFmpeg_swresample_INCLUDE_DIR
)

if(FFmpeg_FOUND)
    message(STATUS "Virela FFmpeg include: ${FFmpeg_avcodec_INCLUDE_DIR}")
    message(STATUS "Virela FFmpeg avcodec: ${FFmpeg_avcodec_LIBRARY}")
    if(WIN32 AND FFMPEG_RUNTIME_DLLS)
        message(STATUS "Virela FFmpeg runtime DLLs: ${FFMPEG_RUNTIME_DLLS}")
    endif()
endif()
