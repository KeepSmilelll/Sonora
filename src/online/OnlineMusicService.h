#pragma once

#include "audio/AudioTypes.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <atomic>
#include <functional>
#include <mutex>

class QNetworkAccessManager;

namespace virela {

class AppSettings;
class StreamCacheManager;

struct OnlineAlbumItem {
    QString id;
    QString name;
    QString artist;
    QString artistId;
    QString coverUrl;
    int year = 0;
    int trackCount = 0;
    QString description;
};

struct OnlineArtistItem {
    QString id;
    QString name;
    QString coverUrl;
    int albumCount = 0;
    int trackCount = 0;
    QString briefDesc;
};

struct OnlinePlaylistItem {
    QString id;
    QString name;
    QString creator;
    QString coverUrl;
    int trackCount = 0;
    qint64 playCount = 0;
    QString description;
};

struct OnlineHotSearchItem {
    QString searchWord;
    QString content;
    QString iconUrl;
    int score = 0;
    int iconType = 0;
};

struct OnlinePlaylistDetail {
    bool ok = false;
    OnlinePlaylistItem info;
    QVector<TrackInfo> tracks;
    QString error;
};

struct OnlineAlbumDetail {
    bool ok = false;
    OnlineAlbumItem info;
    QVector<TrackInfo> tracks;
    QString error;
};

struct OnlineArtistDetail {
    bool ok = false;
    OnlineArtistItem info;
    QVector<TrackInfo> tracks;
    QString error;
};

struct OnlineLyricsResult {
    bool ok = false;
    QString yrc;
    QString lrc;
    QString tlyric;
    QString sourceLabel;
    QString error;
};

struct ResolvedStreamInfo {
    bool ok = false;
    QString streamUrl;
    QString backendUsed;     // "netease" | "meting"
    QString qualityResolved; // "standard" | "exhigh" | "lossless" | "hires"
    QString codecHint;
    QString formatBadge;
    int bitrateKbps = 0;
    bool isTrial = false;
    QString error;
};

struct BackendTestResult {
    bool configured = false;
    bool ok = false;
    int statusCode = 0;
    int latencyMs = -1;
    QString message;
};

class OnlineMusicService : public QObject {
    Q_OBJECT

public:
    OnlineMusicService(AppSettings* settings,
                       StreamCacheManager* cacheManager,
                       QObject* parent = nullptr);

    // Pure parsing & URL helper utilities (unit-tested)
    static QString normalizeBaseUrl(const QString& rawUrl);
    static QString sizedCoverUrl(const QString& rawUrl, int sizePx);
    static QString extractPlaylistId(const QString& input);
    static QString qualityToNeteaseLevel(const QString& quality);
    static int qualityToBitrate(const QString& quality);
    static QString formatBadgeForQuality(const QString& qualityOrLevel, const QString& codecHint = {});

    static QVector<TrackInfo> parseNeteaseSongsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<OnlineAlbumItem> parseNeteaseAlbumsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<OnlineArtistItem> parseNeteaseArtistsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<OnlinePlaylistItem> parseNeteasePlaylistsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<OnlineHotSearchItem> parseNeteaseHotSearchJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<OnlinePlaylistItem> parseNeteaseRecommendedPlaylistsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static QVector<TrackInfo> parseNeteaseNewSongsJson(const QByteArray& jsonBytes, QString* errorOut = nullptr);
    static OnlinePlaylistDetail parseNeteasePlaylistDetailJson(const QByteArray& jsonBytes);
    static OnlineAlbumDetail parseNeteaseAlbumDetailJson(const QByteArray& jsonBytes);
    static OnlineArtistDetail parseNeteaseArtistDetailJson(const QByteArray& jsonBytes);
    static OnlineLyricsResult parseNeteaseLyricsJson(const QByteArray& jsonBytes);
    static QVector<TrackInfo> parseMetingTracksJson(const QByteArray& jsonBytes, const QString& fallbackAlbum = {});

    // Connection diagnostics
    void testConnections(std::function<void(BackendTestResult netease, BackendTestResult meting)> callback);

    // Discovery (read-only public charts / personalized recommendations)
    void fetchHotSearch(int limit,
                        std::function<void(bool ok, QVector<OnlineHotSearchItem> items, QString error)> callback);
    void fetchRecommendedPlaylists(int limit,
                                   std::function<void(bool ok, QVector<OnlinePlaylistItem> playlists, QString error)> callback);
    void fetchNewSongs(int limit,
                       std::function<void(bool ok, QVector<TrackInfo> tracks, QString error)> callback);

    // Aggregate search
    void searchSongs(const QString& query, int limit, int offset,
                     std::function<void(bool ok, QVector<TrackInfo> tracks, QString error)> callback);
    void searchAlbums(const QString& query, int limit, int offset,
                      std::function<void(bool ok, QVector<OnlineAlbumItem> albums, QString error)> callback);
    void searchArtists(const QString& query, int limit, int offset,
                       std::function<void(bool ok, QVector<OnlineArtistItem> artists, QString error)> callback);
    void searchPlaylists(const QString& query, int limit, int offset,
                         std::function<void(bool ok, QVector<OnlinePlaylistItem> playlists, QString error)> callback);

    // Drill-down detail APIs
    void fetchPlaylistDetail(const QString& playlistIdOrUrl,
                             std::function<void(OnlinePlaylistDetail detail)> callback);
    void fetchAlbumDetail(const QString& albumId,
                          std::function<void(OnlineAlbumDetail detail)> callback);
    void fetchArtistDetail(const QString& artistId,
                           std::function<void(OnlineArtistDetail detail)> callback);

    // Lyrics (/lyric/new yrc + tlyric) & cover image bytes
    void fetchLyrics(const QString& remoteId,
                     std::function<void(OnlineLyricsResult result)> callback);
    void fetchCoverBytes(const QUrl& coverUrl,
                         std::function<void(bool ok, QByteArray data)> callback);

    // Thread-safe blocking direct-URL resolver called by PlaybackEngine decode thread & downloader
    ResolvedStreamInfo resolveStreamBlocking(const TrackInfo& track,
                                             const QString& qualityOverride = {},
                                             const std::atomic<bool>* abortFlag = nullptr,
                                             int timeoutMs = 12000);

    QString currentQuality() const;
    QString currentCookie() const;

private:
    struct ConfigSnapshot {
        QString neteaseUrl;
        QString metingUrl;
        QString priority = QStringLiteral("netease");
        bool autoFallback = true;
        bool unblockTrialFallback = true;
        QString quality = QStringLiteral("lossless");
        QString cookie;
    };

    void syncConfigSnapshot();
    ConfigSnapshot configSnapshot() const;
    QNetworkAccessManager* ensureNam();

    void asyncGet(const QUrl& url,
                  const QString& cookie,
                  int timeoutMs,
                  bool followRedirects,
                  std::function<void(bool ok, int status, QByteArray body, QUrl redirectUrl, QString err)> callback);

    static QUrl buildNeteaseUrl(const QString& base,
                                const QString& path,
                                const QVector<std::pair<QString, QString>>& queryItems = {});
    static QUrl buildMetingUrl(const QString& base,
                               const QString& server,
                               const QString& type,
                               const QString& id,
                               int br = 0);

    ResolvedStreamInfo resolveViaNeteaseBlocking(const ConfigSnapshot& cfg,
                                                 const QString& remoteId,
                                                 const QString& quality,
                                                 const std::atomic<bool>* abortFlag,
                                                 int timeoutMs);
    ResolvedStreamInfo resolveViaNeteaseMatchBlocking(const ConfigSnapshot& cfg,
                                                      const QString& remoteId,
                                                      const QString& quality,
                                                      const std::atomic<bool>* abortFlag,
                                                      int timeoutMs);
    ResolvedStreamInfo resolveViaMetingBlocking(const ConfigSnapshot& cfg,
                                                const QString& remoteId,
                                                const QString& quality,
                                                const std::atomic<bool>* abortFlag,
                                                int timeoutMs);

    AppSettings* settings_ = nullptr;
    StreamCacheManager* cacheManager_ = nullptr;
    QNetworkAccessManager* nam_ = nullptr;

    mutable std::mutex cfgMutex_;
    ConfigSnapshot cfg_;
};

} // namespace virela
