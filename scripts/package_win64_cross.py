#!/usr/bin/env python3
"""Packages cross-compiled Sonora.exe with all Qt 6.9.1, FFmpeg 8.1, and LLVM-MinGW runtime dependencies."""
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path("/home/antigravity/sonora")
BUILD_EXE = ROOT / "build/win64-cross-release/Sonora.exe"
OUT_DIR = ROOT / "release/Sonora-win64"

QT_WIN = pathlib.Path("/home/antigravity/deps/qt-win/6.9.1/llvm-mingw_64")
FFMPEG_WIN = pathlib.Path("/home/antigravity/deps/ffmpeg-8.1-win64")
LLVM_MINGW = pathlib.Path("/home/antigravity/deps/llvm-mingw")
OBJDUMP = str(LLVM_MINGW / "bin/x86_64-w64-mingw32-objdump")
STRIP = str(LLVM_MINGW / "bin/x86_64-w64-mingw32-strip")

DLL_SEARCH_DIRS = [
    FFMPEG_WIN / "bin",
    QT_WIN / "bin",
    LLVM_MINGW / "x86_64-w64-mingw32/bin",
    LLVM_MINGW / "bin",
]

SYSTEM_DLLS = {
    "advapi32.dll", "authz.dll", "bcrypt.dll", "bcryptprimitives.dll", "cfgmgr32.dll",
    "comctl32.dll", "comdlg32.dll", "crypt32.dll", "d2d1.dll", "d3d11.dll", "d3d12.dll",
    "d3d9.dll", "dcomp.dll", "dnsapi.dll", "dwmapi.dll", "dwrite.dll", "dxgi.dll",
    "gdi32.dll", "imm32.dll", "iphlpapi.dll", "kernel32.dll", "mpr.dll", "msvcrt.dll",
    "netapi32.dll", "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll", "powrprof.dll",
    "propsys.dll", "psapi.dll", "rpcrt4.dll", "secur32.dll", "setupapi.dll", "shcore.dll",
    "shell32.dll", "shlwapi.dll", "synchronization.dll", "ucrtbase.dll",
    "uiautomationcore.dll", "user32.dll", "userenv.dll", "uxtheme.dll", "version.dll",
    "winhttp.dll", "winmm.dll", "winspool.drv", "ws2_32.dll", "wtsapi32.dll",
    "runtimeobject.dll", "avrt.dll", "mmdevapi.dll", "ksuser.dll",
}

QML_SUBDIRS = [
    "QtCore",
    "QtQml/Base",
    "QtQml/Models",
    "QtQml/WorkerScript",
    "QtQuick",
]

QML_EXCLUDE_SUBTREES = {
    "Scene2D", "Scene3D", "Particles", "Pdf", "VirtualKeyboard", "Effects",
    "NativeStyle", "Material", "Universal", "Fusion", "Imagine", "FluentWinUI3",
    "Windows", "macOS", "iOS", "LocalStorage", "XmlListModel", "Test",
    "tooling", "VectorImage", "Timeline", "Studio", "designer",
}

def is_system_dll(name: str) -> bool:
    low = name.lower()
    if low in SYSTEM_DLLS:
        return True
    return low.startswith("api-ms-win-") or low.startswith("ext-ms-win-")

def imported_dlls(pe_path: pathlib.Path) -> list[str]:
    out = subprocess.check_output([OBJDUMP, "-p", str(pe_path)], text=True, errors="ignore")
    return re.findall(r"DLL Name:\s+(\S+)", out)

def find_dll(name: str) -> pathlib.Path | None:
    low = name.lower()
    for d in DLL_SEARCH_DIRS:
        if not d.exists():
            continue
        direct = d / name
        if direct.exists():
            return direct
        for p in d.iterdir():
            if p.name.lower() == low:
                return p
    return None

def copy_qml_tree(src: pathlib.Path, dst: pathlib.Path):
    if not src.exists():
        return
    for root_str, dirs, files in os.walk(src):
        root_p = pathlib.Path(root_str)
        # Prune unused styles/heavy QML modules
        dirs[:] = [d for d in dirs if d not in QML_EXCLUDE_SUBTREES]
        rel = root_p.relative_to(src)
        target_dir = dst / rel
        target_dir.mkdir(parents=True, exist_ok=True)
        for fn in files:
            # Skip debug/designer/metadata files not needed at runtime
            if fn.endswith((".debug", ".pdb", ".qmltypes", ".metainfo")):
                continue
            shutil.copy2(root_p / fn, target_dir / fn)

def main():
    if not BUILD_EXE.exists():
        print(f"ERROR: {BUILD_EXE} does not exist", file=sys.stderr)
        sys.exit(1)

    if OUT_DIR.exists():
        shutil.rmtree(OUT_DIR)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Copy and strip Sonora.exe
    dst_exe = OUT_DIR / "Sonora.exe"
    shutil.copy2(BUILD_EXE, dst_exe)
    subprocess.run([STRIP, "--strip-unneeded", str(dst_exe)], check=False)

    # 2. Copy FFmpeg 8.1.3 DLLs
    for dll in (FFMPEG_WIN / "bin").glob("*.dll"):
        shutil.copy2(dll, OUT_DIR / dll.name)

    # 3. Copy Qt platform and imageformat plugins (zero QtWidgets)
    plugin_copies = [
        ("platforms", ["qwindows.dll"]),
        ("imageformats", ["qjpeg.dll", "qico.dll", "qgif.dll", "qwebp.dll"]),
    ]
    for sub, names in plugin_copies:
        src_sub = QT_WIN / "plugins" / sub
        dst_sub = OUT_DIR / sub
        dst_sub.mkdir(parents=True, exist_ok=True)
        for n in names:
            p = src_sub / n
            if p.exists():
                shutil.copy2(p, dst_sub / n)

    # 4. Copy minimal required Qt QML runtime modules (QtQuick + Basic style + Dialogs + Shapes + Layouts)
    qml_dst = OUT_DIR / "qml"
    # Copy top-level QtQml qmldir + dll if present
    (qml_dst / "QtQml").mkdir(parents=True, exist_ok=True)
    for f in (QT_WIN / "qml/QtQml").iterdir():
        if f.is_file() and not f.name.endswith((".debug", ".pdb", ".qmltypes")):
            shutil.copy2(f, qml_dst / "QtQml" / f.name)

    for sub in QML_SUBDIRS:
        copy_qml_tree(QT_WIN / "qml" / sub, qml_dst / sub)

    # 5. Write qt.conf for clean self-contained plugin & QML lookup
    (OUT_DIR / "qt.conf").write_text("[Paths]\nPlugins = .\nQmlImports = qml\n", encoding="utf-8")

    # 6. Recursive PE import closure across Sonora.exe + every copied plugin/QML DLL
    scanned: set[pathlib.Path] = set()
    missing: set[str] = set()
    queue: list[pathlib.Path] = [dst_exe] + list(OUT_DIR.rglob("*.dll"))

    while queue:
        cur = queue.pop(0)
        if cur in scanned or not cur.exists():
            continue
        scanned.add(cur)
        for dep in imported_dlls(cur):
            if is_system_dll(dep):
                continue
            target = OUT_DIR / dep
            if not target.exists():
                found = find_dll(dep)
                if found:
                    shutil.copy2(found, target)
                    queue.append(target)
                else:
                    missing.add(dep)

    # 7. Audit bundle size & missing dependencies
    all_files = [p for p in OUT_DIR.rglob("*") if p.is_file()]
    total_bytes = sum(p.stat().st_size for p in all_files)
    largest = max(all_files, key=lambda p: p.stat().st_size)

    print(f"Packaged {len(all_files)} files into {OUT_DIR}")
    print(f"Total size: {total_bytes / (1024*1024):.2f} MB")
    print(f"Largest file: {largest.relative_to(OUT_DIR)} ({largest.stat().st_size / (1024*1024):.2f} MB)")
    if missing:
        print(f"WARNING - Unresolved non-system DLLs: {sorted(missing)}", file=sys.stderr)
        sys.exit(2)
    print("DLL dependency audit: 100% RESOLVED (0 missing non-system DLLs)")

if __name__ == "__main__":
    main()
