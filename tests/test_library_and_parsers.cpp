#include "library/CoverArtProvider.h"
#include "library/CueParser.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "library/M3uPlaylistIO.h"
#include "lyrics/LyricsParser.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <cmath>
#include <vector>

using namespace sonora;

namespace {

bool writeTinyWav(const QString& path, int sampleRate, double seconds) {
    const uint32_t frames = static_cast<uint32_t>(std::llround(sampleRate * seconds));
    const uint32_t dataBytes = frames * 2 * 2;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    auto u32 = [&](uint32_t v) { char b[4] = {char(v), char(v >> 8), char(v >> 16), char(v >> 24)}; f.write(b, 4); };
    auto u16 = [&](uint16_t v) { char b[2] = {char(v), char(v >> 8)}; f.write(b, 2); };
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(2); u32(sampleRate); u32(sampleRate * 4); u16(4); u16(16);
    f.write("data", 4); u32(dataBytes);
    std::vector<int16_t> pcm(frames * 2, 1000);
    return f.write(reinterpret_cast<const char*>(pcm.data()), dataBytes) == static_cast<qint64>(dataBytes);
}

QByteArray makeSolidPng(const QColor& col, int size = 256) {
    QImage img(size, size, QImage::Format_RGB32);
    img.fill(col);
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return out;
}

} // namespace

class TestLibraryAndParsers : public QObject {
    Q_OBJECT

private slots:
    void sqliteFts5AndChineseSearchAndPlaylists() {
        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));

        QVector<TrackInfo> batch;
        TrackInfo t1;
        t1.filePath = QStringLiteral("/music/jay/qingtian.flac");
        t1.title = QStringLiteral("晴天");
        t1.artist = QStringLiteral("周杰伦");
        t1.album = QStringLiteral("叶惠美");
        t1.albumArtist = QStringLiteral("周杰伦");
        t1.trackNumber = 3;
        t1.year = 2003;
        t1.durationMs = 269000;
        t1.codec = QStringLiteral("FLAC");
        t1.formatBadge = QStringLiteral("Lossless");
        batch.push_back(t1);

        TrackInfo t2;
        t2.filePath = QStringLiteral("/music/queen/bohemian.flac");
        t2.title = QStringLiteral("Bohemian Rhapsody");
        t2.artist = QStringLiteral("Queen");
        t2.album = QStringLiteral("A Night at the Opera");
        t2.albumArtist = QStringLiteral("Queen");
        t2.trackNumber = 11;
        t2.year = 1975;
        t2.durationMs = 354000;
        t2.codec = QStringLiteral("FLAC");
        t2.formatBadge = QStringLiteral("Hi-Res");
        batch.push_back(t2);

        QVERIFY(db.upsertTracksBatch(batch));
        QCOMPARE(db.totalTrackCount(), 2);
        QVERIFY(batch[0].id > 0);
        QVERIFY(batch[1].id > 0);

        // 2-character Chinese search (uses LIKE path since trigram needs >= 3 chars)
        const auto zhRes = db.queryTracks(QStringLiteral("晴天"));
        QCOMPARE(zhRes.size(), 1);
        QCOMPARE(zhRes.first().artist, QStringLiteral("周杰伦"));

        // 3+ character English & Chinese search (uses FTS5 trigram index!)
        const auto enRes = db.queryTracks(QStringLiteral("Bohem"));
        QCOMPARE(enRes.size(), 1);
        QCOMPARE(enRes.first().title, QStringLiteral("Bohemian Rhapsody"));

        const auto zh3Res = db.queryTracks(QStringLiteral("周杰伦"));
        QCOMPARE(zh3Res.size(), 1);

        // Albums & Artists aggregation
        QCOMPARE(db.queryAlbums().size(), 2);
        QCOMPARE(db.queryArtists().size(), 2);

        // Favorites & Play count
        QVERIFY(db.setFavorite(batch[0].id, true));
        QCOMPARE(db.queryFavoriteTracks().size(), 1);
        QVERIFY(db.recordPlayback(batch[1].id));
        QCOMPARE(db.queryRecentlyPlayed(10).size(), 1);
        QCOMPARE(db.queryRecentlyPlayed(10).first().playCount, 1);

        // Playlists CRUD
        const int64_t plId = db.createPlaylist(QStringLiteral("Late Night Hi-Res"));
        QVERIFY(plId > 0);
        QVERIFY(db.addTrackToPlaylist(plId, batch[0].id));
        QVERIFY(db.addTrackToPlaylist(plId, batch[1].id));
        QCOMPARE(db.queryPlaylistTracks(plId).size(), 2);
        QVERIFY(db.removePlaylistItem(plId, 0));
        QCOMPARE(db.queryPlaylistTracks(plId).size(), 1);
        QCOMPARE(db.queryPlaylistTracks(plId).first().id, batch[1].id);
    }

    void cueParserTimestampsGb18030AndScannerFolding() {
        QCOMPARE(CueParser::parseCueTimestampMs(QStringLiteral("01:02:37")), 62493);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString wavPath = dir.filePath(QStringLiteral("Concert.wav"));
        QVERIFY(writeTinyWav(wavPath, 44100, 4.0));
        QFile coverFile(dir.filePath(QStringLiteral("cover.png")));
        QVERIFY(coverFile.open(QIODevice::WriteOnly));
        coverFile.write(makeSolidPng(QColor(220, 60, 90), 200));
        coverFile.close();

        // Write GB18030-encoded CUE referencing "Concert.flac" (tests stem rescue to Concert.wav + GB18030 decode!)
        QByteArray gbCue;
        gbCue.append("PERFORMER \"");
        gbCue.append(QByteArray::fromHex("D6DCBDDCC2D7")); // 周杰伦
        gbCue.append("\"\r\nTITLE \"");
        gbCue.append(QByteArray::fromHex("D2B6BBDDC3C0")); // 叶惠美
        gbCue.append("\"\r\nFILE \"Concert.flac\" WAVE\r\n");
        gbCue.append("  TRACK 01 AUDIO\r\n");
        gbCue.append("    TITLE \"");
        gbCue.append(QByteArray::fromHex("C7E7CCEC")); // 晴天
        gbCue.append("\"\r\n    INDEX 01 00:00:00\r\n");
        gbCue.append("  TRACK 02 AUDIO\r\n");
        gbCue.append("    TITLE \"");
        gbCue.append(QByteArray::fromHex("D2B9C7FA")); // 夜曲
        gbCue.append("\"\r\n    INDEX 01 00:02:00\r\n");

        const QString cuePath = dir.filePath(QStringLiteral("Concert.cue"));
        QFile cf(cuePath);
        QVERIFY(cf.open(QIODevice::WriteOnly));
        cf.write(gbCue);
        cf.close();

        const CueParseResult parsed = CueParser::parseFile(cuePath);
        QVERIFY(parsed.valid);
        QCOMPARE(parsed.tracks.size(), 2);
        QCOMPARE(parsed.tracks[0].title, QStringLiteral("晴天"));
        QCOMPARE(parsed.tracks[0].artist, QStringLiteral("周杰伦"));
        QCOMPARE(parsed.tracks[0].startMs, 0);
        QCOMPARE(parsed.tracks[0].endMs, 2000);
        QCOMPARE(parsed.tracks[1].title, QStringLiteral("夜曲"));
        QCOMPARE(parsed.tracks[1].startMs, 2000);
        QVERIFY(parsed.tracks[1].endMs >= 3900);

        // Verify LibraryScanner folds Concert.wav into the 2 CUE subtracks (not 3!) and indexes cover.png
        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        CoverArtProvider covers(dir.filePath(QStringLiteral("cover_cache")));
        LibraryScanner scanner(&db, &covers);
        const int indexed = scanner.scanSynchronously({dir.path()});
        QCOMPARE(indexed, 2);
        QCOMPARE(db.totalTrackCount(), 2);
        const auto tracks = db.queryTracks();
        QVERIFY(!tracks.first().coverHash.isEmpty());
    }

    void lyricsParserMultiTagTranslationAndEnhancedLrc() {
        const QString raw = QStringLiteral(
            "[ti:Test Song]\n"
            "[offset:200]\n"
            "[00:01.00][00:10.00]Hello world\n"
            "[00:01.00]你好世界\n"
            "[00:05.50]<00:05.50>Word <00:06.00>by <00:06.50>word\n");

        const ParsedLyrics lyr = LyricsParser::parse(raw, QStringLiteral("LRC"));
        QVERIFY(lyr.synced);
        QCOMPARE(lyr.offsetMs, 200);
        QCOMPARE(lyr.lines.size(), 3);

        // Line 0: merged dual-language translation
        QCOMPARE(lyr.lines[0].timestampMs, 1000);
        QCOMPARE(lyr.lines[0].text, QStringLiteral("Hello world"));
        QCOMPARE(lyr.lines[0].translation, QStringLiteral("你好世界"));

        // Line 1: Enhanced LRC word tokens stripped cleanly
        QCOMPARE(lyr.lines[1].timestampMs, 5500);
        QCOMPARE(lyr.lines[1].text, QStringLiteral("Word by word"));
        QCOMPARE(lyr.lines[1].words.size(), 3);

        // Line 2: expanded chorus repeat timestamp
        QCOMPARE(lyr.lines[2].timestampMs, 10000);
        QCOMPARE(lyr.lines[2].text, QStringLiteral("Hello world"));

        // Active line binary search with offset (+200ms)
        QCOMPARE(LyricsParser::findActiveLineIndex(lyr, 500), -1);   // 500+200=700 < 1000
        QCOMPARE(LyricsParser::findActiveLineIndex(lyr, 900), 0);    // 900+200=1100 >= 1000
        QCOMPARE(LyricsParser::findActiveLineIndex(lyr, 5400), 1);   // 5400+200=5600 >= 5500
        QCOMPARE(LyricsParser::findActiveLineIndex(lyr, 12000), 2);
    }

    void m3u8ExportImportRoundTrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString m3uPath = dir.filePath(QStringLiteral("favorites.m3u8"));

        TrackInfo a;
        a.filePath = dir.filePath(QStringLiteral("01 晴天.flac"));
        a.title = QStringLiteral("晴天");
        a.artist = QStringLiteral("周杰伦");
        a.durationMs = 269000;

        TrackInfo b;
        b.filePath = dir.filePath(QStringLiteral("02 Nocturne.wav"));
        b.title = QStringLiteral("Nocturne");
        b.artist = QStringLiteral("Chopin");
        b.durationMs = 240000;

        QVERIFY(M3uPlaylistIO::saveFile(m3uPath, {a, b}, true));
        const auto loaded = M3uPlaylistIO::loadFile(m3uPath);
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].filePath, QDir::cleanPath(a.filePath));
        QCOMPARE(loaded[0].durationSeconds, 269);
        QCOMPARE(loaded[1].filePath, QDir::cleanPath(b.filePath));
    }

    void coverArtProviderStrictRamLruCap() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Set a tight 512 KB RAM cap and load 16 distinct 360x360 covers (~518 KB each in RGBA32!)
        constexpr int kCapBytes = 512 * 1024;
        CoverArtProvider provider(dir.path(), kCapBytes);

        for (int i = 0; i < 16; ++i) {
            const QByteArray png = makeSolidPng(QColor::fromHsv(i * 22, 200, 220), 400);
            const QString hash = provider.storeRawImageBytes(png);
            QVERIFY(!hash.isEmpty());
            const QImage loaded = provider.loadCover(hash, 360);
            QVERIFY(!loaded.isNull());
            QVERIFY(loaded.width() <= 360 && loaded.height() <= 360);
            QVERIFY2(provider.currentRamCacheBytes() <= kCapBytes,
                     qPrintable(QStringLiteral("RAM cache exceeded cap: %1 > %2")
                                    .arg(provider.currentRamCacheBytes())
                                    .arg(kCapBytes)));
        }
    }
};

QTEST_MAIN(TestLibraryAndParsers)
#include "test_library_and_parsers.moc"
