#pragma once

#include "audio/AudioTypes.h"
#include "online/OnlineMusicService.h"

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

namespace virela {

class AppSettings;
class CoverArtProvider;
class LibraryController;
class LocalBackendManager;
class LyricsController;
class PlaybackController;
class StreamCacheManager;

class OnlineTrackListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        AlbumRole,
        DurationMsRole,
        FormatBadgeRole,
        CoverUrlRole,
        SourceRole,
        RemoteIdRole,
        RemoteAlbumIdRole,
        RemoteArtistIdRole,
        TrackKeyRole,
        DownloadStatusRole
    };

    explicit OnlineTrackListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(tracks_.size()); }
    const QVector<TrackInfo>& tracks() const { return tracks_; }
    void setTracks(QVector<TrackInfo> tracks);
    void setDownloadStatus(const QString& trackKey, const QString& status);
    QString downloadStatus(const QString& trackKey) const;

    Q_INVOKABLE QVariantMap trackMapAt(int row) const;

signals:
    void countChanged();

private:
    QVector<TrackInfo> tracks_;
    QHash<QString, QString> statusMap_;
};

class OnlineAlbumListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        AlbumIdRole = Qt::UserRole + 1,
        NameRole,
        ArtistRole,
        ArtistIdRole,
        CoverUrlRole,
        YearRole,
        TrackCountRole
    };

    explicit OnlineAlbumListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(albums_.size()); }
    void setAlbums(QVector<OnlineAlbumItem> albums);

signals:
    void countChanged();

private:
    QVector<OnlineAlbumItem> albums_;
};

class OnlineArtistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        ArtistIdRole = Qt::UserRole + 1,
        NameRole,
        CoverUrlRole,
        AlbumCountRole,
        TrackCountRole,
        BriefDescRole
    };

    explicit OnlineArtistListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(artists_.size()); }
    void setArtists(QVector<OnlineArtistItem> artists);

signals:
    void countChanged();

private:
    QVector<OnlineArtistItem> artists_;
};

class OnlinePlaylistListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        PlaylistIdRole = Qt::UserRole + 1,
        NameRole,
        CreatorRole,
        CoverUrlRole,
        TrackCountRole,
        PlayCountRole,
        DescriptionRole
    };

    explicit OnlinePlaylistListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(playlists_.size()); }
    void setPlaylists(QVector<OnlinePlaylistItem> playlists);

signals:
    void countChanged();

private:
    QVector<OnlinePlaylistItem> playlists_;
};

class OnlineMusicController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QObject* songsModel READ songsModel CONSTANT)
    Q_PROPERTY(QObject* albumsModel READ albumsModel CONSTANT)
    Q_PROPERTY(QObject* artistsModel READ artistsModel CONSTANT)
    Q_PROPERTY(QObject* playlistsModel READ playlistsModel CONSTANT)
    Q_PROPERTY(QObject* detailTracksModel READ detailTracksModel CONSTANT)
    Q_PROPERTY(QObject* recommendedPlaylistsModel READ recommendedPlaylistsModel CONSTANT)
    Q_PROPERTY(QObject* newSongsModel READ newSongsModel CONSTANT)

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchType READ searchType WRITE setSearchType NOTIFY searchStateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(bool backendConfigured READ isBackendConfigured NOTIFY backendConfigChanged)
    Q_PROPERTY(bool neteaseConfigured READ isNeteaseConfigured NOTIFY backendConfigChanged)

    Q_PROPERTY(QVariantList hotSearches READ hotSearches NOTIFY discoverChanged)
    Q_PROPERTY(bool discoverLoading READ discoverLoading NOTIFY discoverChanged)
    Q_PROPERTY(bool discoverLoaded READ discoverLoaded NOTIFY discoverChanged)
    Q_PROPERTY(QString discoverError READ discoverError NOTIFY discoverChanged)

    Q_PROPERTY(bool inDetail READ inDetail NOTIFY detailChanged)
    Q_PROPERTY(QString detailType READ detailType NOTIFY detailChanged)
    Q_PROPERTY(QString detailRemoteId READ detailRemoteId NOTIFY detailChanged)
    Q_PROPERTY(QString detailTitle READ detailTitle NOTIFY detailChanged)
    Q_PROPERTY(QString detailSubtitle READ detailSubtitle NOTIFY detailChanged)
    Q_PROPERTY(QString detailCoverUrl READ detailCoverUrl NOTIFY detailChanged)
    Q_PROPERTY(QString detailDescription READ detailDescription NOTIFY detailChanged)
    Q_PROPERTY(int detailTrackCount READ detailTrackCount NOTIFY detailChanged)

    Q_PROPERTY(bool testingConnection READ isTestingConnection NOTIFY connectionTestChanged)
    Q_PROPERTY(QString neteaseTestStatus READ neteaseTestStatus NOTIFY connectionTestChanged)
    Q_PROPERTY(bool neteaseTestOk READ neteaseTestOk NOTIFY connectionTestChanged)
    Q_PROPERTY(QString metingTestStatus READ metingTestStatus NOTIFY connectionTestChanged)
    Q_PROPERTY(bool metingTestOk READ metingTestOk NOTIFY connectionTestChanged)

    Q_PROPERTY(QString cacheUsageText READ cacheUsageText NOTIFY cacheStatsChanged)
    Q_PROPERTY(int activeDownloadCount READ activeDownloadCount NOTIFY downloadsChanged)
    Q_PROPERTY(QString currentTrackDownloadStatus READ currentTrackDownloadStatus NOTIFY downloadsChanged)
    Q_PROPERTY(QVariantList downloadTasks READ downloadTasks NOTIFY downloadsChanged)

    Q_PROPERTY(bool neteaseServiceRunning READ neteaseServiceRunning NOTIFY localServicesChanged)
    Q_PROPERTY(bool metingServiceRunning READ metingServiceRunning NOTIFY localServicesChanged)
    Q_PROPERTY(QString localServicesStatus READ localServicesStatus NOTIFY localServicesChanged)

public:
    OnlineMusicController(OnlineMusicService* service,
                          StreamCacheManager* cacheManager,
                          AppSettings* settings,
                          PlaybackController* player,
                          LibraryController* library,
                          LyricsController* lyrics,
                          CoverArtProvider* covers,
                          QObject* parent = nullptr);

    QObject* songsModel() { return &songsModel_; }
    QObject* albumsModel() { return &albumsModel_; }
    QObject* artistsModel() { return &artistsModel_; }
    QObject* playlistsModel() { return &playlistsModel_; }
    QObject* detailTracksModel() { return &detailTracksModel_; }
    QObject* recommendedPlaylistsModel() { return &recommendedPlaylistsModel_; }
    QObject* newSongsModel() { return &newSongsModel_; }

    QString searchQuery() const { return searchQuery_; }
    void setSearchQuery(const QString& q);
    QString searchType() const { return searchType_; }
    void setSearchType(const QString& t);
    bool isBusy() const { return busy_; }
    QString errorMessage() const { return errorMessage_; }
    bool isBackendConfigured() const;
    bool isNeteaseConfigured() const;

    QVariantList hotSearches() const { return hotSearches_; }
    bool discoverLoading() const { return discoverLoading_; }
    bool discoverLoaded() const { return discoverLoaded_; }
    QString discoverError() const { return discoverError_; }

    bool inDetail() const { return inDetail_; }
    QString detailType() const { return detailType_; }
    QString detailRemoteId() const { return detailRemoteId_; }
    QString detailTitle() const { return detailTitle_; }
    QString detailSubtitle() const { return detailSubtitle_; }
    QString detailCoverUrl() const;
    QString detailDescription() const { return detailDescription_; }
    int detailTrackCount() const { return detailTracksModel_.count(); }

    bool isTestingConnection() const { return testingConnection_; }
    QString neteaseTestStatus() const { return neteaseTestStatus_; }
    bool neteaseTestOk() const { return neteaseTestOk_; }
    QString metingTestStatus() const { return metingTestStatus_; }
    bool metingTestOk() const { return metingTestOk_; }

    QString cacheUsageText() const { return cacheUsageText_; }
    int activeDownloadCount() const { return activeDownloadCount_; }
    QString currentTrackDownloadStatus() const;
    QVariantList downloadTasks() const;

    void setLocalBackendManager(LocalBackendManager* manager);
    bool neteaseServiceRunning() const;
    bool metingServiceRunning() const;
    QString localServicesStatus() const;

    // Discovery home (lazy-loaded on OnlineMusicView activation)
    Q_INVOKABLE void ensureDiscoverLoaded();
    Q_INVOKABLE void refreshDiscover();

    // Search & Playlist URL/ID parsing
    Q_INVOKABLE void search(const QString& query, const QString& type = {});
    Q_INVOKABLE void parseAndOpenPlaylist(const QString& playlistIdOrUrl);
    Q_INVOKABLE QString detectPlaylistLinkId(const QString& input) const;

    // Drill-down views
    Q_INVOKABLE void openAlbum(const QString& albumId,
                               const QString& title = {},
                               const QString& artist = {},
                               const QString& coverUrl = {});
    Q_INVOKABLE void openArtist(const QString& artistId,
                                const QString& name = {},
                                const QString& coverUrl = {});
    Q_INVOKABLE void openPlaylist(const QString& playlistId,
                                  const QString& title = {},
                                  const QString& creator = {},
                                  const QString& coverUrl = {},
                                  const QString& description = {});
    Q_INVOKABLE void closeDetail();

    // Playback queue actions
    Q_INVOKABLE void playSongAt(int index);
    Q_INVOKABLE void playNextSongAt(int index);
    Q_INVOKABLE void enqueueSongAt(int index);

    Q_INVOKABLE void playDetailAt(int index);
    Q_INVOKABLE void playAllDetail();
    Q_INVOKABLE void playNextDetailAt(int index);
    Q_INVOKABLE void enqueueDetailAt(int index);
    Q_INVOKABLE void enqueueAllDetail();

    Q_INVOKABLE void playNewSongAt(int index);
    Q_INVOKABLE void playNextNewSongAt(int index);
    Q_INVOKABLE void enqueueNewSongAt(int index);

    // Downloads (zero-traffic reuse from StreamCacheManager -> metadata/cover/sidecar -> Library ingest)
    Q_INVOKABLE void downloadSongAt(int index);
    Q_INVOKABLE void downloadDetailAt(int index);
    Q_INVOKABLE void downloadNewSongAt(int index);
    Q_INVOKABLE void downloadAllDetail();
    Q_INVOKABLE void downloadCurrentTrack();
    Q_INVOKABLE void retryDownload(const QString& trackKey);
    Q_INVOKABLE void clearFinishedDownloads();
    Q_INVOKABLE void playDownloadTask(const QString& trackKey);

    // Settings diagnostics & cache maintenance
    Q_INVOKABLE void testConnections();
    Q_INVOKABLE void refreshCacheStats();
    Q_INVOKABLE void clearStreamCache();
    Q_INVOKABLE void setDownloadDirFromUrl(const QUrl& folderUrl);
    Q_INVOKABLE void restartLocalServices();
    Q_INVOKABLE void setNeteaseServiceDirFromUrl(const QUrl& folderUrl);
    Q_INVOKABLE void setMetingServiceDirFromUrl(const QUrl& folderUrl);
    Q_INVOKABLE QString sizedCoverUrl(const QString& rawUrl, int sizePx) const;
    Q_INVOKABLE void trimMemory();

    // Connected to PlaybackEngine::currentTrackChanged for online lyrics + cover loading
    void onCurrentTrackChanged(const TrackInfo& track);

signals:
    void searchStateChanged();
    void busyChanged();
    void errorChanged();
    void backendConfigChanged();
    void detailChanged();
    void connectionTestChanged();
    void cacheStatsChanged();
    void downloadsChanged();
    void localServicesChanged();
    void discoverChanged();

private:
    struct DownloadJob;
    struct DownloadTaskRecord {
        TrackInfo track;
        QString trackKey;
        QString status; // "queued" | "downloading" | "done" | "error"
        QString error;
        QString savedFilePath;
    };

    void setBusy(bool b);
    void setError(const QString& err);
    void enqueueDownloadTrack(const TrackInfo& track);
    void pumpDownloadQueue();
    void startDownloadJob(const TrackInfo& track);
    void tryFinalizeDownloadJob(const std::shared_ptr<DownloadJob>& job);
    void setTrackDownloadStatus(const QString& trackKey, const QString& status);
    TrackInfo resolvePlaybackTrack(const TrackInfo& track) const;
    QVector<TrackInfo> resolvePlaybackTracks(const QVector<TrackInfo>& tracks) const;
    void syncModelDownloadStatuses(OnlineTrackListModel& model, const QVector<TrackInfo>& tracks);
    void upsertDownloadTask(const TrackInfo& track,
                            const QString& status,
                            const QString& error = {},
                            const QString& savedPath = {});

    OnlineMusicService* service_ = nullptr;
    StreamCacheManager* cache_ = nullptr;
    AppSettings* settings_ = nullptr;
    PlaybackController* player_ = nullptr;
    LibraryController* library_ = nullptr;
    LyricsController* lyrics_ = nullptr;
    CoverArtProvider* covers_ = nullptr;
    LocalBackendManager* localBackends_ = nullptr;

    OnlineTrackListModel songsModel_;
    OnlineAlbumListModel albumsModel_;
    OnlineArtistListModel artistsModel_;
    OnlinePlaylistListModel playlistsModel_;
    OnlineTrackListModel detailTracksModel_;
    OnlinePlaylistListModel recommendedPlaylistsModel_;
    OnlineTrackListModel newSongsModel_;

    QString searchQuery_;
    QString searchType_ = QStringLiteral("songs");
    bool busy_ = false;
    QString errorMessage_;
    quint64 searchSeq_ = 0;
    quint64 detailSeq_ = 0;
    quint64 discoverSeq_ = 0;

    QVariantList hotSearches_;
    bool discoverLoading_ = false;
    bool discoverLoaded_ = false;
    QString discoverError_;
    QString lastDiscoverNeteaseUrl_;

    bool inDetail_ = false;
    QString detailType_;
    QString detailRemoteId_;
    QString detailTitle_;
    QString detailSubtitle_;
    QString detailCoverUrl_;
    QString detailDescription_;

    bool testingConnection_ = false;
    QString neteaseTestStatus_;
    bool neteaseTestOk_ = false;
    QString metingTestStatus_;
    bool metingTestOk_ = false;
    QString cacheUsageText_;

    QString activeTrackKey_;
    QHash<QString, QString> urlToCoverHash_;

    QVector<TrackInfo> downloadQueue_;
    QSet<QString> pendingDownloadKeys_;
    QHash<QString, std::shared_ptr<DownloadJob>> activeDownloadsByCacheKey_;
    QHash<QString, QString> trackDownloadStatus_;
    QVector<DownloadTaskRecord> downloadTasks_;
    int activeDownloadCount_ = 0;
    QThreadPool downloadPool_;
};

} // namespace virela
