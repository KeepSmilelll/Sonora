#include "core/TextEncodingHelper.h"

#include <QStringDecoder>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <iconv.h>
#endif

namespace virela {

namespace {

bool isCjkOrCommonZhChar(char16_t ch) {
    // CJK Unified Ideographs, Extension A, Compatibility, CJK punctuation, Fullwidth ASCII
    return (ch >= 0x4E00 && ch <= 0x9FFF)
        || (ch >= 0x3400 && ch <= 0x4DBF)
        || (ch >= 0x3000 && ch <= 0x303F)
        || (ch >= 0xFF00 && ch <= 0xFFEF)
        || (ch >= 0x3040 && ch <= 0x30FF); // Hiragana/Katakana
}

bool isCommonEuropeanAccent(char16_t ch) {
    // Common Western European Latin-1 Supplement letters (À-Ö, Ø-ö, ø-ÿ)
    return (ch >= 0x00C0 && ch <= 0x00D6)
        || (ch >= 0x00D8 && ch <= 0x00F6)
        || (ch >= 0x00F8 && ch <= 0x00FF);
}

int scoreDecodedText(const QString& s) {
    int cjkCount = 0;
    int controlOrPrivate = 0;
    int replacementCount = 0;
    for (QChar qc : s) {
        char16_t c = qc.unicode();
        if (c == 0xFFFD) {
            ++replacementCount;
        } else if (isCjkOrCommonZhChar(c)) {
            ++cjkCount;
        } else if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r')
                   || (c >= 0xE000 && c <= 0xF8FF)) {
            ++controlOrPrivate;
        }
    }
    if (replacementCount > 0 || controlOrPrivate > 0) {
        return -1000;
    }
    return cjkCount * 4;
}

} // namespace

bool TextEncodingHelper::isValidUtf8(const QByteArray& bytes) {
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.constData());
    const size_t len = static_cast<size_t>(bytes.size());
    size_t i = 0;
    while (i < len) {
        uint8_t c = data[i];
        if (c <= 0x7F) {
            ++i;
            continue;
        }
        size_t needed = 0;
        uint32_t cp = 0;
        if ((c & 0xE0) == 0xC0) {
            needed = 1;
            cp = c & 0x1F;
            if (cp < 0x02) return false; // overlong
        } else if ((c & 0xF0) == 0xE0) {
            needed = 2;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            needed = 3;
            cp = c & 0x07;
        } else {
            return false;
        }
        if (i + needed >= len) return false;
        for (size_t j = 1; j <= needed; ++j) {
            uint8_t next = data[i + j];
            if ((next & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3F);
        }
        if (needed == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) {
            return false;
        }
        if (needed == 3 && (cp < 0x10000 || cp > 0x10FFFF)) {
            return false;
        }
        i += needed + 1;
    }
    return true;
}

bool TextEncodingHelper::tryDecodeCodepage(const QByteArray& bytes, const char* encodingName, QString& out) {
    if (bytes.isEmpty()) {
        out.clear();
        return true;
    }

#ifdef _WIN32
    UINT cp = CP_UTF8;
    if (std::strcmp(encodingName, "GB18030") == 0) {
        cp = 54936;
    } else if (std::strcmp(encodingName, "GBK") == 0) {
        cp = 936;
    } else if (std::strcmp(encodingName, "Shift-JIS") == 0) {
        cp = 932;
    } else if (std::strcmp(encodingName, "Big5") == 0) {
        cp = 950;
    } else {
        return false;
    }

    int needed = MultiByteToWideChar(
        cp, MB_ERR_INVALID_CHARS,
        bytes.constData(), static_cast<int>(bytes.size()),
        nullptr, 0);
    if (needed <= 0) {
        return false;
    }
    QString result(needed, Qt::Uninitialized);
    int written = MultiByteToWideChar(
        cp, MB_ERR_INVALID_CHARS,
        bytes.constData(), static_cast<int>(bytes.size()),
        reinterpret_cast<LPWSTR>(result.data()), needed);
    if (written <= 0) {
        return false;
    }
    out = std::move(result);
    return true;
#else
    const char* iconvName = encodingName;
    if (std::strcmp(encodingName, "Shift-JIS") == 0) {
        iconvName = "CP932";
    } else if (std::strcmp(encodingName, "Big5") == 0) {
        iconvName = "BIG5-HKSCS";
    }
    iconv_t cd = iconv_open("UTF-8", iconvName);
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        return false;
    }

    size_t inBytesLeft = static_cast<size_t>(bytes.size());
    char* inPtr = const_cast<char*>(bytes.constData());
    QByteArray utf8Buf;
    utf8Buf.resize(static_cast<qsizetype>(inBytesLeft * 4 + 16));
    char* outPtr = utf8Buf.data();
    size_t outBytesLeft = static_cast<size_t>(utf8Buf.size());

    size_t res = iconv(cd, &inPtr, &inBytesLeft, &outPtr, &outBytesLeft);
    iconv_close(cd);
    if (res == static_cast<size_t>(-1) || inBytesLeft != 0) {
        return false;
    }
    const qsizetype produced = utf8Buf.size() - static_cast<qsizetype>(outBytesLeft);
    out = QString::fromUtf8(utf8Buf.constData(), produced);
    return true;
#endif
}

QString TextEncodingHelper::decodeBytes(const QByteArray& raw) {
    if (raw.isEmpty()) {
        return {};
    }

    // 1. Check BOMs
    if (raw.size() >= 3
        && static_cast<uint8_t>(raw[0]) == 0xEF
        && static_cast<uint8_t>(raw[1]) == 0xBB
        && static_cast<uint8_t>(raw[2]) == 0xBF) {
        return QString::fromUtf8(raw.constData() + 3, raw.size() - 3);
    }
    if (raw.size() >= 2
        && static_cast<uint8_t>(raw[0]) == 0xFF
        && static_cast<uint8_t>(raw[1]) == 0xFE) {
        auto decoder = QStringDecoder(QStringDecoder::Utf16LE);
        return decoder.decode(raw.mid(2));
    }
    if (raw.size() >= 2
        && static_cast<uint8_t>(raw[0]) == 0xFE
        && static_cast<uint8_t>(raw[1]) == 0xFF) {
        auto decoder = QStringDecoder(QStringDecoder::Utf16BE);
        return decoder.decode(raw.mid(2));
    }

    // 2. Strict UTF-8 check
    if (isValidUtf8(raw)) {
        return QString::fromUtf8(raw);
    }

    // 3. Try GB18030 (primary for Chinese local libraries), then Shift-JIS, then Big5
    QString gb;
    if (tryDecodeCodepage(raw, "GB18030", gb) && scoreDecodedText(gb) > 0) {
        return gb;
    }
    QString sjis;
    if (tryDecodeCodepage(raw, "Shift-JIS", sjis) && scoreDecodedText(sjis) > 0) {
        return sjis;
    }
    QString big5;
    if (tryDecodeCodepage(raw, "Big5", big5) && scoreDecodedText(big5) > 0) {
        return big5;
    }

    // Fallback to GB18030 if valid even without CJK characters, else Latin-1
    if (!gb.isEmpty()) {
        return gb;
    }
    return QString::fromLatin1(raw);
}

QString TextEncodingHelper::repairMetadataString(const QString& input) {
    if (input.isEmpty()) {
        return input;
    }

    // Check if every char fits in Latin-1 and contains high bytes (>= 0x80).
    // When legacy ID3v1/v2 stores GBK/GB18030 bytes marked as ISO-8859-1,
    // FFmpeg converts each byte directly to U+0080..U+00FF.
    bool hasHighByte = false;
    int consecutiveHighRun = 0;
    int maxHighRun = 0;
    int singleIsolatedAccents = 0;
    for (int i = 0; i < input.size(); ++i) {
        const char16_t c = input[i].unicode();
        if (c > 0xFF) {
            // Already contains real Unicode (CJK, Cyrillic, etc.) -> do not touch
            return input;
        }
        if (c >= 0x80) {
            hasHighByte = true;
            ++consecutiveHighRun;
            if (consecutiveHighRun > maxHighRun) {
                maxHighRun = consecutiveHighRun;
            }
        } else {
            if (consecutiveHighRun == 1 && i > 0 && isCommonEuropeanAccent(input[i - 1].unicode())) {
                ++singleIsolatedAccents;
            }
            consecutiveHighRun = 0;
        }
    }
    if (consecutiveHighRun == 1 && isCommonEuropeanAccent(input.back().unicode())) {
        ++singleIsolatedAccents;
    }

    if (!hasHighByte) {
        return input;
    }

    // Single isolated European accents (e.g. "Café", "Björk", "Sigur Rós") have maxHighRun == 1
    if (maxHighRun < 2 && singleIsolatedAccents > 0) {
        return input;
    }

    const QByteArray raw = input.toLatin1();

    // First check if the recovered raw bytes are actually valid UTF-8 (double-encoded UTF-8-as-Latin1)
    if (isValidUtf8(raw)) {
        const QString utf8Candidate = QString::fromUtf8(raw);
        if (scoreDecodedText(utf8Candidate) > 0) {
            return utf8Candidate;
        }
    }

    // Check GB18030 recovery (must yield valid CJK characters to avoid mangling accented names)
    QString gbCandidate;
    if (tryDecodeCodepage(raw, "GB18030", gbCandidate) && scoreDecodedText(gbCandidate) >= 4) {
        return gbCandidate;
    }

    QString sjisCandidate;
    if (tryDecodeCodepage(raw, "Shift-JIS", sjisCandidate) && scoreDecodedText(sjisCandidate) >= 4) {
        return sjisCandidate;
    }

    return input;
}

} // namespace virela
