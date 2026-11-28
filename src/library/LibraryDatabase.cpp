#include "library/LibraryDatabase.h"

#include "sqlite3.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace virela {

namespace {

class StmtGuard {
public:
    explicit StmtGuard(sqlite3_stmt* s = nullptr) : stmt_(s) {}
    ~StmtGuard() {
        if (stmt_) sqlite3_finalize(stmt_);
    }
    sqlite3_stmt* get() const { return stmt_; }
    explicit operator bool() const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

void bindText(sqlite3_stmt* stmt, int idx, const QString& val) {
    const QByteArray utf8 = val.toUtf8();
    sqlite3_bind_text(stmt, idx, utf8.constData(), static_cast<int>(utf8.size()), SQLITE_TRANSIENT);
}

QString colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* p = sqlite3_column_text(stmt, col);
    return p ? QString::fromUtf8(reinterpret_cast<const char*>(p)) : QString{};
}

QString escapeFtsTrigramQuery(const QString& q) {
    QString clean = q;
    clean.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(clean);
}

TrackInfo readTrackRow(sqlite3_stmt* s) {
    TrackInfo t;
    t.id = sqlite3_column_int64(s, 0);
    t.filePath = colText(s, 1);
    t.isCue = sqlite3_column_int(s, 2) != 0;
    t.cuePath = colText(s, 3);
    t.startMs = sqlite3_column_int64(s, 4);
    t.endMs = sqlite3_column_int64(s, 5);
    t.title = colText(s, 6);
    t.artist = colText(s, 7);
    t.album = colText(s, 8);
    t.albumArtist = colText(s, 9);
    t.trackNumber = sqlite3_column_int(s, 10);
    t.discNumber = sqlite3_column_int(s, 11);
    t.year = sqlite3_column_int(s, 12);
    t.genre = colText(s, 13);
    t.durationMs = sqlite3_column_int64(s, 14);
    t.sampleRate = sqlite3_column_int(s, 15);
    t.bitDepth = sqlite3_column_int(s, 16);
    t.bitrateKbps = sqlite3_column_int(s, 17);
    t.channels = sqlite3_column_int(s, 18);
    t.codec = colText(s, 19);
    t.formatBadge = colText(s, 20);
    t.isDsd = sqlite3_column_int(s, 21) != 0;
    t.rgTrackGainDb = sqlite3_column_double(s, 22);
    t.rgTrackPeak = sqlite3_column_double(s, 23);
    t.rgAlbumGainDb = sqlite3_column_double(s, 24);
    t.rgAlbumPeak = sqlite3_column_double(s, 25);
    t.hasRgTrack = sqlite3_column_int(s, 26) != 0;
    t.hasRgAlbum = sqlite3_column_int(s, 27) != 0;
    t.coverHash = colText(s, 28);
    t.fileSize = sqlite3_column_int64(s, 29);
    t.fileMtime = sqlite3_column_int64(s, 30);
    t.isFavorite = sqlite3_column_int(s, 31) != 0;
    t.playCount = sqlite3_column_int(s, 32);
    t.lastPlayedAt = sqlite3_column_int64(s, 33);
    t.addedAt = sqlite3_column_int64(s, 34);
    t.source = colText(s, 35);
    if (t.source.isEmpty()) t.source = QStringLiteral("local");
    t.remoteId = colText(s, 36);
    t.remoteAlbumId = colText(s, 37);
    t.remoteArtistId = colText(s, 38);
    t.coverUrl = colText(s, 39);
    if (t.source != QStringLiteral("local")) {
        t.filePath.clear();
    }
    return t;
}

constexpr const char* kSelectTrackColumns =
    "SELECT t.id, t.file_path, t.is_cue, t.cue_path, t.start_ms, t.end_ms, "
    "t.title, t.artist, t.album, t.album_artist, t.track_number, t.disc_number, "
    "t.year, t.genre, t.duration_ms, t.sample_rate, t.bit_depth, t.bitrate_kbps, "
    "t.channels, t.codec, t.format_badge, t.is_dsd, t.rg_track_gain, t.rg_track_peak, "
    "t.rg_album_gain, t.rg_album_peak, t.has_rg_track, t.has_rg_album, "
    "t.cover_hash, t.file_size, t.file_mtime, t.is_favorite, t.play_count, "
    "t.last_played_at, t.added_at, t.source, t.remote_id, t.remote_album_id, "
    "t.remote_artist_id, t.cover_url ";

QString syntheticOnlineFilePath(const QString& source, const QString& remoteId) {
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    return QStringLiteral("online://%1/%2").arg(src, remoteId.trimmed());
}

} // namespace

LibraryDatabase::~LibraryDatabase() {
    close();
}

bool LibraryDatabase::open(const QString& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }

    if (dbPath != QStringLiteral(":memory:")) {
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
    }

    const QByteArray utf8 = dbPath.toUtf8();
    if (sqlite3_open_v2(utf8.constData(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Strict low-memory pragmas: cap page cache at 4 MB and disable mmap inflation
    execSql("PRAGMA journal_mode=WAL;");
    execSql("PRAGMA synchronous=NORMAL;");
    execSql("PRAGMA cache_size=-4000;");
    execSql("PRAGMA mmap_size=0;");
    execSql("PRAGMA temp_store=MEMORY;");
    execSql("PRAGMA foreign_keys=ON;");

    return initSchema();
}

void LibraryDatabase::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool LibraryDatabase::execSql(const char* sql) const {
    if (!db_) return false;
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
    return rc == SQLITE_OK;
}

bool LibraryDatabase::initSchema() {
    static constexpr const char* kSchemaSql = R"SQL(
        CREATE TABLE IF NOT EXISTS tracks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            is_cue INTEGER NOT NULL DEFAULT 0,
            cue_path TEXT NOT NULL DEFAULT '',
            start_ms INTEGER NOT NULL DEFAULT 0,
            end_ms INTEGER NOT NULL DEFAULT 0,
            title TEXT NOT NULL DEFAULT '',
            artist TEXT NOT NULL DEFAULT '',
            album TEXT NOT NULL DEFAULT '',
            album_artist TEXT NOT NULL DEFAULT '',
            track_number INTEGER NOT NULL DEFAULT 0,
            disc_number INTEGER NOT NULL DEFAULT 0,
            year INTEGER NOT NULL DEFAULT 0,
            genre TEXT NOT NULL DEFAULT '',
            duration_ms INTEGER NOT NULL DEFAULT 0,
            sample_rate INTEGER NOT NULL DEFAULT 0,
            bit_depth INTEGER NOT NULL DEFAULT 0,
            bitrate_kbps INTEGER NOT NULL DEFAULT 0,
            channels INTEGER NOT NULL DEFAULT 2,
            codec TEXT NOT NULL DEFAULT '',
            format_badge TEXT NOT NULL DEFAULT '',
            is_dsd INTEGER NOT NULL DEFAULT 0,
            rg_track_gain REAL NOT NULL DEFAULT 0.0,
            rg_track_peak REAL NOT NULL DEFAULT 1.0,
            rg_album_gain REAL NOT NULL DEFAULT 0.0,
            rg_album_peak REAL NOT NULL DEFAULT 1.0,
            has_rg_track INTEGER NOT NULL DEFAULT 0,
            has_rg_album INTEGER NOT NULL DEFAULT 0,
            cover_hash TEXT NOT NULL DEFAULT '',
            file_size INTEGER NOT NULL DEFAULT 0,
            file_mtime INTEGER NOT NULL DEFAULT 0,
            is_favorite INTEGER NOT NULL DEFAULT 0,
            play_count INTEGER NOT NULL DEFAULT 0,
            last_played_at INTEGER NOT NULL DEFAULT 0,
            added_at INTEGER NOT NULL DEFAULT 0,
            source TEXT NOT NULL DEFAULT 'local',
            remote_id TEXT NOT NULL DEFAULT '',
            remote_album_id TEXT NOT NULL DEFAULT '',
            remote_artist_id TEXT NOT NULL DEFAULT '',
            cover_url TEXT NOT NULL DEFAULT '',
            UNIQUE(file_path, start_ms)
        );

        CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album, album_artist);
        CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist);
        CREATE INDEX IF NOT EXISTS idx_tracks_fav ON tracks(is_favorite);
        CREATE INDEX IF NOT EXISTS idx_tracks_recent ON tracks(last_played_at DESC);
        CREATE INDEX IF NOT EXISTS idx_tracks_path ON tracks(file_path);

        CREATE VIRTUAL TABLE IF NOT EXISTS tracks_fts USING fts5(
            title, artist, album, album_artist, file_path,
            content='tracks', content_rowid='id',
            tokenize='trigram'
        );

        CREATE TRIGGER IF NOT EXISTS tracks_ai AFTER INSERT ON tracks BEGIN
            INSERT INTO tracks_fts(rowid, title, artist, album, album_artist, file_path)
            VALUES (new.id, new.title, new.artist, new.album, new.album_artist, new.file_path);
        END;

        CREATE TRIGGER IF NOT EXISTS tracks_ad AFTER DELETE ON tracks BEGIN
            INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, album_artist, file_path)
            VALUES ('delete', old.id, old.title, old.artist, old.album, old.album_artist, old.file_path);
        END;

        CREATE TRIGGER IF NOT EXISTS tracks_au AFTER UPDATE ON tracks BEGIN
            INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, album_artist, file_path)
            VALUES ('delete', old.id, old.title, old.artist, old.album, old.album_artist, old.file_path);
            INSERT INTO tracks_fts(rowid, title, artist, album, album_artist, file_path)
            VALUES (new.id, new.title, new.artist, new.album, new.album_artist, new.file_path);
        END;

        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            created_at INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS playlist_items (
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            PRIMARY KEY (playlist_id, position)
        );

        CREATE TABLE IF NOT EXISTS pinned_online_playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source TEXT NOT NULL DEFAULT 'netease',
            remote_id TEXT NOT NULL,
            name TEXT NOT NULL,
            creator TEXT NOT NULL DEFAULT '',
            cover_url TEXT NOT NULL DEFAULT '',
            track_count INTEGER NOT NULL DEFAULT 0,
            pinned_at INTEGER NOT NULL DEFAULT 0,
            UNIQUE(source, remote_id)
        );
    )SQL";

    if (!execSql(kSchemaSql)) {
        return false;
    }

    // Idempotent column migration for existing databases created in Phases 1–2
    QSet<QString> existingCols;
    {
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, "PRAGMA table_info(tracks);", -1, &raw, nullptr) == SQLITE_OK) {
            StmtGuard s(raw);
            while (sqlite3_step(s.get()) == SQLITE_ROW) {
                existingCols.insert(colText(s.get(), 1));
            }
        }
    }

    struct ColMigration {
        const char* name;
        const char* alterSql;
    };
    static constexpr ColMigration kMigrations[] = {
        {"source",           "ALTER TABLE tracks ADD COLUMN source TEXT NOT NULL DEFAULT 'local';"},
        {"remote_id",        "ALTER TABLE tracks ADD COLUMN remote_id TEXT NOT NULL DEFAULT '';"},
        {"remote_album_id",  "ALTER TABLE tracks ADD COLUMN remote_album_id TEXT NOT NULL DEFAULT '';"},
        {"remote_artist_id", "ALTER TABLE tracks ADD COLUMN remote_artist_id TEXT NOT NULL DEFAULT '';"},
        {"cover_url",        "ALTER TABLE tracks ADD COLUMN cover_url TEXT NOT NULL DEFAULT '';"}
    };
    for (const auto& m : kMigrations) {
        if (!existingCols.contains(QString::fromLatin1(m.name))) {
            if (!execSql(m.alterSql)) {
                return false;
            }
        }
    }

    return execSql("CREATE INDEX IF NOT EXISTS idx_tracks_source_remote ON tracks(source, remote_id);");
}

bool LibraryDatabase::upsertTracksBatch(QVector<TrackInfo>& tracks) {
    if (tracks.isEmpty()) return true;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    if (!execSql("BEGIN IMMEDIATE;")) {
        return false;
    }

    static constexpr const char* kUpsertSql = R"SQL(
        INSERT INTO tracks (
            file_path, is_cue, cue_path, start_ms, end_ms,
            title, artist, album, album_artist, track_number, disc_number,
            year, genre, duration_ms, sample_rate, bit_depth, bitrate_kbps,
            channels, codec, format_badge, is_dsd,
            rg_track_gain, rg_track_peak, rg_album_gain, rg_album_peak,
            has_rg_track, has_rg_album, cover_hash, file_size, file_mtime, added_at,
            source, remote_id
        ) VALUES (
            ?1, ?2, ?3, ?4, ?5,
            ?6, ?7, ?8, ?9, ?10, ?11,
            ?12, ?13, ?14, ?15, ?16, ?17,
            ?18, ?19, ?20, ?21,
            ?22, ?23, ?24, ?25,
            ?26, ?27, ?28, ?29, ?30, ?31,
            'local', ?32
        )
        ON CONFLICT(file_path, start_ms) DO UPDATE SET
            source='local',
            remote_id=CASE WHEN excluded.remote_id != '' THEN excluded.remote_id ELSE tracks.remote_id END,
            is_cue=excluded.is_cue,
            cue_path=excluded.cue_path,
            end_ms=excluded.end_ms,
            title=excluded.title,
            artist=excluded.artist,
            album=excluded.album,
            album_artist=excluded.album_artist,
            track_number=excluded.track_number,
            disc_number=excluded.disc_number,
            year=excluded.year,
            genre=excluded.genre,
            duration_ms=excluded.duration_ms,
            sample_rate=excluded.sample_rate,
            bit_depth=excluded.bit_depth,
            bitrate_kbps=excluded.bitrate_kbps,
            channels=excluded.channels,
            codec=excluded.codec,
            format_badge=excluded.format_badge,
            is_dsd=excluded.is_dsd,
            rg_track_gain=excluded.rg_track_gain,
            rg_track_peak=excluded.rg_track_peak,
            rg_album_gain=excluded.rg_album_gain,
            rg_album_peak=excluded.rg_album_peak,
            has_rg_track=excluded.has_rg_track,
            has_rg_album=excluded.has_rg_album,
            cover_hash=CASE WHEN excluded.cover_hash != '' THEN excluded.cover_hash ELSE tracks.cover_hash END,
            file_size=excluded.file_size,
            file_mtime=excluded.file_mtime
        RETURNING id;
    )SQL";

    {
        sqlite3_stmt* rawStmt = nullptr;
        if (sqlite3_prepare_v2(db_, kUpsertSql, -1, &rawStmt, nullptr) != SQLITE_OK) {
            execSql("ROLLBACK;");
            return false;
        }
        StmtGuard stmt(rawStmt);
        const int64_t now = QDateTime::currentSecsSinceEpoch();

        for (TrackInfo& t : tracks) {
            sqlite3_reset(stmt.get());
            sqlite3_clear_bindings(stmt.get());

            bindText(stmt.get(), 1, t.filePath);
            sqlite3_bind_int(stmt.get(), 2, t.isCue ? 1 : 0);
            bindText(stmt.get(), 3, t.cuePath);
            sqlite3_bind_int64(stmt.get(), 4, t.startMs);
            sqlite3_bind_int64(stmt.get(), 5, t.endMs);
            bindText(stmt.get(), 6, t.title);
            bindText(stmt.get(), 7, t.artist);
            bindText(stmt.get(), 8, t.album);
            bindText(stmt.get(), 9, t.albumArtist);
            sqlite3_bind_int(stmt.get(), 10, t.trackNumber);
            sqlite3_bind_int(stmt.get(), 11, t.discNumber);
            sqlite3_bind_int(stmt.get(), 12, t.year);
            bindText(stmt.get(), 13, t.genre);
            sqlite3_bind_int64(stmt.get(), 14, t.durationMs);
            sqlite3_bind_int(stmt.get(), 15, t.sampleRate);
            sqlite3_bind_int(stmt.get(), 16, t.bitDepth);
            sqlite3_bind_int(stmt.get(), 17, t.bitrateKbps);
            sqlite3_bind_int(stmt.get(), 18, t.channels);
            bindText(stmt.get(), 19, t.codec);
            bindText(stmt.get(), 20, t.formatBadge);
            sqlite3_bind_int(stmt.get(), 21, t.isDsd ? 1 : 0);
            sqlite3_bind_double(stmt.get(), 22, t.rgTrackGainDb);
            sqlite3_bind_double(stmt.get(), 23, t.rgTrackPeak);
            sqlite3_bind_double(stmt.get(), 24, t.rgAlbumGainDb);
            sqlite3_bind_double(stmt.get(), 25, t.rgAlbumPeak);
            sqlite3_bind_int(stmt.get(), 26, t.hasRgTrack ? 1 : 0);
            sqlite3_bind_int(stmt.get(), 27, t.hasRgAlbum ? 1 : 0);
            bindText(stmt.get(), 28, t.coverHash);
            sqlite3_bind_int64(stmt.get(), 29, t.fileSize);
            sqlite3_bind_int64(stmt.get(), 30, t.fileMtime);
            sqlite3_bind_int64(stmt.get(), 31, t.addedAt > 0 ? t.addedAt : now);
            bindText(stmt.get(), 32, t.remoteId);

            if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                t.id = sqlite3_column_int64(stmt.get(), 0);
                // Drain statement to SQLITE_DONE so RETURNING completes cleanly
                while (sqlite3_step(stmt.get()) == SQLITE_ROW) {}
            }
        }
    }

    return execSql("COMMIT;");
}

bool LibraryDatabase::upsertTrack(TrackInfo& track) {
    QVector<TrackInfo> batch = {track};
    const bool ok = upsertTracksBatch(batch);
    if (ok && !batch.isEmpty()) {
        track.id = batch.first().id;
    }
    return ok;
}

bool LibraryDatabase::removeTracksForBackingFile(const QString& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM tracks WHERE file_path = ?1;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, filePath);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::removeTracksOutsideFolders(const QStringList& watchedFolders) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    if (watchedFolders.isEmpty()) {
        return execSql("DELETE FROM tracks WHERE source = 'local';");
    }
    QString sql = QStringLiteral("DELETE FROM tracks WHERE source = 'local' AND NOT (");
    for (int i = 0; i < watchedFolders.size(); ++i) {
        if (i > 0) sql += QStringLiteral(" OR ");
        sql += QStringLiteral("file_path LIKE ?%1").arg(i + 1);
    }
    sql += QStringLiteral(");");

    sqlite3_stmt* raw = nullptr;
    const QByteArray sqlUtf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, sqlUtf8.constData(), -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    for (int i = 0; i < watchedFolders.size(); ++i) {
        QString prefix = QDir::cleanPath(watchedFolders[i]);
        if (!prefix.endsWith(QLatin1Char('/'))) prefix += QLatin1Char('/');
        bindText(s.get(), i + 1, prefix + QLatin1Char('%'));
    }
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

QHash<QString, std::pair<int64_t, int64_t>> LibraryDatabase::loadFileFingerprintIndex() const {
    QHash<QString, std::pair<int64_t, int64_t>> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT DISTINCT file_path, file_mtime, file_size FROM tracks WHERE source = 'local';", -1, &raw, nullptr) != SQLITE_OK) {
        return out;
    }
    StmtGuard s(raw);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.insert(colText(s.get(), 0), {sqlite3_column_int64(s.get(), 1), sqlite3_column_int64(s.get(), 2)});
    }
    return out;
}

int LibraryDatabase::totalTrackCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM tracks WHERE source = 'local';", -1, &raw, nullptr) != SQLITE_OK) {
        return 0;
    }
    StmtGuard s(raw);
    return (sqlite3_step(s.get()) == SQLITE_ROW) ? sqlite3_column_int(s.get(), 0) : 0;
}

QVector<TrackInfo> LibraryDatabase::queryTracks(const QString& searchQuery,
                                                const QString& sortBy,
                                                bool ascending) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    QString orderClause = QStringLiteral(" ORDER BY ");
    const QString dir = ascending ? QStringLiteral("ASC") : QStringLiteral("DESC");
    if (sortBy == QStringLiteral("title")) {
        orderClause += QStringLiteral("t.title COLLATE NOCASE %1").arg(dir);
    } else if (sortBy == QStringLiteral("album")) {
        orderClause += QStringLiteral("t.album COLLATE NOCASE %1, t.disc_number ASC, t.track_number ASC").arg(dir);
    } else if (sortBy == QStringLiteral("duration")) {
        orderClause += QStringLiteral("t.duration_ms %1").arg(dir);
    } else if (sortBy == QStringLiteral("added")) {
        orderClause += QStringLiteral("t.added_at %1").arg(dir);
    } else {
        orderClause += QStringLiteral("t.artist COLLATE NOCASE %1, t.album COLLATE NOCASE ASC, t.disc_number ASC, t.track_number ASC").arg(dir);
    }

    const QString q = searchQuery.trimmed();
    QString sql = QString::fromLatin1(kSelectTrackColumns);

    bool useFts = false;
    bool useLike = false;
    if (q.size() >= 3) {
        useFts = true;
        sql += QStringLiteral("FROM tracks t JOIN tracks_fts f ON t.id = f.rowid WHERE tracks_fts MATCH ?1 AND t.source = 'local'");
    } else if (!q.isEmpty()) {
        useLike = true;
        sql += QStringLiteral("FROM tracks t WHERE t.source = 'local' AND (t.title LIKE ?1 OR t.artist LIKE ?1 OR t.album LIKE ?1)");
    } else {
        sql += QStringLiteral("FROM tracks t WHERE t.source = 'local'");
    }
    sql += orderClause + QStringLiteral(";");

    sqlite3_stmt* raw = nullptr;
    const QByteArray sqlUtf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, sqlUtf8.constData(), -1, &raw, nullptr) != SQLITE_OK) {
        return out;
    }
    StmtGuard s(raw);
    if (useFts) {
        bindText(s.get(), 1, escapeFtsTrigramQuery(q));
    } else if (useLike) {
        bindText(s.get(), 1, QStringLiteral("%") + q + QStringLiteral("%"));
    }

    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryAlbumTracks(const QString& album, const QString& albumArtist) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = 'local' AND t.album = ?1");
    if (!albumArtist.isEmpty()) {
        sql += QStringLiteral(" AND t.album_artist = ?2");
    }
    sql += QStringLiteral(" ORDER BY t.disc_number ASC, t.track_number ASC, t.start_ms ASC;");

    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    bindText(s.get(), 1, album);
    if (!albumArtist.isEmpty()) bindText(s.get(), 2, albumArtist);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryArtistTracks(const QString& artist) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = 'local' AND (t.artist = ?1 OR t.album_artist = ?1) ORDER BY t.year DESC, t.album COLLATE NOCASE ASC, t.disc_number ASC, t.track_number ASC;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    bindText(s.get(), 1, artist);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryFolderTracks(const QString& folderPrefix) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    QString prefix = QDir::cleanPath(folderPrefix);
    if (!prefix.endsWith(QLatin1Char('/'))) prefix += QLatin1Char('/');

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = 'local' AND t.file_path LIKE ?1 ORDER BY t.file_path ASC, t.start_ms ASC;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    bindText(s.get(), 1, prefix + QLatin1Char('%'));
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryFavoriteTracks() const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.is_favorite = 1 ORDER BY t.added_at DESC;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryRecentlyPlayed(int limit) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.last_played_at > 0 ORDER BY t.last_played_at DESC LIMIT ?1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    sqlite3_bind_int(s.get(), 1, std::max(1, limit));
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

QVector<TrackInfo> LibraryDatabase::queryRecentlyAdded(int limit) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = 'local' ORDER BY t.added_at DESC LIMIT ?1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    sqlite3_bind_int(s.get(), 1, std::max(1, limit));
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

bool LibraryDatabase::findTrackByPath(const QString& filePath, TrackInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.file_path = ?1 ORDER BY t.start_ms ASC LIMIT 1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    bindText(s.get(), 1, filePath);
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        out = readTrackRow(s.get());
        return true;
    }
    return false;
}

bool LibraryDatabase::findTrackById(int64_t trackId, TrackInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || trackId < 0) return false;
    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.id = ?1 LIMIT 1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, trackId);
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        out = readTrackRow(s.get());
        return true;
    }
    return false;
}

QVector<AlbumSummary> LibraryDatabase::queryAlbums(const QString& searchQuery) const {
    QVector<AlbumSummary> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString q = searchQuery.trimmed();
    QString sql = QStringLiteral(
        "SELECT album, album_artist, MAX(year), COUNT(*), SUM(duration_ms), "
        "MAX(cover_hash), MAX(format_badge) FROM tracks WHERE source = 'local' ");
    if (!q.isEmpty()) {
        sql += QStringLiteral("AND (album LIKE ?1 OR album_artist LIKE ?1 OR artist LIKE ?1) ");
    }
    sql += QStringLiteral("GROUP BY album, album_artist ORDER BY album COLLATE NOCASE ASC;");

    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    if (!q.isEmpty()) {
        bindText(s.get(), 1, QStringLiteral("%") + q + QStringLiteral("%"));
    }

    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        AlbumSummary a;
        a.album = colText(s.get(), 0);
        a.albumArtist = colText(s.get(), 1);
        a.year = sqlite3_column_int(s.get(), 2);
        a.trackCount = sqlite3_column_int(s.get(), 3);
        a.totalDurationMs = sqlite3_column_int64(s.get(), 4);
        a.coverHash = colText(s.get(), 5);
        a.formatBadge = colText(s.get(), 6);
        out.push_back(std::move(a));
    }
    return out;
}

QVector<ArtistSummary> LibraryDatabase::queryArtists(const QString& searchQuery) const {
    QVector<ArtistSummary> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString q = searchQuery.trimmed();
    QString sql = QStringLiteral(
        "SELECT COALESCE(NULLIF(album_artist, ''), artist) AS grp_artist, "
        "COUNT(DISTINCT album), COUNT(*), MAX(cover_hash) FROM tracks WHERE source = 'local' ");
    if (!q.isEmpty()) {
        sql += QStringLiteral("AND (artist LIKE ?1 OR album_artist LIKE ?1) ");
    }
    sql += QStringLiteral("GROUP BY grp_artist ORDER BY grp_artist COLLATE NOCASE ASC;");

    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    if (!q.isEmpty()) {
        bindText(s.get(), 1, QStringLiteral("%") + q + QStringLiteral("%"));
    }

    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        ArtistSummary a;
        a.artist = colText(s.get(), 0);
        a.albumCount = sqlite3_column_int(s.get(), 1);
        a.trackCount = sqlite3_column_int(s.get(), 2);
        a.coverHash = colText(s.get(), 3);
        out.push_back(std::move(a));
    }
    return out;
}

bool LibraryDatabase::setFavorite(int64_t trackId, bool favorite) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE tracks SET is_favorite = ?1 WHERE id = ?2;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    sqlite3_bind_int(s.get(), 1, favorite ? 1 : 0);
    sqlite3_bind_int64(s.get(), 2, trackId);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::recordPlayback(int64_t trackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE tracks SET play_count = play_count + 1, last_played_at = ?1 WHERE id = ?2;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, QDateTime::currentSecsSinceEpoch());
    sqlite3_bind_int64(s.get(), 2, trackId);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

int64_t LibraryDatabase::findUpgradedLocalIdLocked(const QString& remoteId) const {
    if (!db_ || remoteId.trimmed().isEmpty()) return -1;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT id FROM tracks WHERE source = 'local' AND remote_id = ?1 LIMIT 1;",
                           -1, &raw, nullptr) != SQLITE_OK) {
        return -1;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, remoteId.trimmed());
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        return sqlite3_column_int64(s.get(), 0);
    }
    return -1;
}

int64_t LibraryDatabase::upsertOnlineTrackLocked(const TrackInfo& track) {
    if (!db_ || track.remoteId.trimmed().isEmpty()) return -1;

    const QString src = track.source.trimmed().isEmpty() ? QStringLiteral("netease") : track.source.trimmed();
    const QString rid = track.remoteId.trimmed();

    // If this remote track was already upgraded to a local lossless file, reuse that row id
    const int64_t upgradedId = findUpgradedLocalIdLocked(rid);
    if (upgradedId >= 0) {
        return upgradedId;
    }

    static constexpr const char* kOnlineUpsertSql = R"SQL(
        INSERT INTO tracks (
            file_path, start_ms, title, artist, album, album_artist,
            duration_ms, format_badge, source, remote_id,
            remote_album_id, remote_artist_id, cover_url, added_at
        ) VALUES (
            ?1, 0, ?2, ?3, ?4, ?5,
            ?6, ?7, ?8, ?9,
            ?10, ?11, ?12, ?13
        )
        ON CONFLICT(file_path, start_ms) DO UPDATE SET
            title=CASE WHEN excluded.title != '' THEN excluded.title ELSE tracks.title END,
            artist=CASE WHEN excluded.artist != '' THEN excluded.artist ELSE tracks.artist END,
            album=CASE WHEN excluded.album != '' THEN excluded.album ELSE tracks.album END,
            album_artist=CASE WHEN excluded.album_artist != '' THEN excluded.album_artist ELSE tracks.album_artist END,
            duration_ms=CASE WHEN excluded.duration_ms > 0 THEN excluded.duration_ms ELSE tracks.duration_ms END,
            format_badge=CASE WHEN excluded.format_badge != '' THEN excluded.format_badge ELSE tracks.format_badge END,
            source=excluded.source,
            remote_id=excluded.remote_id,
            remote_album_id=CASE WHEN excluded.remote_album_id != '' THEN excluded.remote_album_id ELSE tracks.remote_album_id END,
            remote_artist_id=CASE WHEN excluded.remote_artist_id != '' THEN excluded.remote_artist_id ELSE tracks.remote_artist_id END,
            cover_url=CASE WHEN excluded.cover_url != '' THEN excluded.cover_url ELSE tracks.cover_url END
        RETURNING id;
    )SQL";

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, kOnlineUpsertSql, -1, &raw, nullptr) != SQLITE_OK) {
        return -1;
    }
    StmtGuard s(raw);
    const QString synPath = syntheticOnlineFilePath(src, rid);
    const int64_t now = QDateTime::currentSecsSinceEpoch();
    bindText(s.get(), 1, synPath);
    bindText(s.get(), 2, track.title);
    bindText(s.get(), 3, track.artist);
    bindText(s.get(), 4, track.album);
    bindText(s.get(), 5, track.albumArtist.isEmpty() ? track.artist : track.albumArtist);
    sqlite3_bind_int64(s.get(), 6, track.durationMs);
    bindText(s.get(), 7, track.formatBadge.isEmpty() ? QStringLiteral("Online") : track.formatBadge);
    bindText(s.get(), 8, src);
    bindText(s.get(), 9, rid);
    bindText(s.get(), 10, track.remoteAlbumId);
    bindText(s.get(), 11, track.remoteArtistId);
    bindText(s.get(), 12, track.coverUrl);
    sqlite3_bind_int64(s.get(), 13, now);

    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        return sqlite3_column_int64(s.get(), 0);
    }
    return -1;
}

int64_t LibraryDatabase::upsertOnlineTrack(const TrackInfo& track) {
    std::lock_guard<std::mutex> lock(mutex_);
    return upsertOnlineTrackLocked(track);
}

bool LibraryDatabase::findOnlineTrack(const QString& source, const QString& remoteId, TrackInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || remoteId.trimmed().isEmpty()) return false;
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = ?1 AND t.remote_id = ?2 LIMIT 1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    bindText(s.get(), 1, src);
    bindText(s.get(), 2, remoteId.trimmed());
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        out = readTrackRow(s.get());
        return true;
    }
    return false;
}

bool LibraryDatabase::findLocalTrackByRemoteId(const QString& remoteId, TrackInfo& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || remoteId.trimmed().isEmpty()) return false;
    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM tracks t WHERE t.source = 'local' AND t.remote_id = ?1 LIMIT 1;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    bindText(s.get(), 1, remoteId.trimmed());
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        out = readTrackRow(s.get());
        return true;
    }
    return false;
}

bool LibraryDatabase::isOnlineTrackFavorite(const QString& source, const QString& remoteId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || remoteId.trimmed().isEmpty()) return false;
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT is_favorite FROM tracks WHERE remote_id = ?1 AND (source = ?2 OR source = 'local') "
            "ORDER BY is_favorite DESC LIMIT 1;",
            -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, remoteId.trimmed());
    bindText(s.get(), 2, src);
    if (sqlite3_step(s.get()) == SQLITE_ROW) {
        return sqlite3_column_int(s.get(), 0) != 0;
    }
    return false;
}

bool LibraryDatabase::setOnlineTrackFavorite(const TrackInfo& track, bool favorite, int64_t* trackIdOut) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t id = upsertOnlineTrackLocked(track);
    if (id < 0) return false;
    if (trackIdOut) *trackIdOut = id;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE tracks SET is_favorite = ?1 WHERE id = ?2;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    sqlite3_bind_int(s.get(), 1, favorite ? 1 : 0);
    sqlite3_bind_int64(s.get(), 2, id);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::recordOnlinePlayback(const TrackInfo& track, int64_t* trackIdOut) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int64_t id = upsertOnlineTrackLocked(track);
    if (id < 0) return false;
    if (trackIdOut) *trackIdOut = id;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "UPDATE tracks SET play_count = play_count + 1, last_played_at = ?1 WHERE id = ?2;",
            -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, QDateTime::currentSecsSinceEpoch());
    sqlite3_bind_int64(s.get(), 2, id);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::upgradeOnlineTrackToLocal(const QString& source, const QString& remoteId, TrackInfo& localTrack) {
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    const QString rid = remoteId.trimmed();
    localTrack.source = QStringLiteral("local");
    localTrack.remoteId = rid;

    if (rid.isEmpty()) {
        return upsertTrack(localTrack);
    }

    int64_t onlineId = -1;
    bool mergedFav = localTrack.isFavorite;
    int mergedPlays = localTrack.playCount;
    int64_t mergedLastPlayed = localTrack.lastPlayedAt;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) return false;

        sqlite3_stmt* rawFind = nullptr;
        if (sqlite3_prepare_v2(
                db_,
                "SELECT id, is_favorite, play_count, last_played_at FROM tracks "
                "WHERE source = ?1 AND remote_id = ?2 LIMIT 1;",
                -1, &rawFind, nullptr) == SQLITE_OK) {
            StmtGuard sFind(rawFind);
            bindText(sFind.get(), 1, src);
            bindText(sFind.get(), 2, rid);
            if (sqlite3_step(sFind.get()) == SQLITE_ROW) {
                onlineId = sqlite3_column_int64(sFind.get(), 0);
                mergedFav = mergedFav || (sqlite3_column_int(sFind.get(), 1) != 0);
                mergedPlays += sqlite3_column_int(sFind.get(), 2);
                mergedLastPlayed = std::max<int64_t>(mergedLastPlayed, sqlite3_column_int64(sFind.get(), 3));
            }
        }

        if (onlineId >= 0) {
            if (!execSql("BEGIN IMMEDIATE;")) return false;

            // If another row already exists for (file_path, start_ms), merge stats/playlists & delete it first
            int64_t existingLocalId = -1;
            {
                sqlite3_stmt* rawDup = nullptr;
                if (sqlite3_prepare_v2(
                        db_,
                        "SELECT id, is_favorite, play_count, last_played_at FROM tracks "
                        "WHERE file_path = ?1 AND start_ms = ?2 AND id != ?3 LIMIT 1;",
                        -1, &rawDup, nullptr) == SQLITE_OK) {
                    StmtGuard sDup(rawDup);
                    bindText(sDup.get(), 1, localTrack.filePath);
                    sqlite3_bind_int64(sDup.get(), 2, localTrack.startMs);
                    sqlite3_bind_int64(sDup.get(), 3, onlineId);
                    if (sqlite3_step(sDup.get()) == SQLITE_ROW) {
                        existingLocalId = sqlite3_column_int64(sDup.get(), 0);
                        mergedFav = mergedFav || (sqlite3_column_int(sDup.get(), 1) != 0);
                        mergedPlays += sqlite3_column_int(sDup.get(), 2);
                        mergedLastPlayed = std::max<int64_t>(mergedLastPlayed, sqlite3_column_int64(sDup.get(), 3));
                    }
                }
            }

            if (existingLocalId >= 0) {
                sqlite3_stmt* rawCleanPl = nullptr;
                if (sqlite3_prepare_v2(
                        db_,
                        "DELETE FROM playlist_items WHERE track_id = ?1 "
                        "AND playlist_id IN (SELECT playlist_id FROM playlist_items WHERE track_id = ?2);",
                        -1, &rawCleanPl, nullptr) == SQLITE_OK) {
                    StmtGuard sClean(rawCleanPl);
                    sqlite3_bind_int64(sClean.get(), 1, existingLocalId);
                    sqlite3_bind_int64(sClean.get(), 2, onlineId);
                    sqlite3_step(sClean.get());
                }

                sqlite3_stmt* rawMovePl = nullptr;
                if (sqlite3_prepare_v2(
                        db_,
                        "UPDATE playlist_items SET track_id = ?1 WHERE track_id = ?2;",
                        -1, &rawMovePl, nullptr) == SQLITE_OK) {
                    StmtGuard sMove(rawMovePl);
                    sqlite3_bind_int64(sMove.get(), 1, onlineId);
                    sqlite3_bind_int64(sMove.get(), 2, existingLocalId);
                    sqlite3_step(sMove.get());
                }

                sqlite3_stmt* rawDelDup = nullptr;
                if (sqlite3_prepare_v2(db_, "DELETE FROM tracks WHERE id = ?1;", -1, &rawDelDup, nullptr) == SQLITE_OK) {
                    StmtGuard sDel(rawDelDup);
                    sqlite3_bind_int64(sDel.get(), 1, existingLocalId);
                    sqlite3_step(sDel.get());
                }
            }

            static constexpr const char* kUpgradeSql = R"SQL(
                UPDATE tracks SET
                    source='local',
                    file_path=?1,
                    is_cue=?2,
                    cue_path=?3,
                    start_ms=?4,
                    end_ms=?5,
                    title=?6,
                    artist=?7,
                    album=?8,
                    album_artist=?9,
                    track_number=?10,
                    disc_number=?11,
                    year=?12,
                    genre=?13,
                    duration_ms=?14,
                    sample_rate=?15,
                    bit_depth=?16,
                    bitrate_kbps=?17,
                    channels=?18,
                    codec=?19,
                    format_badge=?20,
                    is_dsd=?21,
                    rg_track_gain=?22,
                    rg_track_peak=?23,
                    rg_album_gain=?24,
                    rg_album_peak=?25,
                    has_rg_track=?26,
                    has_rg_album=?27,
                    cover_hash=CASE WHEN ?28 != '' THEN ?28 ELSE cover_hash END,
                    file_size=?29,
                    file_mtime=?30,
                    remote_id=?31,
                    is_favorite=?32,
                    play_count=?33,
                    last_played_at=?34
                WHERE id=?35;
            )SQL";

            sqlite3_stmt* rawUp = nullptr;
            if (sqlite3_prepare_v2(db_, kUpgradeSql, -1, &rawUp, nullptr) != SQLITE_OK) {
                execSql("ROLLBACK;");
                return false;
            }
            bool updated = false;
            {
                StmtGuard sUp(rawUp);
                bindText(sUp.get(), 1, localTrack.filePath);
                sqlite3_bind_int(sUp.get(), 2, localTrack.isCue ? 1 : 0);
                bindText(sUp.get(), 3, localTrack.cuePath);
                sqlite3_bind_int64(sUp.get(), 4, localTrack.startMs);
                sqlite3_bind_int64(sUp.get(), 5, localTrack.endMs);
                bindText(sUp.get(), 6, localTrack.title);
                bindText(sUp.get(), 7, localTrack.artist);
                bindText(sUp.get(), 8, localTrack.album);
                bindText(sUp.get(), 9, localTrack.albumArtist);
                sqlite3_bind_int(sUp.get(), 10, localTrack.trackNumber);
                sqlite3_bind_int(sUp.get(), 11, localTrack.discNumber);
                sqlite3_bind_int(sUp.get(), 12, localTrack.year);
                bindText(sUp.get(), 13, localTrack.genre);
                sqlite3_bind_int64(sUp.get(), 14, localTrack.durationMs);
                sqlite3_bind_int(sUp.get(), 15, localTrack.sampleRate);
                sqlite3_bind_int(sUp.get(), 16, localTrack.bitDepth);
                sqlite3_bind_int(sUp.get(), 17, localTrack.bitrateKbps);
                sqlite3_bind_int(sUp.get(), 18, localTrack.channels);
                bindText(sUp.get(), 19, localTrack.codec);
                bindText(sUp.get(), 20, localTrack.formatBadge);
                sqlite3_bind_int(sUp.get(), 21, localTrack.isDsd ? 1 : 0);
                sqlite3_bind_double(sUp.get(), 22, localTrack.rgTrackGainDb);
                sqlite3_bind_double(sUp.get(), 23, localTrack.rgTrackPeak);
                sqlite3_bind_double(sUp.get(), 24, localTrack.rgAlbumGainDb);
                sqlite3_bind_double(sUp.get(), 25, localTrack.rgAlbumPeak);
                sqlite3_bind_int(sUp.get(), 26, localTrack.hasRgTrack ? 1 : 0);
                sqlite3_bind_int(sUp.get(), 27, localTrack.hasRgAlbum ? 1 : 0);
                bindText(sUp.get(), 28, localTrack.coverHash);
                sqlite3_bind_int64(sUp.get(), 29, localTrack.fileSize);
                sqlite3_bind_int64(sUp.get(), 30, localTrack.fileMtime);
                bindText(sUp.get(), 31, rid);
                sqlite3_bind_int(sUp.get(), 32, mergedFav ? 1 : 0);
                sqlite3_bind_int(sUp.get(), 33, mergedPlays);
                sqlite3_bind_int64(sUp.get(), 34, mergedLastPlayed);
                sqlite3_bind_int64(sUp.get(), 35, onlineId);
                updated = (sqlite3_step(sUp.get()) == SQLITE_DONE);
            }
            if (!updated || !execSql("COMMIT;")) {
                execSql("ROLLBACK;");
                return false;
            }
            localTrack.id = onlineId;
            localTrack.isFavorite = mergedFav;
            localTrack.playCount = mergedPlays;
            localTrack.lastPlayedAt = mergedLastPlayed;
            return true;
        }
    }

    return upsertTrack(localTrack);
}

QVector<PlaylistSummary> LibraryDatabase::listPlaylists() const {
    QVector<PlaylistSummary> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    static constexpr const char* kSql =
        "SELECT p.id, p.name, p.created_at, COUNT(pi.track_id) "
        "FROM playlists p LEFT JOIN playlist_items pi ON p.id = pi.playlist_id "
        "GROUP BY p.id ORDER BY p.created_at ASC;";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, kSql, -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        PlaylistSummary p;
        p.id = sqlite3_column_int64(s.get(), 0);
        p.name = colText(s.get(), 1);
        p.createdAt = sqlite3_column_int64(s.get(), 2);
        p.trackCount = sqlite3_column_int(s.get(), 3);
        out.push_back(std::move(p));
    }
    return out;
}

int64_t LibraryDatabase::createPlaylist(const QString& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || name.trimmed().isEmpty()) return -1;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO playlists(name, created_at) VALUES(?1, ?2);", -1, &raw, nullptr) != SQLITE_OK) {
        return -1;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, name.trimmed());
    sqlite3_bind_int64(s.get(), 2, QDateTime::currentSecsSinceEpoch());
    if (sqlite3_step(s.get()) != SQLITE_DONE) return -1;
    return sqlite3_last_insert_rowid(db_);
}

bool LibraryDatabase::renamePlaylist(int64_t playlistId, const QString& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || newName.trimmed().isEmpty()) return false;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE playlists SET name = ?1 WHERE id = ?2;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, newName.trimmed());
    sqlite3_bind_int64(s.get(), 2, playlistId);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::deletePlaylist(int64_t playlistId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM playlists WHERE id = ?1;", -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, playlistId);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::addTrackToPlaylist(int64_t playlistId, int64_t trackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    sqlite3_stmt* raw = nullptr;
    static constexpr const char* kSql =
        "INSERT INTO playlist_items(playlist_id, track_id, position) "
        "VALUES (?1, ?2, COALESCE((SELECT MAX(position) + 1 FROM playlist_items WHERE playlist_id = ?1), 0));";
    if (sqlite3_prepare_v2(db_, kSql, -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, playlistId);
    sqlite3_bind_int64(s.get(), 2, trackId);
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::removePlaylistItem(int64_t playlistId, int position) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    if (!execSql("BEGIN IMMEDIATE;")) return false;

    sqlite3_stmt* rawDel = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM playlist_items WHERE playlist_id = ?1 AND position = ?2;", -1, &rawDel, nullptr) != SQLITE_OK) {
        execSql("ROLLBACK;");
        return false;
    }
    {
        StmtGuard s(rawDel);
        sqlite3_bind_int64(s.get(), 1, playlistId);
        sqlite3_bind_int(s.get(), 2, position);
        sqlite3_step(s.get());
    }

    // Re-compact positions
    sqlite3_stmt* rawShift = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE playlist_items SET position = position - 1 WHERE playlist_id = ?1 AND position > ?2;", -1, &rawShift, nullptr) == SQLITE_OK) {
        StmtGuard s(rawShift);
        sqlite3_bind_int64(s.get(), 1, playlistId);
        sqlite3_bind_int(s.get(), 2, position);
        sqlite3_step(s.get());
    }

    return execSql("COMMIT;");
}

QVector<TrackInfo> LibraryDatabase::queryPlaylistTracks(int64_t playlistId) const {
    QVector<TrackInfo> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    const QString sql = QString::fromLatin1(kSelectTrackColumns)
        + QStringLiteral("FROM playlist_items pi JOIN tracks t ON t.id = pi.track_id WHERE pi.playlist_id = ?1 ORDER BY pi.position ASC;");
    sqlite3_stmt* raw = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db_, utf8.constData(), -1, &raw, nullptr) != SQLITE_OK) return out;
    StmtGuard s(raw);
    sqlite3_bind_int64(s.get(), 1, playlistId);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        out.push_back(readTrackRow(s.get()));
    }
    return out;
}

bool LibraryDatabase::pinOnlinePlaylist(const PinnedOnlinePlaylist& playlist) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || playlist.remoteId.trimmed().isEmpty() || playlist.name.trimmed().isEmpty()) {
        return false;
    }
    const QString src = playlist.source.trimmed().isEmpty() ? QStringLiteral("netease") : playlist.source.trimmed();
    static constexpr const char* kSql = R"SQL(
        INSERT INTO pinned_online_playlists (source, remote_id, name, creator, cover_url, track_count, pinned_at)
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
        ON CONFLICT(source, remote_id) DO UPDATE SET
            name=excluded.name,
            creator=CASE WHEN excluded.creator != '' THEN excluded.creator ELSE pinned_online_playlists.creator END,
            cover_url=CASE WHEN excluded.cover_url != '' THEN excluded.cover_url ELSE pinned_online_playlists.cover_url END,
            track_count=CASE WHEN excluded.track_count > 0 THEN excluded.track_count ELSE pinned_online_playlists.track_count END;
    )SQL";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, kSql, -1, &raw, nullptr) != SQLITE_OK) return false;
    StmtGuard s(raw);
    bindText(s.get(), 1, src);
    bindText(s.get(), 2, playlist.remoteId.trimmed());
    bindText(s.get(), 3, playlist.name.trimmed());
    bindText(s.get(), 4, playlist.creator.trimmed());
    bindText(s.get(), 5, playlist.coverUrl.trimmed());
    sqlite3_bind_int(s.get(), 6, playlist.trackCount);
    sqlite3_bind_int64(s.get(), 7, QDateTime::currentSecsSinceEpoch());
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::unpinOnlinePlaylist(const QString& source, const QString& remoteId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || remoteId.trimmed().isEmpty()) return false;
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "DELETE FROM pinned_online_playlists WHERE source = ?1 AND remote_id = ?2;",
            -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, src);
    bindText(s.get(), 2, remoteId.trimmed());
    return sqlite3_step(s.get()) == SQLITE_DONE;
}

bool LibraryDatabase::isOnlinePlaylistPinned(const QString& source, const QString& remoteId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || remoteId.trimmed().isEmpty()) return false;
    const QString src = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT 1 FROM pinned_online_playlists WHERE source = ?1 AND remote_id = ?2 LIMIT 1;",
            -1, &raw, nullptr) != SQLITE_OK) {
        return false;
    }
    StmtGuard s(raw);
    bindText(s.get(), 1, src);
    bindText(s.get(), 2, remoteId.trimmed());
    return sqlite3_step(s.get()) == SQLITE_ROW;
}

QVector<PinnedOnlinePlaylist> LibraryDatabase::listPinnedOnlinePlaylists() const {
    QVector<PinnedOnlinePlaylist> out;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return out;

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT id, source, remote_id, name, creator, cover_url, track_count, pinned_at "
            "FROM pinned_online_playlists ORDER BY pinned_at DESC, id DESC;",
            -1, &raw, nullptr) != SQLITE_OK) {
        return out;
    }
    StmtGuard s(raw);
    while (sqlite3_step(s.get()) == SQLITE_ROW) {
        PinnedOnlinePlaylist p;
        p.id = sqlite3_column_int64(s.get(), 0);
        p.source = colText(s.get(), 1);
        p.remoteId = colText(s.get(), 2);
        p.name = colText(s.get(), 3);
        p.creator = colText(s.get(), 4);
        p.coverUrl = colText(s.get(), 5);
        p.trackCount = sqlite3_column_int(s.get(), 6);
        p.pinnedAt = sqlite3_column_int64(s.get(), 7);
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace virela
