#pragma once

#include "audio/AudioTypes.h"

#include <QByteArray>
#include <QString>

namespace virela {

class AudioMetadataWriter {
public:
    // Losslessly remuxes srcAudioPath to dstAudioPath using FFmpeg, writing container
    // metadata tags, embedded lyrics, and attached cover art (FLAC / MP3 / M4A).
    static bool writeMetadataAndCover(const QString& srcAudioPath,
                                      const QString& dstAudioPath,
                                      const TrackInfo& meta,
                                      const QByteArray& coverBytes,
                                      const QString& embeddedLyrics = {},
                                      QString* errorOut = nullptr);
};

} // namespace virela
