# CMake cross-compilation toolchain: Linux x86_64 -> Windows x86_64 (LLVM-MinGW 17.0.6 UCRT)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LLVM_MINGW_ROOT "/home/antigravity/deps/llvm-mingw" CACHE PATH "Path to llvm-mingw root")

set(CMAKE_C_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_AR "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-strip" CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH
    "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32"
    "/home/antigravity/deps/qt-win/6.9.1/llvm-mingw_64"
    "/home/antigravity/deps/ffmpeg-9.0.2-win64"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
