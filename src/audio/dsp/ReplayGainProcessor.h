#pragma once

#include "audio/AudioTypes.h"

#include <algorithm>
#include <cmath>

namespace sonora {

// Computes effective linear ReplayGain scale with anti-clipping peak safeguard
// and gentle soft-clipping limiter for boosted EQ / hot masters.
class ReplayGainProcessor {
public:
    static double computeLinearGain(const TrackInfo& track,
                                    ReplayGainMode mode,
                                    double preampDb = 0.0,
                                    double fallbackPreampDb = 0.0,
                                    bool preventClipping = true) noexcept {
        if (mode == ReplayGainMode::Off) {
            return 1.0;
        }

        double gainDb = 0.0;
        double peak = 1.0;
        bool hasGain = false;

        if (mode == ReplayGainMode::Album && track.hasRgAlbum) {
            gainDb = track.rgAlbumGainDb;
            peak = track.rgAlbumPeak > 0.0 ? track.rgAlbumPeak : 1.0;
            hasGain = true;
        } else if (track.hasRgTrack) {
            gainDb = track.rgTrackGainDb;
            peak = track.rgTrackPeak > 0.0 ? track.rgTrackPeak : 1.0;
            hasGain = true;
        } else if (track.hasRgAlbum) {
            gainDb = track.rgAlbumGainDb;
            peak = track.rgAlbumPeak > 0.0 ? track.rgAlbumPeak : 1.0;
            hasGain = true;
        }

        const double effectiveDb = hasGain ? (gainDb + preampDb) : fallbackPreampDb;
        double linear = std::pow(10.0, std::clamp(effectiveDb, -24.0, 24.0) / 20.0);

        if (preventClipping && peak > 0.0 && linear * peak > 0.999) {
            linear = 0.999 / peak;
        }
        return linear;
    }

    // Smooth soft-knee limiter above 0.95 (-0.45 dBFS) capping strictly below 0.999 (-0.009 dBFS)
    static inline float softLimit(float x) noexcept {
        constexpr float threshold = 0.95f;
        constexpr float ceiling = 0.999f;
        const float ax = std::abs(x);
        if (ax <= threshold) {
            return x;
        }
        const float excess = (ax - threshold) / (ceiling - threshold);
        const float limited = threshold + (ceiling - threshold) * std::tanh(excess);
        return x >= 0.0f ? limited : -limited;
    }
};

} // namespace sonora
