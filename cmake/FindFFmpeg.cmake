# FindFFmpeg.cmake — Locates FFmpeg 8.1+ (avformat, avcodec, avutil, swresample)
# Supports FFMPEG_ROOT / $ENV{FFMPEG_ROOT} (Windows BtbN/gyan.dev or custom prefix)
# and pkg-config on Linux/macOS.

include(FindPackageHandleStandardArgs)

set(_ffmpeg_hints "")
if(DEFINED FFMPEG_ROOT AND NOT FFMPEG_ROOT STREQUAL "")
    list(APPEND _ffmpeg_hints "${FFMPEG_ROOT}")
endif()
if(DEFINED ENV{FFMPEG_ROOT} AND NOT "$ENV{FFMPEG_ROOT}" STREQUAL "")
    list(APPEND _ffmpeg_hints "$ENV{FFMPEG_ROOT}")
endif()

find_package(PkgConfig QUIET)

function(_sonora_find_ffmpeg_component comp header libname)
    if(PKG_CONFIG_FOUND AND NOT _ffmpeg_hints)
        pkg_check_modules(PC_${comp} QUIET lib${libname})
    endif()

    find_path(FFmpeg_${comp}_INCLUDE_DIR
        NAMES ${header}
        HINTS ${_ffmpeg_hints} ${PC_${comp}_INCLUDE_DIRS}
        PATH_SUFFIXES include
    )

    find_library(FFmpeg_${comp}_LIBRARY
        NAMES ${libname} lib${libname} lib${libname}.dll.a ${libname}.dll.a
        HINTS ${_ffmpeg_hints} ${PC_${comp}_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib/x64 bin
    )

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

_sonora_find_ffmpeg_component(avutil     libavutil/avutil.h         avutil)
_sonora_find_ffmpeg_component(swresample libswresample/swresample.h swresample)
_sonora_find_ffmpeg_component(avcodec    libavcodec/avcodec.h       avcodec)
_sonora_find_ffmpeg_component(avformat   libavformat/avformat.h     avformat)

if(FFmpeg_avutil_FOUND AND FFmpeg_swresample_FOUND)
    set_property(TARGET FFmpeg::swresample APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avutil)
endif()
if(FFmpeg_avutil_FOUND AND FFmpeg_avcodec_FOUND)
    set_property(TARGET FFmpeg::avcodec APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avutil)
endif()
if(FFmpeg_avcodec_FOUND AND FFmpeg_avformat_FOUND)
    set_property(TARGET FFmpeg::avformat APPEND PROPERTY INTERFACE_LINK_LIBRARIES FFmpeg::avcodec FFmpeg::avutil)
endif()

# Locate runtime DLLs on Windows for post-build deployment
if(WIN32 AND _ffmpeg_hints)
    file(GLOB FFMPEG_RUNTIME_DLLS
        "${_ffmpeg_hints}/bin/avcodec*.dll"
        "${_ffmpeg_hints}/bin/avformat*.dll"
        "${_ffmpeg_hints}/bin/avutil*.dll"
        "${_ffmpeg_hints}/bin/swresample*.dll"
    )
    set(FFMPEG_RUNTIME_DLLS "${FFMPEG_RUNTIME_DLLS}" CACHE INTERNAL "FFmpeg runtime DLLs")
endif()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS
        FFmpeg_avformat_LIBRARY FFmpeg_avformat_INCLUDE_DIR
        FFmpeg_avcodec_LIBRARY  FFmpeg_avcodec_INCLUDE_DIR
        FFmpeg_avutil_LIBRARY   FFmpeg_avutil_INCLUDE_DIR
        FFmpeg_swresample_LIBRARY FFmpeg_swresample_INCLUDE_DIR
)
