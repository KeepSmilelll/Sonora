#include "audio/PlaybackEngine.h"
#include "core/AppSettings.h"
#include "core/PlaybackController.h"
#include "core/ThemeManager.h"
#include "library/CoverArtProvider.h"
#include "library/CueParser.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "library/M3uPlaylistIO.h"
#include "lyrics/LyricsController.h"
#include "lyrics/LyricsParser.h"
#include "models/LibraryModels.h"
#include "sqlite3.h"
#include "ui/IconImageProvider.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <cmath>
#include <vector>

using namespace virela;

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

    void lyricsParserStripsNumericWordMarkers() {
        const QString raw = QStringLiteral(
            "[00:02.00]<13000,11000>Chin<13600,11600>)\n"
            "[00:14.80]<3500,-3500>你<4850,-650>不<7700,-200>要<8465,3775>那<8877,6177>样\n"
            "[00:14.80]<+3500 , -3500>Don't look at me like that <3\n"
            "[00:18.00]<123,0><00:18.00>Word <00:19.00>timing\n");

        const ParsedLyrics parsed = LyricsParser::parse(raw, QStringLiteral("LRC"));
        QVERIFY(parsed.synced);
        QCOMPARE(parsed.lines.size(), 3);
        QCOMPARE(parsed.lines[0].text, QStringLiteral("Chin)"));
        QCOMPARE(parsed.lines[1].timestampMs, 14800);
        QCOMPARE(parsed.lines[1].text, QStringLiteral("你不要那样"));
        QCOMPARE(parsed.lines[1].translation, QStringLiteral("Don't look at me like that <3"));
        QVERIFY(parsed.lines[1].words.isEmpty());
        QCOMPARE(parsed.lines[2].text, QStringLiteral("Word timing"));
        QCOMPARE(parsed.lines[2].words.size(), 2);
        QCOMPARE(LyricsParser::findActiveLineIndex(parsed, 14799), 0);
        QCOMPARE(LyricsParser::findActiveLineIndex(parsed, 14800), 1);

        const ParsedLyrics plain = LyricsParser::parse(QStringLiteral("<1200,-500>Plain line"), {});
        QVERIFY(!plain.synced);
        QCOMPARE(plain.lines.size(), 1);
        QCOMPARE(plain.lines[0].text, QStringLiteral("Plain line"));
    }

    void lyricsParserPairsTranslationsAtLineEnd() {
        const QString raw = QStringLiteral(
            "[offset:200]\n"
            "[00:00.00]<1300,-1300>Song title\n"
            "[00:10.00]<0,0> <0,0>\n"
            "[00:10.00]<360,-360>First line\n"
            "[00:14.00]<0,0>第<0,0>一<0,0>句\n"
            "[00:14.00]<1250,-1250>Second line\n"
            "[00:19.00]<0,0>第<0,0>二<0,0>句\n"
            "[00:19.00]<300,0>Third line\n"
            "[00:22.00]<0,0>第<0,0>三<0,0>句\n"
            "[00:22.00]\n"
            "[00:27.25]Later line\n"
            "[00:27.25]后面一句\n");

        const auto parsed = LyricsParser::parse(raw);
        QVERIFY(parsed.synced);
        QCOMPARE(parsed.offsetMs, 200);
        QCOMPARE(parsed.lines.size(), 6);
        QCOMPARE(parsed.lines[0].text, QStringLiteral("Song title"));
        QCOMPARE(parsed.lines[1].timestampMs, 10000);
        QCOMPARE(parsed.lines[1].text, QStringLiteral("First line"));
        QCOMPARE(parsed.lines[1].translation, QStringLiteral("第一句"));
        QCOMPARE(parsed.lines[2].timestampMs, 14000);
        QCOMPARE(parsed.lines[2].text, QStringLiteral("Second line"));
        QCOMPARE(parsed.lines[2].translation, QStringLiteral("第二句"));
        QCOMPARE(parsed.lines[3].timestampMs, 19000);
        QCOMPARE(parsed.lines[3].text, QStringLiteral("Third line"));
        QCOMPARE(parsed.lines[3].translation, QStringLiteral("第三句"));
        QCOMPARE(parsed.lines[4].timestampMs, 22000);
        QVERIFY(parsed.lines[4].text.isEmpty());
        QCOMPARE(parsed.lines[5].text, QStringLiteral("Later line"));
        QCOMPARE(parsed.lines[5].translation, QStringLiteral("后面一句"));
        QCOMPARE(LyricsParser::findActiveLineIndex(parsed, 13799), 1);
        QCOMPARE(LyricsParser::findActiveLineIndex(parsed, 13800), 2);
    }

    void lyricsParserPreservesOrdinarySameTimestampPairsWithNumericMarkers() {
        const auto parsed = LyricsParser::parse(QStringLiteral(
            "[00:10.00]<120,0>Title\n"
            "[00:14.00]<0,0>第一句\n"
            "[00:14.00]<1300,-1300>First line\n"
            "[00:19.00]<0,0>第二句\n"
            "[00:19.00]<420,0>Second line\n"
            "[00:21.00]\n"));

        QCOMPARE(parsed.lines.size(), 4);
        QVERIFY(parsed.lines[0].translation.isEmpty());
        QCOMPARE(parsed.lines[1].timestampMs, 14000);
        QCOMPARE(parsed.lines[1].text, QStringLiteral("第一句"));
        QCOMPARE(parsed.lines[1].translation, QStringLiteral("First line"));
        QCOMPARE(parsed.lines[2].text, QStringLiteral("第二句"));
        QCOMPARE(parsed.lines[2].translation, QStringLiteral("Second line"));
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

    void manualRescanFindsAddedCoverAndRepairsCache() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeTinyWav(dir.filePath(QStringLiteral("song.wav")), 44100, 0.1));

        const QString cacheDir = dir.filePath(QStringLiteral("cache"));
        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        CoverArtProvider covers(cacheDir);
        LibraryScanner scanner(&db, &covers);
        const QStringList folders{dir.path()};
        QCOMPARE(scanner.scanSynchronously(folders), 1);
        const auto initial = db.queryTracks();
        QCOMPARE(initial.size(), 1);
        QVERIFY(initial.first().coverHash.isEmpty());
        QVERIFY(db.setFavorite(initial.first().id, true));

        QFile brokenCover(dir.filePath(QStringLiteral("cover.jpg")));
        QVERIFY(brokenCover.open(QIODevice::WriteOnly));
        QCOMPARE(brokenCover.write("bad", 3), 3);
        brokenCover.close();
        QFile matchingCover(dir.filePath(QStringLiteral("song.png")));
        QVERIFY(matchingCover.open(QIODevice::WriteOnly));
        const QByteArray art = makeSolidPng(Qt::blue);
        QCOMPARE(matchingCover.write(art), art.size());
        matchingCover.close();

        QCOMPARE(scanner.scanSynchronously(folders), 0);
        QCOMPARE(scanner.scanSynchronously(folders, true), 1);
        const auto refreshed = db.queryTracks();
        QCOMPARE(refreshed.size(), 1);
        QCOMPARE(refreshed.first().id, initial.first().id);
        QVERIFY(refreshed.first().isFavorite);
        const QString hash = refreshed.first().coverHash;
        QVERIFY(!hash.isEmpty());
        QVERIFY(!covers.loadCover(hash).isNull());

        QFile cachedCover(QDir(cacheDir).filePath(hash + QStringLiteral(".jpg")));
        QVERIFY(cachedCover.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(cachedCover.write("bad", 3), 3);
        cachedCover.close();
        covers.clearRamCache();
        QVERIFY(covers.loadCover(hash).isNull());
        QCOMPARE(scanner.scanSynchronously(folders, true), 1);
        QVERIFY(!covers.loadCover(hash).isNull());
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

    void lyricsControllerExposesWordTokensAndProgress() {
        LyricsController ctrl;
        const QString enhanced = QStringLiteral(
            "[00:01.00]<00:01.00>Hel<00:01.50>lo <00:02.00>world\n"
            "[00:03.00]<00:03.00>Next\n");
        ctrl.loadForTrack({}, enhanced);

        QVERIFY(ctrl.hasLyrics());
        QVERIFY(ctrl.isSynced());
        QVERIFY(ctrl.hasEnhancedWords());
        QCOMPARE(ctrl.rowCount(), 2);
        QVERIFY(ctrl.data(ctrl.index(0), LyricsController::HasWordsRole).toBool());

        const QVariantList tokens = ctrl.data(ctrl.index(0), LyricsController::WordTokensRole).toList();
        QCOMPARE(tokens.size(), 3);
        QCOMPARE(tokens[0].toMap().value(QStringLiteral("startMs")).toLongLong(), 1000);
        QCOMPARE(tokens[0].toMap().value(QStringLiteral("endMs")).toLongLong(), 1500);
        QCOMPARE(tokens[0].toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Hel"));
        QCOMPARE(tokens[2].toMap().value(QStringLiteral("endMs")).toLongLong(), 3000);

        ctrl.updatePositionMs(1250);
        QCOMPARE(ctrl.activeLineIndex(), 0);
        QCOMPARE(ctrl.activeLineText(), QStringLiteral("Hello world"));
        QCOMPARE(ctrl.effectivePositionMs(), 1250);
        QVERIFY(ctrl.activeLineProgress() > 0.10 && ctrl.activeLineProgress() < 0.30);

        ctrl.updatePositionMs(2500);
        QVERIFY(ctrl.activeLineProgress() > 0.75 && ctrl.activeLineProgress() < 0.99);

        const QString plainSynced = QStringLiteral("[00:01.00]Line one\n[00:03.00]Line two\n");
        ctrl.loadForTrack({}, plainSynced);
        QVERIFY(!ctrl.hasEnhancedWords());
        ctrl.updatePositionMs(2000);
        QCOMPARE(ctrl.activeLineIndex(), 0);
        QVERIFY(std::abs(ctrl.activeLineProgress() - 0.5) < 0.01);
    }

    void lyricsParserWordTimedYrcQrcKrcAndTailCap() {
        // 1. NetEase YRC with JSON metadata header
        const QString yrcRaw = QStringLiteral(
            "{\"t\":0,\"c\":[{\"tx\":\"Artist\"}]}\n"
            "[1000,2200](1000,400,0)你(1400,800,0)好\n");
        const ParsedLyrics yrc = LyricsParser::parse(yrcRaw, QStringLiteral("YRC"));
        QVERIFY(yrc.synced);
        QCOMPARE(yrc.lines.size(), 1);
        QCOMPARE(yrc.lines[0].timestampMs, 1000);
        QCOMPARE(yrc.lines[0].durationMs, 2200);
        QCOMPARE(yrc.lines[0].text, QStringLiteral("你好"));
        QCOMPARE(yrc.lines[0].words.size(), 2);
        QCOMPARE(yrc.lines[0].words[0].timestampMs, 1000);
        QCOMPARE(yrc.lines[0].words[0].endTimestampMs, 1400);
        QCOMPARE(yrc.lines[0].words[1].timestampMs, 1400);
        QCOMPARE(yrc.lines[0].words[1].endTimestampMs, 2200);

        // 2. QQ QRC: text(start,dur)
        const QString qrcRaw = QStringLiteral("[2000,1800]星(2000,600)光(2600,900)\n");
        const ParsedLyrics qrc = LyricsParser::parse(qrcRaw, QStringLiteral("QRC"));
        QVERIFY(qrc.synced);
        QCOMPARE(qrc.lines.size(), 1);
        QCOMPARE(qrc.lines[0].text, QStringLiteral("星光"));
        QCOMPARE(qrc.lines[0].words.size(), 2);
        QCOMPARE(qrc.lines[0].words[0].timestampMs, 2000);
        QCOMPARE(qrc.lines[0].words[0].endTimestampMs, 2600);
        QCOMPARE(qrc.lines[0].words[1].timestampMs, 2600);
        QCOMPARE(qrc.lines[0].words[1].endTimestampMs, 3500);

        // 3. KRC-style: [lineStart,lineDur]<offset,dur,0>text
        const QString krcRaw = QStringLiteral("[3000,1500]<0,500,0>夜<500,700,0>空\n");
        const ParsedLyrics krc = LyricsParser::parse(krcRaw, QStringLiteral("KRC"));
        QVERIFY(krc.synced);
        QCOMPARE(krc.lines.size(), 1);
        QCOMPARE(krc.lines[0].text, QStringLiteral("夜空"));
        QCOMPARE(krc.lines[0].words.size(), 2);
        QCOMPARE(krc.lines[0].words[0].timestampMs, 3000);
        QCOMPARE(krc.lines[0].words[0].endTimestampMs, 3500);
        QCOMPARE(krc.lines[0].words[1].timestampMs, 3500);
        QCOMPARE(krc.lines[0].words[1].endTimestampMs, 4200);

        // 4. Enhanced LRC trailing closing tag (no empty ghost token)
        const QString elrcRaw = QStringLiteral("[00:01.00]<00:01.00>A<00:01.40>B<00:01.90>\n");
        const ParsedLyrics elrc = LyricsParser::parse(elrcRaw, QStringLiteral("LRC"));
        QCOMPARE(elrc.lines.size(), 1);
        QCOMPARE(elrc.lines[0].text, QStringLiteral("AB"));
        QCOMPARE(elrc.lines[0].words.size(), 2);
        QCOMPARE(elrc.lines[0].words[1].endTimestampMs, 1900);

        // 5. Final-word interlude tail cap in LyricsController (20s gap capped cleanly)
        LyricsController ctrl;
        ctrl.loadForTrack({}, QStringLiteral("[00:01.00]<00:01.00>Hi\n[00:20.00]<00:20.00>Next\n"));
        const QVariantList tokens = ctrl.data(ctrl.index(0), LyricsController::WordTokensRole).toList();
        QCOMPARE(tokens.size(), 1);
        const qint64 hiEnd = tokens[0].toMap().value(QStringLiteral("endMs")).toLongLong();
        QVERIFY2(hiEnd > 1500 && hiEnd <= 3000,
                 qPrintable(QStringLiteral("Expected capped tail end <= 3000ms, got %1").arg(hiEnd)));

        // 6. Sidecar .yrc auto-discovery alongside audio track
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString songPath = dir.filePath(QStringLiteral("track.flac"));
        QFile yrcFile(dir.filePath(QStringLiteral("track.yrc")));
        QVERIFY(yrcFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        yrcFile.write(yrcRaw.toUtf8());
        yrcFile.close();

        const ParsedLyrics loaded = LyricsParser::loadForTrack(songPath);
        QCOMPARE(loaded.sourceLabel, QStringLiteral("YRC"));
        QCOMPARE(loaded.lines.size(), 1);
        QCOMPARE(loaded.lines[0].words.size(), 2);
    }

    void testIconImageProvider() {
        const QStringList names = IconImageProvider::allIconNames();
        QCOMPARE(names.size(), 52);

        auto hasOpaquePixels = [](const QImage& img) {
            int maxAlpha = 0;
            int nonZeroCount = 0;
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const int a = qAlpha(img.pixel(x, y));
                    if (a > 0) {
                        ++nonZeroCount;
                        maxAlpha = std::max(maxAlpha, a);
                    }
                }
            }
            return std::pair<int, int>{nonZeroCount, maxAlpha};
        };

        IconImageProvider provider;

        // Every registered icon renders valid non-empty pixels in stroked mode
        for (const QString& name : names) {
            for (int px : {32, 48}) {
                const QImage stroked = IconImageProvider::renderIcon(name, px, QColor(240, 240, 250), 1.8, false);
                QCOMPARE(stroked.size(), QSize(px, px));
                QCOMPARE(stroked.format(), QImage::Format_ARGB32_Premultiplied);
                QVERIFY2(hasOpaquePixels(stroked).first > 8,
                         qPrintable(QStringLiteral("Stroked icon empty: %1").arg(name)));
            }
        }

        // Closed-contour icons render non-empty pixels in filled mode
        const QStringList filledIcons = {
            QStringLiteral("play"), QStringLiteral("pause"), QStringLiteral("heart_filled"),
            QStringLiteral("heart"), QStringLiteral("folder"), QStringLiteral("cloud"),
            QStringLiteral("album"), QStringLiteral("maximize")
        };
        for (const QString& name : filledIcons) {
            for (int px : {32, 48}) {
                const QImage filled = IconImageProvider::renderIcon(name, px, QColor(255, 80, 110), 1.8, true);
                QCOMPARE(filled.size(), QSize(px, px));
                QCOMPARE(filled.format(), QImage::Format_ARGB32_Premultiplied);
                QVERIFY2(hasOpaquePixels(filled).first > 8,
                         qPrintable(QStringLiteral("Filled icon empty: %1").arg(name)));
            }
        }

        // Alpha hex color parsing & cache hit behavior via requestImage()
        QSize sz1;
        const QImage semi = provider.requestImage(
            QStringLiteral("settings?s=36&c=a8141a29&sw=18&f=0"), &sz1, {});
        QCOMPARE(sz1, QSize(36, 36));
        QCOMPARE(semi.size(), QSize(36, 36));
        const auto semiStats = hasOpaquePixels(semi);
        QVERIFY(semiStats.first > 12);
        QVERIFY(semiStats.second <= 170); // ~0xA8 = 168

        // Cache hit returns identical size and pixels
        QSize sz2;
        const QImage cached = provider.requestImage(
            QStringLiteral("settings?s=36&c=a8141a29&sw=18&f=0"), &sz2, {});
        QCOMPARE(sz2, QSize(36, 36));
        QCOMPARE(cached, semi);

        // Unknown icon name falls back gracefully without crashing
        QSize sz3;
        const QImage fallback = provider.requestImage(
            QStringLiteral("nonexistent_icon_xyz?s=28&c=ffffffff&sw=18&f=0"), &sz3, {});
        QCOMPARE(sz3, QSize(28, 28));
        QVERIFY(hasOpaquePixels(fallback).first > 8);
    }

    void quickSearchAndDownloadedTracksModel() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dlDir = dir.filePath(QStringLiteral("downloads"));
        const QString otherDir = dir.filePath(QStringLiteral("local_music"));
        QVERIFY(QDir().mkpath(dlDir));
        QVERIFY(QDir().mkpath(otherDir));

        const QString wavOther = QDir(otherDir).filePath(QStringLiteral("Nocturne_Op9.wav"));
        const QString wavDl = QDir(dlDir).filePath(QStringLiteral("Cloud_Downloaded.wav"));
        QVERIFY(writeTinyWav(wavOther, 44100, 0.1));
        QVERIFY(writeTinyWav(wavDl, 44100, 0.1));

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineDownloadDir(dlDir);

        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        CoverArtProvider covers(dir.filePath(QStringLiteral("covers")));
        LibraryScanner scanner(&db, &covers);

        TrackInfo t1;
        t1.filePath = wavOther;
        t1.title = QStringLiteral("Nocturne Op.9");
        t1.artist = QStringLiteral("Chopin");
        t1.album = QStringLiteral("Complete Nocturnes");
        t1.albumArtist = QStringLiteral("Chopin");
        t1.durationMs = 100;
        t1.formatBadge = QStringLiteral("WAV");
        QVERIFY(db.upsertTrack(t1));
        QVERIFY(t1.id >= 0);

        TrackInfo foundById;
        QVERIFY(db.findTrackById(t1.id, foundById));
        QCOMPARE(foundById.title, QStringLiteral("Nocturne Op.9"));
        QVERIFY(!db.findTrackById(999999, foundById));

        LibraryController libCtrl(&db, &scanner, &covers, &settings, nullptr);
        auto* tracksMdl = qobject_cast<TrackListModel*>(libCtrl.tracksModel());
        auto* dlMdl = qobject_cast<TrackListModel*>(libCtrl.downloadedTracksModel());
        QVERIFY(tracksMdl != nullptr);
        QVERIFY(dlMdl != nullptr);
        QCOMPARE(tracksMdl->count(), 1);
        QCOMPARE(dlMdl->count(), 0);

        // quickSearchLocal MUST return matching items without mutating searchQuery or tracksModel
        const QVariantMap res = libCtrl.quickSearchLocal(QStringLiteral("Nocturne"), 5, 3, 2);
        QCOMPARE(res.value(QStringLiteral("totalTracks")).toInt(), 1);
        QCOMPARE(res.value(QStringLiteral("tracks")).toList().size(), 1);
        QCOMPARE(res.value(QStringLiteral("albums")).toList().size(), 1);
        QVERIFY(libCtrl.searchQuery().isEmpty());
        QCOMPARE(tracksMdl->count(), 1);

        // Empty query returns empty results
        const QVariantMap emptyRes = libCtrl.quickSearchLocal(QStringLiteral("   "));
        QCOMPARE(emptyRes.value(QStringLiteral("totalTracks")).toInt(), 0);
        QVERIFY(emptyRes.value(QStringLiteral("tracks")).toList().isEmpty());

        // Ingesting a file into onlineDownloadDir populates downloadedTracksModel
        QVERIFY(libCtrl.ingestLocalFile(wavDl));
        QCOMPARE(tracksMdl->count(), 2);
        QCOMPARE(dlMdl->count(), 1);
        QCOMPARE(dlMdl->tracks().first().filePath, QFileInfo(wavDl).absoluteFilePath());

        // Home summaries fall back to Recently Added before any plays, and expose album summaries
        QVERIFY(!libCtrl.hasRecentPlays());
        QCOMPARE(libCtrl.recentTracksSummary().size(), 2);
        QVERIFY(libCtrl.recentAlbumsSummary().size() >= 1);

        // Recording a playback switches Continue Listening to Recently Played
        QVERIFY(db.recordPlayback(t1.id));
        libCtrl.refreshAll();
        QVERIFY(libCtrl.hasRecentPlays());
        QCOMPARE(libCtrl.recentTracksSummary().size(), 1);
        QCOMPARE(libCtrl.recentTracksSummary().first().toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Nocturne Op.9"));
    }

    void crossSourceUnificationAndLocalUpgrade() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("legacy.db"));

        // 1. Seed a legacy Phase 1/2 SQLite database WITHOUT source/remote_id/cover_url columns
        {
            sqlite3* rawDb = nullptr;
            QCOMPARE(sqlite3_open(dbPath.toUtf8().constData(), &rawDb), SQLITE_OK);
            static constexpr const char* kLegacySql =
                "CREATE TABLE tracks ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, file_path TEXT NOT NULL, is_cue INTEGER NOT NULL DEFAULT 0,"
                "cue_path TEXT NOT NULL DEFAULT '', start_ms INTEGER NOT NULL DEFAULT 0, end_ms INTEGER NOT NULL DEFAULT 0,"
                "title TEXT NOT NULL DEFAULT '', artist TEXT NOT NULL DEFAULT '', album TEXT NOT NULL DEFAULT '',"
                "album_artist TEXT NOT NULL DEFAULT '', track_number INTEGER NOT NULL DEFAULT 0,"
                "disc_number INTEGER NOT NULL DEFAULT 0, year INTEGER NOT NULL DEFAULT 0, genre TEXT NOT NULL DEFAULT '',"
                "duration_ms INTEGER NOT NULL DEFAULT 0, sample_rate INTEGER NOT NULL DEFAULT 0,"
                "bit_depth INTEGER NOT NULL DEFAULT 0, bitrate_kbps INTEGER NOT NULL DEFAULT 0,"
                "channels INTEGER NOT NULL DEFAULT 2, codec TEXT NOT NULL DEFAULT '', format_badge TEXT NOT NULL DEFAULT '',"
                "is_dsd INTEGER NOT NULL DEFAULT 0, rg_track_gain REAL NOT NULL DEFAULT 0.0, rg_track_peak REAL NOT NULL DEFAULT 1.0,"
                "rg_album_gain REAL NOT NULL DEFAULT 0.0, rg_album_peak REAL NOT NULL DEFAULT 1.0,"
                "has_rg_track INTEGER NOT NULL DEFAULT 0, has_rg_album INTEGER NOT NULL DEFAULT 0,"
                "cover_hash TEXT NOT NULL DEFAULT '', file_size INTEGER NOT NULL DEFAULT 0,"
                "file_mtime INTEGER NOT NULL DEFAULT 0, is_favorite INTEGER NOT NULL DEFAULT 0,"
                "play_count INTEGER NOT NULL DEFAULT 0, last_played_at INTEGER NOT NULL DEFAULT 0,"
                "added_at INTEGER NOT NULL DEFAULT 0, UNIQUE(file_path, start_ms));";
            QCOMPARE(sqlite3_exec(rawDb, kLegacySql, nullptr, nullptr, nullptr), SQLITE_OK);
            sqlite3_close(rawDb);
        }

        LibraryDatabase db;
        QVERIFY(db.open(dbPath));

        const QString dlDir = dir.filePath(QStringLiteral("downloads"));
        const QString locDir = dir.filePath(QStringLiteral("local"));
        QVERIFY(QDir().mkpath(dlDir));
        QVERIFY(QDir().mkpath(locDir));
        const QString localWav = QDir(locDir).filePath(QStringLiteral("local_piece.wav"));
        const QString downloadedWav = QDir(dlDir).filePath(QStringLiteral("online_2025.wav"));
        QVERIFY(writeTinyWav(localWav, 44100, 0.1));
        QVERIFY(writeTinyWav(downloadedWav, 48000, 0.1));

        TrackInfo loc;
        loc.filePath = localWav;
        loc.title = QStringLiteral("Local Sonata");
        loc.artist = QStringLiteral("Beethoven");
        loc.album = QStringLiteral("Sonatas");
        loc.albumArtist = QStringLiteral("Beethoven");
        loc.durationMs = 100;
        loc.formatBadge = QStringLiteral("WAV");
        QVERIFY(db.upsertTrack(loc));

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineDownloadDir(dlDir);
        CoverArtProvider covers(dir.filePath(QStringLiteral("covers")));
        LibraryScanner scanner(&db, &covers);
        LibraryController libCtrl(&db, &scanner, &covers, &settings, nullptr);

        // 2. Favorite & play an online track; verify isolation from local library counts
        QVariantMap onlineMap;
        onlineMap.insert(QStringLiteral("source"), QStringLiteral("netease"));
        onlineMap.insert(QStringLiteral("remoteId"), QStringLiteral("888001"));
        onlineMap.insert(QStringLiteral("title"), QStringLiteral("Clair de Lune"));
        onlineMap.insert(QStringLiteral("artist"), QStringLiteral("Debussy"));
        onlineMap.insert(QStringLiteral("album"), QStringLiteral("Suite bergamasque"));
        onlineMap.insert(QStringLiteral("durationMs"), 240000);
        onlineMap.insert(QStringLiteral("formatBadge"), QStringLiteral("Hi-Res"));
        onlineMap.insert(QStringLiteral("coverUrl"), QStringLiteral("https://example.com/clair.jpg"));

        QVERIFY(libCtrl.toggleTrackFavorite(onlineMap));
        QVERIFY(libCtrl.isTrackFavorite(QStringLiteral("netease"), QStringLiteral("888001")));

        TrackInfo persistedOnline;
        QVERIFY(db.findOnlineTrack(QStringLiteral("netease"), QStringLiteral("888001"), persistedOnline));
        const int64_t onlineRowId = persistedOnline.id;
        QVERIFY(onlineRowId >= 0);
        QVERIFY(persistedOnline.isOnline());
        QVERIFY(db.recordOnlinePlayback(persistedOnline));

        // Online shadow track must NOT pollute local totalTrackCount, queryTracks, or queryAlbums
        QCOMPARE(db.totalTrackCount(), 1);
        QCOMPARE(db.queryTracks().size(), 1);
        QCOMPARE(db.queryAlbums().size(), 1);

        // Removing all local watched folders must NOT wipe online favorites/playlists
        QVERIFY(db.removeTracksOutsideFolders({}));
        QCOMPARE(db.totalTrackCount(), 0);
        QVERIFY(db.isOnlineTrackFavorite(QStringLiteral("netease"), QStringLiteral("888001")));

        // Restore local track and create mixed local + online playlist
        QVERIFY(db.upsertTrack(loc));
        const int64_t plId = libCtrl.createPlaylist(QStringLiteral("Mixed Mix"));
        QVERIFY(plId >= 0);
        libCtrl.addTrackToPlaylist(plId, loc.id);
        libCtrl.addTrackMapToPlaylist(plId, onlineMap);
        const QVector<TrackInfo> mixedTracks = db.queryPlaylistTracks(plId);
        QCOMPARE(mixedTracks.size(), 2);
        QCOMPARE(mixedTracks[1].isOnline(), true);

        // 3. Pin & unpin online playlists
        QVERIFY(libCtrl.pinOnlinePlaylist(
            QStringLiteral("netease"), QStringLiteral("24381616"),
            QStringLiteral("Midnight Piano"), QStringLiteral("Virela"), QStringLiteral("https://example.com/pl.jpg"), 20));
        QVERIFY(libCtrl.isOnlinePlaylistPinned(QStringLiteral("netease"), QStringLiteral("24381616")));
        QCOMPARE(libCtrl.pinnedPlaylists().size(), 1);

        // 4. Seamless download upgrade: online row upgrades in-place preserving id, favorite, play_count, and playlist items
        QVERIFY(libCtrl.ingestDownloadedTrack(downloadedWav, QStringLiteral("netease"), QStringLiteral("888001")));
        TrackInfo upgraded;
        QVERIFY(db.findTrackById(onlineRowId, upgraded));
        QCOMPARE(upgraded.source, QStringLiteral("local"));
        QCOMPARE(upgraded.remoteId, QStringLiteral("888001"));
        QCOMPARE(upgraded.filePath, QFileInfo(downloadedWav).absoluteFilePath());
        QVERIFY(upgraded.isFavorite);
        QCOMPARE(upgraded.playCount, 1);

        const QVector<TrackInfo> plAfterUpgrade = db.queryPlaylistTracks(plId);
        QCOMPARE(plAfterUpgrade.size(), 2);
        QCOMPARE(plAfterUpgrade[1].id, onlineRowId);
        QCOMPARE(plAfterUpgrade[1].source, QStringLiteral("local"));
        QVERIFY(libCtrl.unpinOnlinePlaylist(QStringLiteral("netease"), QStringLiteral("24381616")));
        QVERIFY(!libCtrl.isOnlinePlaylistPinned(QStringLiteral("netease"), QStringLiteral("24381616")));

        // 5. Lazy model loading & memory reclamation
        auto* tracksMdl = qobject_cast<TrackListModel*>(libCtrl.tracksModel());
        auto* albumsMdl = qobject_cast<AlbumListModel*>(libCtrl.albumsModel());
        QVERIFY(tracksMdl && albumsMdl);
        QVERIFY(tracksMdl->count() >= 1);
        QVERIFY(albumsMdl->count() >= 1);

        libCtrl.releaseInactiveModels(QStringLiteral("online"));
        QCOMPARE(tracksMdl->count(), 0);
        QCOMPARE(albumsMdl->count(), 0);

        // Accessing model re-hydrates automatically on demand
        QCOMPARE(libCtrl.tracksModel(), tracksMdl);
        QVERIFY(tracksMdl->count() >= 1);
    }
};

QTEST_MAIN(TestLibraryAndParsers)
#include "test_library_and_parsers.moc"
