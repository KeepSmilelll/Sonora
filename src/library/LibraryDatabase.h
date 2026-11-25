#pragma once

#include "audio/AudioTypes.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <cstdint>
#include <mutex>
#include <utility>

struct sqlite3;

namespace sonora {

struct AlbumSummary {
    QString album;
    QString albumArtist;
    int year = 0;
    int trackCount = 0;
    int64_t totalDurationMs = 0;
    QString coverHash;
    QString formatBadge;
};

struct ArtistSummary {
    QString artist;
    int albumCount = 0;
    int trackCount = 0;
    QString coverHash;
};

struct PlaylistSummary {
    int64_t id = -1;
    QString name;
    int trackCount = 0;
    int64_t createdAt = 0;
};

class LibraryDatabase {
public:
    LibraryDatabase() = default;
    ~LibraryDatabase();

    LibraryDatabase(const LibraryDatabase&) = delete;
    LibraryDatabase& operator=(const LibraryDatabase&) = delete;

    bool open(const QString& dbPath);
    void close();
    bool isOpen() const noexcept { return db_ != nullptr; }

    // Batch upsert inside a single SQLite transaction for fast library scans
    bool upsertTracksBatch(QVector<TrackInfo>& tracks);
    bool upsertTrack(TrackInfo& track);
    bool removeTracksForBackingFile(const QString& filePath);
    bool removeTracksOutsideFolders(const QStringList& watchedFolders);

    // Incremental scan index: file_path -> {file_mtime, file_size}
    QHash<QString, std::pair<int64_t, int64_t>> loadFileFingerprintIndex() const;
    int totalTrackCount() const;

    // Track queries (uses FTS5 trigram for >= 3 chars, indexed LIKE for 1-2 char CJK queries)
    QVector<TrackInfo> queryTracks(const QString& searchQuery = {},
                                   const QString& sortBy = QStringLiteral("artist"),
                                   bool ascending = true) const;
    QVector<TrackInfo> queryAlbumTracks(const QString& album, const QString& albumArtist = {}) const;
    QVector<TrackInfo> queryArtistTracks(const QString& artist) const;
    QVector<TrackInfo> queryFolderTracks(const QString& folderPrefix) const;
    QVector<TrackInfo> queryFavoriteTracks() const;
    QVector<TrackInfo> queryRecentlyPlayed(int limit = 100) const;
    QVector<TrackInfo> queryRecentlyAdded(int limit = 100) const;
    bool findTrackByPath(const QString& filePath, TrackInfo& out) const;

    QVector<AlbumSummary> queryAlbums(const QString& searchQuery = {}) const;
    QVector<ArtistSummary> queryArtists(const QString& searchQuery = {}) const;

    bool setFavorite(int64_t trackId, bool favorite);
    bool recordPlayback(int64_t trackId);

    // Playlists
    QVector<PlaylistSummary> listPlaylists() const;
    int64_t createPlaylist(const QString& name);
    bool renamePlaylist(int64_t playlistId, const QString& newName);
    bool deletePlaylist(int64_t playlistId);
    bool addTrackToPlaylist(int64_t playlistId, int64_t trackId);
    bool removePlaylistItem(int64_t playlistId, int position);
    QVector<TrackInfo> queryPlaylistTracks(int64_t playlistId) const;

private:
    bool execSql(const char* sql) const;
    bool initSchema();

    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
};

} // namespace sonora
