#pragma once

#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace sonora {

struct MemorySnapshot {
    double workingSetMb = 0.0;
    double privateBytesMb = 0.0;
};

inline MemorySnapshot queryProcessMemory() {
    MemorySnapshot snap;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        snap.workingSetMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        snap.privateBytesMb = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
#else
    std::ifstream statm("/proc/self/statm");
    uint64_t sizePages = 0, rssPages = 0, sharedPages = 0;
    if (statm >> sizePages >> rssPages >> sharedPages) {
        const double pageMb = static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
        snap.workingSetMb = static_cast<double>(rssPages) * pageMb;
        const uint64_t priv = rssPages > sharedPages ? (rssPages - sharedPages) : 0;
        snap.privateBytesMb = static_cast<double>(priv) * pageMb;
    }
#endif
    return snap;
}

} // namespace sonora
