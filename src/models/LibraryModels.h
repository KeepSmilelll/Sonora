#pragma once

#include "audio/AudioTypes.h"
#include "library/LibraryDatabase.h"

#include <QAbstractListModel>
#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace virela {

class AppSettings;
class CoverArtProvider;
class LibraryScanner;
class PlaybackController;

class TrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TrackIdRole = Qt::UserRole + 1,
        FilePathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        TrackNumberRole,
        DiscNumberRole,
        YearRole,
        DurationMsRole,
        FormatBadgeRole,
        TechSummaryRole,
        CoverHashRole,
        IsFavoriteRole,
        IsCueRole,
        SampleRateRole,
        BitDepthRole,
        CodecRole,
        SourceRole,
        RemoteIdRole,
        CoverUrlRole,
        IsOnlineRole,
        TrackKeyRole
    };

    explicit TrackListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(tracks_.size()); }
    const QVector<TrackInfo>& tracks() const { return tracks_; }
    void setTracks(QVector<TrackInfo> tracks);
    void updateFavoriteForTrack(int64_t trackId, bool favorite);
    void updateFavoriteForKey(const QString& trackKey, bool favorite);

    Q_INVOKABLE QVariantMap trackMapAt(int row) const;

signals:
    void countChanged();

private:
    QVector<TrackInfo> tracks_;
};

class AlbumListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        AlbumRole = Qt::UserRole + 1,
        AlbumArtistRole,
        YearRole,
        TrackCountRole,
        TotalDurationMsRole,
        CoverHashRole,
        FormatBadgeRole
    };

    explicit AlbumListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(albums_.size()); }
    void setAlbums(QVector<AlbumSummary> albums);

signals:
    void countChanged();

private:
    QVector<AlbumSummary> albums_;
};

class ArtistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        ArtistRole = Qt::UserRole + 1,
        AlbumCountRole,
        TrackCountRole,
        CoverHashRole
    };

    explicit ArtistListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(artists_.size()); }
    void setArtists(QVector<ArtistSummary> artists);

signals:
    void countChanged();

private:
    QVector<ArtistSummary> artists_;
};

class LibraryController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QObject* tracksModel READ tracksModel CONSTANT)
    Q_PROPERTY(QObject* albumsModel READ albumsModel CONSTANT)
    Q_PROPERTY(QObject* artistsModel READ artistsModel CONSTANT)
    Q_PROPERTY(QObject* detailTracksModel READ detailTracksModel CONSTANT)
    Q_PROPERTY(QObject* downloadedTracksModel READ downloadedTracksModel CONSTANT)

    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QVariantList pinnedPlaylists READ pinnedPlaylists NOTIFY pinnedPlaylistsChanged)
    Q_PROPERTY(QVariantList folderEntries READ folderEntries NOTIFY folderChanged)
    Q_PROPERTY(QString currentFolderPath READ currentFolderPath NOTIFY folderChanged)
    Q_PROPERTY(bool canGoUpFolder READ canGoUpFolder NOTIFY folderChanged)

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY filterChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY filterChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY filterChanged)

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanningChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanningChanged)
    Q_PROPERTY(int totalTrackCount READ totalTrackCount NOTIFY libraryUpdated)
    Q_PROPERTY(QVariantList recentTracksSummary READ recentTracksSummary NOTIFY libraryUpdated)
    Q_PROPERTY(QVariantList recentAlbumsSummary READ recentAlbumsSummary NOTIFY libraryUpdated)
    Q_PROPERTY(bool hasRecentPlays READ hasRecentPlays NOTIFY libraryUpdated)

public:
    LibraryController(LibraryDatabase* db,
                      LibraryScanner* scanner,
                      CoverArtProvider* covers,
                      AppSettings* settings,
                      PlaybackController* player,
                      QObject* parent = nullptr);

    QObject* tracksModel();
    QObject* albumsModel();
    QObject* artistsModel();
    QObject* detailTracksModel() { return &detailTracksModel_; }
    QObject* downloadedTracksModel();

    QVariantList playlists() const { return playlists_; }
    QVariantList pinnedPlaylists() const { return pinnedPlaylists_; }
    QVariantList folderEntries() const { return folderEntries_; }
    QString currentFolderPath() const { return currentFolderPath_; }
    bool canGoUpFolder() const { return !currentFolderPath_.isEmpty(); }

    QString searchQuery() const { return searchQuery_; }
    void setSearchQuery(const QString& q);
    QString sortBy() const { return sortBy_; }
    void setSortBy(const QString& sort);
    bool sortAscending() const { return sortAscending_; }
    void setSortAscending(bool asc);

    bool isScanning() const;
    int scanProgress() const;
    int scanTotal() const;
    int totalTrackCount() const { return totalTrackCount_; }
    QVariantList recentTracksSummary() const { return recentTracksSummary_; }
    QVariantList recentAlbumsSummary() const { return recentAlbumsSummary_; }
    bool hasRecentPlays() const { return hasRecentPlays_; }

    // View loaders
    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void loadSection(const QString& section);
    Q_INVOKABLE void openAlbum(const QString& album, const QString& albumArtist);
    Q_INVOKABLE void openArtist(const QString& artist);
    Q_INVOKABLE void openFolder(const QString& folderPath);
    Q_INVOKABLE void goUpFolder();
    Q_INVOKABLE void openPlaylist(qint64 playlistId);

    // Playback actions on current model
    Q_INVOKABLE void playFromTracksModel(int index);
    Q_INVOKABLE void playFromDetailModel(int index);
    Q_INVOKABLE void enqueueFromTracksModel(int index);
    Q_INVOKABLE void playNextFromTracksModel(int index);
    Q_INVOKABLE void playFromDownloadedModel(int index);
    Q_INVOKABLE void playNextFromDownloadedModel(int index);
    Q_INVOKABLE void enqueueFromDownloadedModel(int index);
    Q_INVOKABLE QVariantMap quickSearchLocal(const QString& query,
                                             int maxTracks = 5,
                                             int maxAlbums = 3,
                                             int maxArtists = 2) const;
    Q_INVOKABLE void playTrackById(qint64 trackId);

    // Favorites & Playlists & M3U8 (unified across local and online tracks)
    Q_INVOKABLE void toggleFavorite(qint64 trackId, bool currentFav);
    Q_INVOKABLE bool toggleTrackFavorite(const QVariantMap& trackMap);
    Q_INVOKABLE bool isTrackFavorite(const QString& source, const QString& remoteId) const;
    Q_INVOKABLE qint64 createPlaylist(const QString& name);
    Q_INVOKABLE void deletePlaylist(qint64 playlistId);
    Q_INVOKABLE void addTrackToPlaylist(qint64 playlistId, qint64 trackId);
    Q_INVOKABLE void addTrackMapToPlaylist(qint64 playlistId, const QVariantMap& trackMap);
    Q_INVOKABLE void removeTrackFromPlaylist(qint64 playlistId, int position);
    Q_INVOKABLE void importM3uPlaylist(const QUrl& fileUrl);
    Q_INVOKABLE void exportM3uPlaylist(qint64 playlistId, const QUrl& dstFileUrl);

    // Pinned online playlists
    Q_INVOKABLE bool pinOnlinePlaylist(const QString& source,
                                       const QString& remoteId,
                                       const QString& name,
                                       const QString& creator = {},
                                       const QString& coverUrl = {},
                                       int trackCount = 0);
    Q_INVOKABLE bool unpinOnlinePlaylist(const QString& source, const QString& remoteId);
    Q_INVOKABLE bool isOnlinePlaylistPinned(const QString& source, const QString& remoteId) const;

    // Folder & Scan management & Memory release
    Q_INVOKABLE void addWatchedFolder(const QUrl& folderUrl);
    Q_INVOKABLE void removeWatchedFolder(const QString& folderPath);
    Q_INVOKABLE void rescanLibrary();
    Q_INVOKABLE bool ingestLocalFile(const QString& audioFilePath);
    Q_INVOKABLE bool ingestDownloadedTrack(const QString& audioFilePath,
                                           const QString& onlineSource,
                                           const QString& onlineRemoteId);
    Q_INVOKABLE void releaseInactiveModels(const QString& activeSection);

    bool findUpgradedLocalTrack(const QString& remoteId, TrackInfo& out) const;

    // Rich drag-and-drop URL expander (supports audio files, folders, .cue, .m3u/.m3u8)
    QVector<TrackInfo> expandDroppedUrls(const QList<QUrl>& urls);

signals:
    void playlistsChanged();
    void pinnedPlaylistsChanged();
    void favoritesChanged();
    void folderChanged();
    void filterChanged();
    void scanningChanged();
    void libraryUpdated();

private:
    void ensureTracksLoaded();
    void ensureAlbumsLoaded();
    void ensureArtistsLoaded();
    void ensureDownloadsLoaded();
    void reloadTracksForCurrentSection();
    void reloadPlaylists();
    void reloadPinnedPlaylists();
    void reloadFolderBrowser();
    void reloadDownloadedTracks();
    void reloadHomeSummaries();

    LibraryDatabase* db_ = nullptr;
    LibraryScanner* scanner_ = nullptr;
    CoverArtProvider* covers_ = nullptr;
    AppSettings* settings_ = nullptr;
    PlaybackController* player_ = nullptr;

    TrackListModel tracksModel_;
    AlbumListModel albumsModel_;
    ArtistListModel artistsModel_;
    TrackListModel detailTracksModel_;
    TrackListModel downloadedTracksModel_;

    QVariantList playlists_;
    QVariantList pinnedPlaylists_;
    QVariantList folderEntries_;
    QVariantList recentTracksSummary_;
    QVariantList recentAlbumsSummary_;
    bool hasRecentPlays_ = false;
    QString currentFolderPath_;
    QString currentSection_ = QStringLiteral("tracks");
    qint64 activePlaylistId_ = -1;

    bool tracksLoaded_ = false;
    bool albumsLoaded_ = false;
    bool artistsLoaded_ = false;
    bool downloadsLoaded_ = false;
    bool tracksDirty_ = true;
    bool albumsDirty_ = true;
    bool artistsDirty_ = true;
    bool downloadsDirty_ = true;

    QString searchQuery_;
    QString sortBy_ = QStringLiteral("artist");
    bool sortAscending_ = true;
    int totalTrackCount_ = 0;
};

} // namespace virela
