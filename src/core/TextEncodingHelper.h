#pragma once

#include <QByteArray>
#include <QString>

namespace sonora {

class TextEncodingHelper {
public:
    // Auto-detects BOM (UTF-8/UTF-16LE/UTF-16BE), strict UTF-8, GB18030,
    // Shift-JIS, Big5, and falls back to Latin-1.
    static QString decodeBytes(const QByteArray& raw);

    // Repairs legacy ID3v1 / ISO-8859-1-flagged ID3v2 tags where GBK/GB18030
    // bytes were interpreted byte-for-byte as Latin-1 by FFmpeg.
    static QString repairMetadataString(const QString& input);

    // Validates strict UTF-8 byte sequences (rejects overlong/surrogates/truncated).
    static bool isValidUtf8(const QByteArray& bytes);

    // Converts bytes from a specific legacy codepage (e.g., "GB18030", "Shift-JIS", "Big5")
    // using Win32 MultiByteToWideChar on Windows and iconv on POSIX.
    static bool tryDecodeCodepage(const QByteArray& bytes, const char* encodingName, QString& out);
};

} // namespace sonora
