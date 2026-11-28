#include "online/OnlineMusicService.h"
#include "core/AppSettings.h"
#include "online/StreamCacheManager.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <memory>

namespace virela {

namespace {

constexpr int kDefaultTimeoutMs = 12000;
constexpr qint64 kMaxCoverBytes = 10 * 1024 * 1024;

QString extractJsonMessage(const QByteArray& body) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString msg = obj.value(QStringLiteral("message")).toString();
        if (!msg.isEmpty()) return msg;
        const QString msg2 = obj.value(QStringLiteral("msg")).toString();
        if (!msg2.isEmpty()) return msg2;
    }
    return {};
}

QString joinArtistNames(const QJsonArray& arr, QString* firstArtistIdOut = nullptr) {
    QStringList names;
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject a = v.toObject();
        const QString n = a.value(QStringLiteral("name")).toString().trimmed();
        if (!n.isEmpty()) names.append(n);
        if (firstArtistIdOut && firstArtistIdOut->isEmpty()) {
            const qint64 aid = static_cast<qint64>(a.value(QStringLiteral("id")).toDouble(0));
            if (aid > 0) *firstArtistIdOut = QString::number(aid);
        }
    }
    return names.join(QStringLiteral(" / "));
}

TrackInfo songObjectToTrack(const QJsonObject& obj) {
    TrackInfo t;
    t.source = QStringLiteral("netease");
    const qint64 idVal = static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0));
    if (idVal > 0) {
        t.remoteId = QString::number(idVal);
    } else {
        t.remoteId = obj.value(QStringLiteral("id")).toString();
    }
    t.title = obj.value(QStringLiteral("name")).toString().trimmed();

    QJsonArray ar = obj.value(QStringLiteral("ar")).toArray();
    if (ar.isEmpty()) ar = obj.value(QStringLiteral("artists")).toArray();
    t.artist = joinArtistNames(ar, &t.remoteArtistId);
    t.albumArtist = t.artist;

    QJsonObject al = obj.value(QStringLiteral("al")).toObject();
    if (al.isEmpty()) al = obj.value(QStringLiteral("album")).toObject();
    if (!al.isEmpty()) {
        t.album = al.value(QStringLiteral("name")).toString().trimmed();
        const qint64 alId = static_cast<qint64>(al.value(QStringLiteral("id")).toDouble(0));
        if (alId > 0) t.remoteAlbumId = QString::number(alId);
        t.coverUrl = al.value(QStringLiteral("picUrl")).toString().trimmed();
        if (t.coverUrl.isEmpty()) {
            t.coverUrl = al.value(QStringLiteral("blurPicUrl")).toString().trimmed();
        }
    }

    qint64 dt = static_cast<qint64>(obj.value(QStringLiteral("dt")).toDouble(0));
    if (dt <= 0) dt = static_cast<qint64>(obj.value(QStringLiteral("duration")).toDouble(0));
    t.durationMs = std::max<qint64>(0, dt);
    t.trackNumber = obj.value(QStringLiteral("no")).toInt(0);

    const QJsonObject priv = obj.value(QStringLiteral("privilege")).toObject();
    const QString maxBrLevel = priv.value(QStringLiteral("maxBrLevel")).toString();
    if (!maxBrLevel.isEmpty()) {
        t.formatBadge = OnlineMusicService::formatBadgeForQuality(maxBrLevel);
    } else if ((obj.value(QStringLiteral("hr")).isObject() && !obj.value(QStringLiteral("hr")).isNull()) ||
               (obj.value(QStringLiteral("hrMusic")).isObject() && !obj.value(QStringLiteral("hrMusic")).isNull())) {
        t.formatBadge = QStringLiteral("Hi-Res");
    } else if ((obj.value(QStringLiteral("sq")).isObject() && !obj.value(QStringLiteral("sq")).isNull()) ||
               (obj.value(QStringLiteral("sqMusic")).isObject() && !obj.value(QStringLiteral("sqMusic")).isNull())) {
        t.formatBadge = QStringLiteral("Lossless");
    } else {
        t.formatBadge = QStringLiteral("Online");
    }

    return t;
}

QString extractIdFromMetingUrl(const QString& urlStr) {
    if (urlStr.isEmpty()) return {};
    const QUrl u(urlStr);
    const QUrlQuery q(u);
    const QString idParam = q.queryItemValue(QStringLiteral("id")).trimmed();
    if (!idParam.isEmpty()) return idParam;
    static const QRegularExpression re(QStringLiteral("(\\d{4,})"));
    const auto m = re.match(urlStr);
    return m.hasMatch() ? m.captured(1) : QString();
}

} // namespace

OnlineMusicService::OnlineMusicService(AppSettings* settings,
                                       StreamCacheManager* cacheManager,
                                       QObject* parent)
    : QObject(parent)
    , settings_(settings)
    , cacheManager_(cacheManager)
    , nam_(nullptr) {
    syncConfigSnapshot();
    if (settings_) {
        connect(settings_, &AppSettings::onlineSettingsChanged, this, &OnlineMusicService::syncConfigSnapshot);
    }
}

QNetworkAccessManager* OnlineMusicService::ensureNam() {
    if (!nam_) {
        nam_ = new QNetworkAccessManager(this);
    }
    return nam_;
}

void OnlineMusicService::syncConfigSnapshot() {
    if (!settings_) return;
    std::lock_guard<std::mutex> lock(cfgMutex_);
    cfg_.neteaseUrl = normalizeBaseUrl(settings_->onlineNeteaseApiUrl());
    cfg_.metingUrl = settings_->onlineMetingApiUrl().trimmed();
    cfg_.priority = settings_->onlineDirectUrlPriority();
    cfg_.autoFallback = true;
    cfg_.unblockTrialFallback = true;
    cfg_.quality = settings_->onlineAudioQuality();
    cfg_.cookie = settings_->onlineCookie().trimmed();
}

OnlineMusicService::ConfigSnapshot OnlineMusicService::configSnapshot() const {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    return cfg_;
}

QString OnlineMusicService::currentQuality() const {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    return cfg_.quality;
}

QString OnlineMusicService::currentCookie() const {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    return cfg_.cookie;
}

QString OnlineMusicService::normalizeBaseUrl(const QString& rawUrl) {
    QString u = rawUrl.trimmed();
    while (u.endsWith(QLatin1Char('/'))) {
        u.chop(1);
    }
    return u;
}

QString OnlineMusicService::sizedCoverUrl(const QString& rawUrl, int sizePx) {
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty() || sizePx <= 0 || trimmed.startsWith(QStringLiteral("image://"))) {
        return rawUrl;
    }
    QUrl u(trimmed);
    if (!u.isValid() || !u.host().endsWith(QStringLiteral(".music.126.net"), Qt::CaseInsensitive) || u.hasQuery()) {
        return rawUrl;
    }
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("param"), QStringLiteral("%1y%1").arg(sizePx));
    u.setQuery(q);
    return u.toString();
}

QString OnlineMusicService::extractPlaylistId(const QString& input) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return {};

    // Pure numeric playlist ID
    static const QRegularExpression kPureDigits(QStringLiteral("^\\d+$"));
    if (kPureDigits.match(trimmed).hasMatch()) {
        return trimmed;
    }

    // Extract URL if user pasted share text containing a URL
    static const QRegularExpression kUrlInText(QStringLiteral("(https?://[^\\s]+)"),
                                               QRegularExpression::CaseInsensitiveOption);
    QString candidate = trimmed;
    if (auto um = kUrlInText.match(trimmed); um.hasMatch()) {
        candidate = um.captured(1);
    }

    // Handle hash fragment routes like https://music.163.com/#/playlist?id=123456
    static const QRegularExpression kQueryId(QStringLiteral("[?&#]id=(\\d+)"));
    if (auto qm = kQueryId.match(candidate); qm.hasMatch()) {
        return qm.captured(1);
    }

    // Handle path style like /playlist/123456 or playlist/123456
    static const QRegularExpression kPathId(QStringLiteral("/playlist/(\\d+)"));
    if (auto pm = kPathId.match(candidate); pm.hasMatch()) {
        return pm.captured(1);
    }

    return {};
}

QString OnlineMusicService::qualityToNeteaseLevel(const QString& quality) {
    const QString q = quality.trimmed().toLower();
    if (q == QStringLiteral("standard")) return QStringLiteral("standard");
    if (q == QStringLiteral("exhigh") || q == QStringLiteral("higher")) return QStringLiteral("exhigh");
    if (q == QStringLiteral("hires")) return QStringLiteral("hires");
    return QStringLiteral("lossless");
}

int OnlineMusicService::qualityToBitrate(const QString& quality) {
    const QString q = quality.trimmed().toLower();
    if (q == QStringLiteral("standard")) return 128000;
    if (q == QStringLiteral("exhigh") || q == QStringLiteral("higher")) return 320000;
    return 999000; // lossless / hires
}

QString OnlineMusicService::formatBadgeForQuality(const QString& qualityOrLevel, const QString& codecHint) {
    const QString q = qualityOrLevel.trimmed().toLower();
    const QString c = codecHint.trimmed().toUpper();
    if (q == QStringLiteral("hires") || q == QStringLiteral("jymaster") || q == QStringLiteral("sky")) {
        return QStringLiteral("Hi-Res");
    }
    if (q == QStringLiteral("lossless") || q == QStringLiteral("sq") || c == QStringLiteral("FLAC")) {
        return QStringLiteral("Lossless");
    }
    if (q == QStringLiteral("exhigh") || q == QStringLiteral("higher")) {
        return QStringLiteral("HQ 320k");
    }
    if (q == QStringLiteral("standard")) {
        return c.isEmpty() ? QStringLiteral("MP3") : c;
    }
    if (!c.isEmpty()) return c;
    return QStringLiteral("Online");
}

QUrl OnlineMusicService::buildNeteaseUrl(const QString& base,
                                         const QString& path,
                                         const QVector<std::pair<QString, QString>>& queryItems) {
    const QString norm = normalizeBaseUrl(base);
    if (norm.isEmpty()) return {};
    const QString cleanPath = path.startsWith(QLatin1Char('/')) ? path : (QLatin1Char('/') + path);
    QUrl url(norm + cleanPath);
    if (!queryItems.isEmpty()) {
        QUrlQuery q(url);
        for (const auto& kv : queryItems) {
            q.addQueryItem(kv.first, kv.second);
        }
        url.setQuery(q);
    }
    return url;
}

QUrl OnlineMusicService::buildMetingUrl(const QString& base,
                                        const QString& server,
                                        const QString& type,
                                        const QString& id,
                                        int br) {
    const QString norm = base.trimmed();
    if (norm.isEmpty()) return {};
    QUrl url(norm);
    QUrlQuery q(url);
    q.addQueryItem(QStringLiteral("server"), server);
    q.addQueryItem(QStringLiteral("type"), type);
    q.addQueryItem(QStringLiteral("id"), id);
    if (br > 0) {
        q.addQueryItem(QStringLiteral("br"), QString::number(br));
    }
    url.setQuery(q);
    return url;
}

QVector<TrackInfo> OnlineMusicService::parseNeteaseSongsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<TrackInfo> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) {
            const QString m = extractJsonMessage(jsonBytes);
            *errorOut = m.isEmpty() ? QStringLiteral("API error code %1").arg(code) : m;
        }
        return out;
    }

    QJsonArray arr = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("songs")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("songs")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("hotSongs")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("data")).toArray();

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        TrackInfo t = songObjectToTrack(v.toObject());
        if (!t.remoteId.isEmpty()) {
            out.push_back(std::move(t));
        }
    }
    return out;
}

QVector<OnlineAlbumItem> OnlineMusicService::parseNeteaseAlbumsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<OnlineAlbumItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = extractJsonMessage(jsonBytes);
        return out;
    }

    QJsonArray arr = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("albums")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("albums")).toArray();

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        OnlineAlbumItem item;
        item.id = QString::number(static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0)));
        item.name = obj.value(QStringLiteral("name")).toString().trimmed();
        item.coverUrl = obj.value(QStringLiteral("picUrl")).toString().trimmed();
        if (item.coverUrl.isEmpty()) item.coverUrl = obj.value(QStringLiteral("blurPicUrl")).toString().trimmed();

        const QJsonObject artistObj = obj.value(QStringLiteral("artist")).toObject();
        if (!artistObj.isEmpty()) {
            item.artist = artistObj.value(QStringLiteral("name")).toString().trimmed();
            const qint64 aid = static_cast<qint64>(artistObj.value(QStringLiteral("id")).toDouble(0));
            if (aid > 0) item.artistId = QString::number(aid);
        } else {
            item.artist = joinArtistNames(obj.value(QStringLiteral("artists")).toArray(), &item.artistId);
        }

        const qint64 pubMs = static_cast<qint64>(obj.value(QStringLiteral("publishTime")).toDouble(0));
        if (pubMs > 0) {
            item.year = QDateTime::fromMSecsSinceEpoch(pubMs).date().year();
        }
        item.trackCount = obj.value(QStringLiteral("size")).toInt(0);
        item.description = obj.value(QStringLiteral("description")).toString().trimmed();
        if (item.id != QStringLiteral("0")) {
            out.push_back(std::move(item));
        }
    }
    return out;
}

QVector<OnlineArtistItem> OnlineMusicService::parseNeteaseArtistsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<OnlineArtistItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = extractJsonMessage(jsonBytes);
        return out;
    }

    QJsonArray arr = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("artists")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("artists")).toArray();

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        OnlineArtistItem item;
        item.id = QString::number(static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0)));
        item.name = obj.value(QStringLiteral("name")).toString().trimmed();
        item.coverUrl = obj.value(QStringLiteral("picUrl")).toString().trimmed();
        if (item.coverUrl.isEmpty()) item.coverUrl = obj.value(QStringLiteral("img1v1Url")).toString().trimmed();
        item.albumCount = obj.value(QStringLiteral("albumSize")).toInt(0);
        item.trackCount = obj.value(QStringLiteral("musicSize")).toInt(0);
        item.briefDesc = obj.value(QStringLiteral("briefDesc")).toString().trimmed();
        if (item.id != QStringLiteral("0")) {
            out.push_back(std::move(item));
        }
    }
    return out;
}

QVector<OnlinePlaylistItem> OnlineMusicService::parseNeteasePlaylistsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<OnlinePlaylistItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = extractJsonMessage(jsonBytes);
        return out;
    }

    QJsonArray arr = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("playlists")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("playlists")).toArray();

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        OnlinePlaylistItem item;
        item.id = QString::number(static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0)));
        item.name = obj.value(QStringLiteral("name")).toString().trimmed();
        item.coverUrl = obj.value(QStringLiteral("coverImgUrl")).toString().trimmed();
        item.creator = obj.value(QStringLiteral("creator")).toObject().value(QStringLiteral("nickname")).toString().trimmed();
        item.trackCount = obj.value(QStringLiteral("trackCount")).toInt(0);
        item.playCount = static_cast<qint64>(obj.value(QStringLiteral("playCount")).toDouble(0));
        item.description = obj.value(QStringLiteral("description")).toString().trimmed();
        if (item.id != QStringLiteral("0")) {
            out.push_back(std::move(item));
        }
    }
    return out;
}

OnlinePlaylistDetail OnlineMusicService::parseNeteasePlaylistDetailJson(const QByteArray& jsonBytes) {
    OnlinePlaylistDetail res;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("Invalid playlist response");
        return res;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        res.error = extractJsonMessage(jsonBytes);
        if (res.error.isEmpty()) res.error = QStringLiteral("Playlist request failed (%1)").arg(code);
        return res;
    }

    const QJsonObject pl = root.value(QStringLiteral("playlist")).toObject();
    if (pl.isEmpty()) {
        res.error = QStringLiteral("Playlist not found");
        return res;
    }

    res.info.id = QString::number(static_cast<qint64>(pl.value(QStringLiteral("id")).toDouble(0)));
    res.info.name = pl.value(QStringLiteral("name")).toString().trimmed();
    res.info.coverUrl = pl.value(QStringLiteral("coverImgUrl")).toString().trimmed();
    res.info.creator = pl.value(QStringLiteral("creator")).toObject().value(QStringLiteral("nickname")).toString().trimmed();
    res.info.trackCount = pl.value(QStringLiteral("trackCount")).toInt(0);
    res.info.playCount = static_cast<qint64>(pl.value(QStringLiteral("playCount")).toDouble(0));
    res.info.description = pl.value(QStringLiteral("description")).toString().trimmed();

    const QJsonArray tracksArr = pl.value(QStringLiteral("tracks")).toArray();
    res.tracks.reserve(tracksArr.size());
    for (const QJsonValue& v : tracksArr) {
        if (!v.isObject()) continue;
        TrackInfo t = songObjectToTrack(v.toObject());
        if (!t.remoteId.isEmpty()) res.tracks.push_back(std::move(t));
    }
    res.ok = true;
    return res;
}

OnlineAlbumDetail OnlineMusicService::parseNeteaseAlbumDetailJson(const QByteArray& jsonBytes) {
    OnlineAlbumDetail res;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("Invalid album response");
        return res;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        res.error = extractJsonMessage(jsonBytes);
        return res;
    }

    const QJsonObject al = root.value(QStringLiteral("album")).toObject();
    res.info.id = QString::number(static_cast<qint64>(al.value(QStringLiteral("id")).toDouble(0)));
    res.info.name = al.value(QStringLiteral("name")).toString().trimmed();
    res.info.coverUrl = al.value(QStringLiteral("picUrl")).toString().trimmed();
    res.info.artist = al.value(QStringLiteral("artist")).toObject().value(QStringLiteral("name")).toString().trimmed();
    if (res.info.artist.isEmpty()) {
        res.info.artist = joinArtistNames(al.value(QStringLiteral("artists")).toArray(), &res.info.artistId);
    }
    const qint64 pubMs = static_cast<qint64>(al.value(QStringLiteral("publishTime")).toDouble(0));
    if (pubMs > 0) res.info.year = QDateTime::fromMSecsSinceEpoch(pubMs).date().year();
    res.info.description = al.value(QStringLiteral("description")).toString().trimmed();

    const QJsonArray songsArr = root.value(QStringLiteral("songs")).toArray();
    res.tracks.reserve(songsArr.size());
    for (const QJsonValue& v : songsArr) {
        if (!v.isObject()) continue;
        TrackInfo t = songObjectToTrack(v.toObject());
        if (t.album.isEmpty()) t.album = res.info.name;
        if (t.coverUrl.isEmpty()) t.coverUrl = res.info.coverUrl;
        if (t.year <= 0) t.year = res.info.year;
        if (!t.remoteId.isEmpty()) res.tracks.push_back(std::move(t));
    }
    res.info.trackCount = static_cast<int>(res.tracks.size());
    res.ok = true;
    return res;
}

OnlineArtistDetail OnlineMusicService::parseNeteaseArtistDetailJson(const QByteArray& jsonBytes) {
    OnlineArtistDetail res;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("Invalid artist response");
        return res;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        res.error = extractJsonMessage(jsonBytes);
        return res;
    }

    const QJsonObject ar = root.value(QStringLiteral("artist")).toObject();
    if (!ar.isEmpty()) {
        res.info.id = QString::number(static_cast<qint64>(ar.value(QStringLiteral("id")).toDouble(0)));
        res.info.name = ar.value(QStringLiteral("name")).toString().trimmed();
        res.info.coverUrl = ar.value(QStringLiteral("picUrl")).toString().trimmed();
        if (res.info.coverUrl.isEmpty()) res.info.coverUrl = ar.value(QStringLiteral("img1v1Url")).toString().trimmed();
        res.info.albumCount = ar.value(QStringLiteral("albumSize")).toInt(0);
        res.info.trackCount = ar.value(QStringLiteral("musicSize")).toInt(0);
        res.info.briefDesc = ar.value(QStringLiteral("briefDesc")).toString().trimmed();
    }

    QJsonArray songsArr = root.value(QStringLiteral("hotSongs")).toArray();
    if (songsArr.isEmpty()) songsArr = root.value(QStringLiteral("songs")).toArray();
    res.tracks.reserve(songsArr.size());
    for (const QJsonValue& v : songsArr) {
        if (!v.isObject()) continue;
        TrackInfo t = songObjectToTrack(v.toObject());
        if (!t.remoteId.isEmpty()) res.tracks.push_back(std::move(t));
    }
    res.ok = true;
    return res;
}

OnlineLyricsResult OnlineMusicService::parseNeteaseLyricsJson(const QByteArray& jsonBytes) {
    OnlineLyricsResult res;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        res.error = QStringLiteral("Invalid lyrics JSON");
        return res;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        res.error = extractJsonMessage(jsonBytes);
        return res;
    }

    res.yrc = root.value(QStringLiteral("yrc")).toObject().value(QStringLiteral("lyric")).toString();
    res.lrc = root.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
    const QString ytlrc = root.value(QStringLiteral("ytlrc")).toObject().value(QStringLiteral("lyric")).toString();
    const QString tlyric = root.value(QStringLiteral("tlyric")).toObject().value(QStringLiteral("lyric")).toString();
    res.tlyric = (!res.yrc.trimmed().isEmpty() && !ytlrc.trimmed().isEmpty()) ? ytlrc : tlyric;
    res.sourceLabel = !res.yrc.trimmed().isEmpty() ? QStringLiteral("Online YRC") : QStringLiteral("Online LRC");
    res.ok = true;
    return res;
}

QVector<TrackInfo> OnlineMusicService::parseMetingTracksJson(const QByteArray& jsonBytes, const QString& fallbackAlbum) {
    QVector<TrackInfo> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.value(QStringLiteral("songs")).isArray()) arr = root.value(QStringLiteral("songs")).toArray();
        else if (root.value(QStringLiteral("data")).isArray()) arr = root.value(QStringLiteral("data")).toArray();
        else if (root.value(QStringLiteral("result")).isObject()) {
            arr = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("songs")).toArray();
        }
    }

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        TrackInfo t;
        t.source = QStringLiteral("meting");
        const qint64 numId = static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0));
        QString idStr = numId > 0 ? QString::number(numId) : obj.value(QStringLiteral("id")).toString().trimmed();
        if (idStr.isEmpty()) {
            idStr = extractIdFromMetingUrl(obj.value(QStringLiteral("url")).toString());
        }
        if (idStr.isEmpty()) {
            idStr = extractIdFromMetingUrl(obj.value(QStringLiteral("lrc")).toString());
        }
        if (idStr.isEmpty()) continue;

        t.remoteId = idStr;
        t.title = obj.value(QStringLiteral("title")).toString().trimmed();
        if (t.title.isEmpty()) t.title = obj.value(QStringLiteral("name")).toString().trimmed();

        t.artist = obj.value(QStringLiteral("author")).toString().trimmed();
        if (t.artist.isEmpty()) t.artist = obj.value(QStringLiteral("artist")).toString().trimmed();

        t.album = obj.value(QStringLiteral("album")).toString().trimmed();
        if (t.album.isEmpty()) t.album = fallbackAlbum;

        t.coverUrl = obj.value(QStringLiteral("pic")).toString().trimmed();
        if (t.coverUrl.isEmpty()) t.coverUrl = obj.value(QStringLiteral("cover")).toString().trimmed();

        const qint64 dur = static_cast<qint64>(obj.value(QStringLiteral("duration")).toDouble(0));
        if (dur > 0) {
            t.durationMs = dur > 10000 ? dur : dur * 1000;
        }
        t.formatBadge = QStringLiteral("Online");
        out.push_back(std::move(t));
    }
    return out;
}

void OnlineMusicService::asyncGet(
    const QUrl& url,
    const QString& cookie,
    int timeoutMs,
    bool followRedirects,
    std::function<void(bool ok, int status, QByteArray body, QUrl redirectUrl, QString err)> callback) {
    if (!url.isValid() || url.isEmpty()) {
        if (callback) callback(false, 0, {}, {}, QStringLiteral("Backend URL is not configured"));
        return;
    }

    auto doRequest = std::make_shared<std::function<void(const QUrl&, const QString&, int)>>();
    *doRequest = [this, timeoutMs, followRedirects, callback, doRequest](const QUrl& curUrl, const QString& curCookie, int hop) {
        QNetworkRequest req(curUrl);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Virela/2.0"));
        if (!curCookie.isEmpty()) {
            req.setRawHeader("Cookie", curCookie.toUtf8());
        }

        QNetworkReply* reply = ensureNam()->get(req);
        auto timedOut = std::make_shared<bool>(false);
        auto* timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, [reply, timedOut]() {
            *timedOut = true;
            reply->abort();
        });
        timer->start(std::max(500, timeoutMs));

        connect(reply, &QNetworkReply::finished, this,
                [this, reply, curUrl, curCookie, hop, followRedirects, callback, timedOut, doRequest]() {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl loc = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            const QUrl resolvedLoc = loc.isValid() ? curUrl.resolved(loc) : QUrl();

            if (followRedirects && status >= 300 && status < 400 && resolvedLoc.isValid() && hop < 5) {
                reply->deleteLater();
                const QString nextCookie = (resolvedLoc.host() == curUrl.host()) ? curCookie : QString();
                (*doRequest)(resolvedLoc, nextCookie, hop + 1);
                return;
            }

            const QByteArray body = reply->readAll();
            const bool netOk = !*timedOut && reply->error() == QNetworkReply::NoError;
            const bool redirOk = !*timedOut && !followRedirects && status >= 300 && status < 400 && resolvedLoc.isValid();
            QString err = *timedOut ? QStringLiteral("Request timed out") : reply->errorString();
            reply->deleteLater();

            if (callback) {
                callback(netOk || redirOk, status, body, resolvedLoc, (netOk || redirOk) ? QString() : err);
            }
        });
    };

    (*doRequest)(url, cookie, 0);
}

void OnlineMusicService::testConnections(std::function<void(BackendTestResult netease, BackendTestResult meting)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    struct State {
        int remaining = 2;
        BackendTestResult netease;
        BackendTestResult meting;
        std::function<void(BackendTestResult, BackendTestResult)> cb;
        void finishOne() {
            if (--remaining == 0 && cb) {
                cb(netease, meting);
            }
        }
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    // 1. NeteaseCloudMusicApi test
    if (cfg.neteaseUrl.isEmpty()) {
        st->netease.configured = false;
        st->netease.ok = false;
        st->netease.message = tr("Not configured");
        st->finishOne();
    } else {
        st->netease.configured = true;
        auto timer = std::make_shared<QElapsedTimer>();
        timer->start();
        const QUrl testUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/search/hot"));
        asyncGet(testUrl, cfg.cookie, 6000, true, [this, cfg, st, timer](bool ok, int status, const QByteArray& body, const QUrl&, const QString& err) {
            if (status == 404) {
                const QUrl fbUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/cloudsearch"),
                                                   {{QStringLiteral("keywords"), QStringLiteral("test")},
                                                    {QStringLiteral("limit"), QStringLiteral("1")},
                                                    {QStringLiteral("type"), QStringLiteral("1")}});
                asyncGet(fbUrl, cfg.cookie, 6000, true, [st, timer](bool ok2, int status2, const QByteArray& body2, const QUrl&, const QString& err2) {
                    st->netease.latencyMs = static_cast<int>(timer->elapsed());
                    st->netease.statusCode = status2;
                    const QJsonDocument doc = QJsonDocument::fromJson(body2);
                    const int apiCode = doc.isObject() ? doc.object().value(QStringLiteral("code")).toInt(200) : 200;
                    st->netease.ok = ok2 && status2 >= 200 && status2 < 400 && apiCode == 200;
                    st->netease.message = st->netease.ok
                        ? tr("Connected (%1 ms)").arg(st->netease.latencyMs)
                        : (!err2.isEmpty() ? err2 : tr("HTTP %1 / code %2").arg(status2).arg(apiCode));
                    st->finishOne();
                });
                return;
            }
            st->netease.latencyMs = static_cast<int>(timer->elapsed());
            st->netease.statusCode = status;
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            const int apiCode = doc.isObject() ? doc.object().value(QStringLiteral("code")).toInt(200) : 200;
            st->netease.ok = ok && status >= 200 && status < 400 && apiCode == 200;
            st->netease.message = st->netease.ok
                ? tr("Connected (%1 ms)").arg(st->netease.latencyMs)
                : (!err.isEmpty() ? err : tr("HTTP %1 / code %2").arg(status).arg(apiCode));
            st->finishOne();
        });
    }

    // 2. Meting API test (optional)
    if (cfg.metingUrl.isEmpty()) {
        st->meting.configured = false;
        st->meting.ok = false;
        st->meting.message = tr("Optional (not configured)");
        st->finishOne();
    } else {
        st->meting.configured = true;
        auto timer = std::make_shared<QElapsedTimer>();
        timer->start();
        const QUrl testUrl = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("song"), QStringLiteral("1"));
        asyncGet(testUrl, cfg.cookie, 6000, false, [st, timer](bool ok, int status, const QByteArray&, const QUrl&, const QString& err) {
            st->meting.latencyMs = static_cast<int>(timer->elapsed());
            st->meting.statusCode = status;
            st->meting.ok = ok && status >= 200 && status < 400;
            st->meting.message = st->meting.ok
                ? tr("Connected (%1 ms)").arg(st->meting.latencyMs)
                : (!err.isEmpty() ? err : tr("HTTP %1").arg(status));
            st->finishOne();
        });
    }
}

void OnlineMusicService::searchSongs(const QString& query, int limit, int offset,
                                     std::function<void(bool ok, QVector<TrackInfo> tracks, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        if (callback) callback(true, {}, {});
        return;
    }

    if (!cfg.neteaseUrl.isEmpty()) {
        const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/cloudsearch"), {
            {QStringLiteral("keywords"), q},
            {QStringLiteral("type"), QStringLiteral("1")},
            {QStringLiteral("limit"), QString::number(limit)},
            {QStringLiteral("offset"), QString::number(offset)}
        });
        asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true,
                 [this, cfg, q, limit, offset, callback](bool ok, int status, const QByteArray& body, const QUrl&, const QString& err) {
            if (status == 404) {
                const QUrl fb = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/search"), {
                    {QStringLiteral("keywords"), q},
                    {QStringLiteral("type"), QStringLiteral("1")},
                    {QStringLiteral("limit"), QString::number(limit)},
                    {QStringLiteral("offset"), QString::number(offset)}
                });
                asyncGet(fb, cfg.cookie, kDefaultTimeoutMs, true, [callback](bool ok2, int, const QByteArray& body2, const QUrl&, const QString& err2) {
                    if (!ok2) {
                        if (callback) callback(false, {}, err2);
                        return;
                    }
                    QString parseErr;
                    auto tracks = parseNeteaseSongsJson(body2, &parseErr);
                    if (callback) callback(parseErr.isEmpty(), std::move(tracks), parseErr);
                });
                return;
            }
            if (!ok) {
                if (callback) callback(false, {}, err);
                return;
            }
            QString parseErr;
            auto tracks = parseNeteaseSongsJson(body, &parseErr);
            if (callback) callback(parseErr.isEmpty(), std::move(tracks), parseErr);
        });
        return;
    }

    if (!cfg.metingUrl.isEmpty()) {
        const QUrl url = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("search"), q);
        asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true, [callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
            if (!ok) {
                if (callback) callback(false, {}, err);
                return;
            }
            auto tracks = parseMetingTracksJson(body);
            if (tracks.isEmpty()) tracks = parseNeteaseSongsJson(body);
            if (callback) callback(true, std::move(tracks), {});
        });
        return;
    }

    if (callback) callback(false, {}, tr("Please configure a Netease API or Meting API URL in Settings."));
}

void OnlineMusicService::searchAlbums(const QString& query, int limit, int offset,
                                      std::function<void(bool ok, QVector<OnlineAlbumItem> albums, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        if (callback) callback(true, {}, {});
        return;
    }
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("Album search requires NeteaseCloudMusicApi URL in Settings."));
        return;
    }

    const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/cloudsearch"), {
        {QStringLiteral("keywords"), q},
        {QStringLiteral("type"), QStringLiteral("10")},
        {QStringLiteral("limit"), QString::number(limit)},
        {QStringLiteral("offset"), QString::number(offset)}
    });
    asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true, [callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        if (!ok) {
            if (callback) callback(false, {}, err);
            return;
        }
        QString parseErr;
        auto albums = parseNeteaseAlbumsJson(body, &parseErr);
        if (callback) callback(parseErr.isEmpty(), std::move(albums), parseErr);
    });
}

void OnlineMusicService::searchArtists(const QString& query, int limit, int offset,
                                       std::function<void(bool ok, QVector<OnlineArtistItem> artists, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        if (callback) callback(true, {}, {});
        return;
    }
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("Artist search requires NeteaseCloudMusicApi URL in Settings."));
        return;
    }

    const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/cloudsearch"), {
        {QStringLiteral("keywords"), q},
        {QStringLiteral("type"), QStringLiteral("100")},
        {QStringLiteral("limit"), QString::number(limit)},
        {QStringLiteral("offset"), QString::number(offset)}
    });
    asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true, [callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        if (!ok) {
            if (callback) callback(false, {}, err);
            return;
        }
        QString parseErr;
        auto artists = parseNeteaseArtistsJson(body, &parseErr);
        if (callback) callback(parseErr.isEmpty(), std::move(artists), parseErr);
    });
}

void OnlineMusicService::searchPlaylists(const QString& query, int limit, int offset,
                                         std::function<void(bool ok, QVector<OnlinePlaylistItem> playlists, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        if (callback) callback(true, {}, {});
        return;
    }
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("Playlist search requires NeteaseCloudMusicApi URL in Settings."));
        return;
    }

    const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/cloudsearch"), {
        {QStringLiteral("keywords"), q},
        {QStringLiteral("type"), QStringLiteral("1000")},
        {QStringLiteral("limit"), QString::number(limit)},
        {QStringLiteral("offset"), QString::number(offset)}
    });
    asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true, [callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        if (!ok) {
            if (callback) callback(false, {}, err);
            return;
        }
        QString parseErr;
        auto playlists = parseNeteasePlaylistsJson(body, &parseErr);
        if (callback) callback(parseErr.isEmpty(), std::move(playlists), parseErr);
    });
}

QVector<OnlineHotSearchItem> OnlineMusicService::parseNeteaseHotSearchJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<OnlineHotSearchItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = root.value(QStringLiteral("message")).toString(QStringLiteral("API error (%1)").arg(code));
        return out;
    }

    // Primary (/search/hot/detail): root.data[]
    QJsonArray arr = root.value(QStringLiteral("data")).toArray();
    if (!arr.isEmpty()) {
        out.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            const QString word = obj.value(QStringLiteral("searchWord")).toString().trimmed();
            if (word.isEmpty()) continue;
            OnlineHotSearchItem item;
            item.searchWord = word;
            item.content = obj.value(QStringLiteral("content")).toString().trimmed();
            item.iconUrl = obj.value(QStringLiteral("iconUrl")).toString().trimmed();
            item.score = obj.value(QStringLiteral("score")).toInt(0);
            item.iconType = obj.value(QStringLiteral("iconType")).toInt(0);
            out.push_back(std::move(item));
        }
        return out;
    }

    // Fallback (/search/hot): root.result.hots[]
    const QJsonArray hots = root.value(QStringLiteral("result")).toObject().value(QStringLiteral("hots")).toArray();
    out.reserve(hots.size());
    for (const QJsonValue& v : hots) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        const QString word = obj.value(QStringLiteral("first")).toString().trimmed();
        if (word.isEmpty()) continue;
        OnlineHotSearchItem item;
        item.searchWord = word;
        item.score = obj.value(QStringLiteral("second")).toInt(0);
        item.iconType = obj.value(QStringLiteral("iconType")).toInt(0);
        out.push_back(std::move(item));
    }
    return out;
}

QVector<OnlinePlaylistItem> OnlineMusicService::parseNeteaseRecommendedPlaylistsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<OnlinePlaylistItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = root.value(QStringLiteral("message")).toString(QStringLiteral("API error (%1)").arg(code));
        return out;
    }

    // Primary (/personalized): root.result[]
    const QJsonArray resultArr = root.value(QStringLiteral("result")).toArray();
    if (!resultArr.isEmpty()) {
        out.reserve(resultArr.size());
        for (const QJsonValue& v : resultArr) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            const qint64 id = static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble(0));
            if (id <= 0) continue;
            OnlinePlaylistItem p;
            p.id = QString::number(id);
            p.name = obj.value(QStringLiteral("name")).toString().trimmed();
            p.coverUrl = obj.value(QStringLiteral("picUrl")).toString().trimmed();
            p.trackCount = obj.value(QStringLiteral("trackCount")).toInt(0);
            p.playCount = static_cast<qint64>(obj.value(QStringLiteral("playCount")).toDouble(0));
            p.description = obj.value(QStringLiteral("copywriter")).toString().trimmed();
            p.creator = obj.value(QStringLiteral("copywriter")).toString().trimmed();
            out.push_back(std::move(p));
        }
        return out;
    }

    // Fallback (/top/playlist): root.playlists[]
    return parseNeteasePlaylistsJson(jsonBytes, errorOut);
}

QVector<TrackInfo> OnlineMusicService::parseNeteaseNewSongsJson(const QByteArray& jsonBytes, QString* errorOut) {
    QVector<TrackInfo> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        if (errorOut) *errorOut = root.value(QStringLiteral("message")).toString(QStringLiteral("API error (%1)").arg(code));
        return out;
    }

    QJsonArray arr = root.value(QStringLiteral("result")).toArray();
    if (arr.isEmpty()) arr = root.value(QStringLiteral("data")).toArray();

    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject outer = v.toObject();
        const QJsonObject songObj = outer.value(QStringLiteral("song")).isObject()
            ? outer.value(QStringLiteral("song")).toObject()
            : outer;
        TrackInfo t = songObjectToTrack(songObj);
        if (t.remoteId.isEmpty()) {
            const qint64 outerId = static_cast<qint64>(outer.value(QStringLiteral("id")).toDouble(0));
            if (outerId > 0) t.remoteId = QString::number(outerId);
        }
        if (t.remoteId.isEmpty()) continue;
        if (t.title.isEmpty()) {
            t.title = outer.value(QStringLiteral("name")).toString().trimmed();
        }
        if (t.coverUrl.isEmpty()) {
            t.coverUrl = outer.value(QStringLiteral("picUrl")).toString().trimmed();
        }
        out.push_back(std::move(t));
    }
    return out;
}

void OnlineMusicService::fetchHotSearch(int limit,
                                          std::function<void(bool ok, QVector<OnlineHotSearchItem> items, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("NeteaseCloudMusicApi URL is not configured."));
        return;
    }
    const QUrl primaryUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/search/hot/detail"));
    asyncGet(primaryUrl, cfg.cookie, kDefaultTimeoutMs, true,
             [this, cfg, limit, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        QString parseErr;
        QVector<OnlineHotSearchItem> items = ok ? parseNeteaseHotSearchJson(body, &parseErr) : QVector<OnlineHotSearchItem>{};
        if (ok && parseErr.isEmpty() && !items.isEmpty()) {
            if (limit > 0 && items.size() > limit) items.resize(limit);
            if (callback) callback(true, std::move(items), {});
            return;
        }
        const QUrl fallbackUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/search/hot"));
        asyncGet(fallbackUrl, cfg.cookie, kDefaultTimeoutMs, true,
                 [limit, callback, primaryErr = err, primaryParseErr = parseErr](bool fbOk, int, const QByteArray& fbBody, const QUrl&, const QString& fbErr) {
            if (!fbOk) {
                const QString outErr = !fbErr.isEmpty() ? fbErr : (!primaryErr.isEmpty() ? primaryErr : primaryParseErr);
                if (callback) callback(false, {}, outErr);
                return;
            }
            QString fbParseErr;
            QVector<OnlineHotSearchItem> fbItems = parseNeteaseHotSearchJson(fbBody, &fbParseErr);
            if (limit > 0 && fbItems.size() > limit) fbItems.resize(limit);
            if (callback) callback(fbParseErr.isEmpty(), std::move(fbItems), fbParseErr);
        });
    });
}

void OnlineMusicService::fetchRecommendedPlaylists(int limit,
                                                     std::function<void(bool ok, QVector<OnlinePlaylistItem> playlists, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("NeteaseCloudMusicApi URL is not configured."));
        return;
    }
    const int targetLimit = limit > 0 ? limit : 12;
    const QUrl primaryUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/personalized"), {
        {QStringLiteral("limit"), QString::number(targetLimit)}
    });
    asyncGet(primaryUrl, cfg.cookie, kDefaultTimeoutMs, true,
             [this, cfg, targetLimit, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        QString parseErr;
        QVector<OnlinePlaylistItem> playlists = ok ? parseNeteaseRecommendedPlaylistsJson(body, &parseErr) : QVector<OnlinePlaylistItem>{};
        if (ok && parseErr.isEmpty() && !playlists.isEmpty()) {
            if (playlists.size() > targetLimit) playlists.resize(targetLimit);
            if (callback) callback(true, std::move(playlists), {});
            return;
        }
        const QUrl fallbackUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/top/playlist"), {
            {QStringLiteral("limit"), QString::number(targetLimit)},
            {QStringLiteral("order"), QStringLiteral("hot")}
        });
        asyncGet(fallbackUrl, cfg.cookie, kDefaultTimeoutMs, true,
                 [targetLimit, callback, primaryErr = err, primaryParseErr = parseErr](bool fbOk, int, const QByteArray& fbBody, const QUrl&, const QString& fbErr) {
            if (!fbOk) {
                const QString outErr = !fbErr.isEmpty() ? fbErr : (!primaryErr.isEmpty() ? primaryErr : primaryParseErr);
                if (callback) callback(false, {}, outErr);
                return;
            }
            QString fbParseErr;
            QVector<OnlinePlaylistItem> fbPlaylists = parseNeteaseRecommendedPlaylistsJson(fbBody, &fbParseErr);
            if (fbPlaylists.size() > targetLimit) fbPlaylists.resize(targetLimit);
            if (callback) callback(fbParseErr.isEmpty(), std::move(fbPlaylists), fbParseErr);
        });
    });
}

void OnlineMusicService::fetchNewSongs(int limit,
                                         std::function<void(bool ok, QVector<TrackInfo> tracks, QString error)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    if (cfg.neteaseUrl.isEmpty()) {
        if (callback) callback(false, {}, tr("NeteaseCloudMusicApi URL is not configured."));
        return;
    }
    const int targetLimit = limit > 0 ? limit : 16;
    const QUrl primaryUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/personalized/newsong"), {
        {QStringLiteral("limit"), QString::number(targetLimit)}
    });
    asyncGet(primaryUrl, cfg.cookie, kDefaultTimeoutMs, true,
             [this, cfg, targetLimit, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
        QString parseErr;
        QVector<TrackInfo> tracks = ok ? parseNeteaseNewSongsJson(body, &parseErr) : QVector<TrackInfo>{};
        if (ok && parseErr.isEmpty() && !tracks.isEmpty()) {
            if (tracks.size() > targetLimit) tracks.resize(targetLimit);
            if (callback) callback(true, std::move(tracks), {});
            return;
        }
        const QUrl fallbackUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/top/song"), {
            {QStringLiteral("type"), QStringLiteral("0")}
        });
        asyncGet(fallbackUrl, cfg.cookie, kDefaultTimeoutMs, true,
                 [targetLimit, callback, primaryErr = err, primaryParseErr = parseErr](bool fbOk, int, const QByteArray& fbBody, const QUrl&, const QString& fbErr) {
            if (!fbOk) {
                const QString outErr = !fbErr.isEmpty() ? fbErr : (!primaryErr.isEmpty() ? primaryErr : primaryParseErr);
                if (callback) callback(false, {}, outErr);
                return;
            }
            QString fbParseErr;
            QVector<TrackInfo> fbTracks = parseNeteaseNewSongsJson(fbBody, &fbParseErr);
            if (fbTracks.size() > targetLimit) fbTracks.resize(targetLimit);
            if (callback) callback(fbParseErr.isEmpty(), std::move(fbTracks), fbParseErr);
        });
    });
}

void OnlineMusicService::fetchPlaylistDetail(const QString& playlistIdOrUrl,
                                             std::function<void(OnlinePlaylistDetail detail)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString pid = extractPlaylistId(playlistIdOrUrl);
    if (pid.isEmpty()) {
        OnlinePlaylistDetail errDetail;
        errDetail.error = tr("Invalid Netease playlist ID or URL.");
        if (callback) callback(std::move(errDetail));
        return;
    }

    auto fallbackMeting = [this, cfg, pid, callback](const QString& priorErr) {
        if (cfg.metingUrl.isEmpty()) {
            OnlinePlaylistDetail errDetail;
            errDetail.error = priorErr.isEmpty() ? tr("Backend URL is not configured.") : priorErr;
            if (callback) callback(std::move(errDetail));
            return;
        }
        const QUrl mUrl = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("playlist"), pid);
        asyncGet(mUrl, cfg.cookie, kDefaultTimeoutMs, true, [pid, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
            OnlinePlaylistDetail detail;
            if (!ok) {
                detail.error = err;
                if (callback) callback(std::move(detail));
                return;
            }
            detail.tracks = parseMetingTracksJson(body);
            detail.info.id = pid;
            detail.info.name = tr("Playlist #%1").arg(pid);
            detail.info.trackCount = static_cast<int>(detail.tracks.size());
            if (!detail.tracks.isEmpty()) {
                detail.info.coverUrl = detail.tracks.first().coverUrl;
            }
            detail.ok = !detail.tracks.isEmpty();
            if (!detail.ok) detail.error = tr("No tracks returned for playlist.");
            if (callback) callback(std::move(detail));
        });
    };

    if (!cfg.neteaseUrl.isEmpty()) {
        const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/playlist/detail"), {
            {QStringLiteral("id"), pid}
        });
        asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true,
                 [this, cfg, pid, callback, fallbackMeting](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
            if (!ok) {
                if (cfg.autoFallback) {
                    fallbackMeting(err);
                } else {
                    OnlinePlaylistDetail d;
                    d.error = err;
                    if (callback) callback(std::move(d));
                }
                return;
            }
            OnlinePlaylistDetail detail = parseNeteasePlaylistDetailJson(body);
            if (!detail.ok) {
                if (cfg.autoFallback) {
                    fallbackMeting(detail.error);
                } else if (callback) {
                    callback(std::move(detail));
                }
                return;
            }

            // If Netease truncated playlist.tracks, fetch full tracks via /playlist/track/all
            if (detail.info.trackCount > detail.tracks.size() && detail.tracks.size() < 500) {
                const QUrl allUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/playlist/track/all"), {
                    {QStringLiteral("id"), pid},
                    {QStringLiteral("limit"), QStringLiteral("500")},
                    {QStringLiteral("offset"), QStringLiteral("0")}
                });
                asyncGet(allUrl, cfg.cookie, kDefaultTimeoutMs, true,
                         [detail, callback](bool okAll, int, const QByteArray& allBody, const QUrl&, const QString&) mutable {
                    if (okAll) {
                        auto fullTracks = parseNeteaseSongsJson(allBody);
                        if (!fullTracks.isEmpty()) {
                            detail.tracks = std::move(fullTracks);
                        }
                    }
                    if (callback) callback(std::move(detail));
                });
                return;
            }

            if (callback) callback(std::move(detail));
        });
        return;
    }

    fallbackMeting({});
}

void OnlineMusicService::fetchAlbumDetail(const QString& albumId,
                                          std::function<void(OnlineAlbumDetail detail)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString aid = albumId.trimmed();
    if (aid.isEmpty()) {
        OnlineAlbumDetail d;
        d.error = tr("Invalid album ID.");
        if (callback) callback(std::move(d));
        return;
    }

    if (!cfg.neteaseUrl.isEmpty()) {
        const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/album"), {{QStringLiteral("id"), aid}});
        asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true,
                 [this, cfg, aid, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
            if (ok) {
                OnlineAlbumDetail detail = parseNeteaseAlbumDetailJson(body);
                if (detail.ok) {
                    if (callback) callback(std::move(detail));
                    return;
                }
            }
            if (cfg.autoFallback && !cfg.metingUrl.isEmpty()) {
                const QUrl mUrl = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("album"), aid);
                asyncGet(mUrl, cfg.cookie, kDefaultTimeoutMs, true, [aid, callback](bool mOk, int, const QByteArray& mBody, const QUrl&, const QString& mErr) {
                    OnlineAlbumDetail d;
                    if (mOk) {
                        d.tracks = parseMetingTracksJson(mBody);
                        d.info.id = aid;
                        d.info.trackCount = static_cast<int>(d.tracks.size());
                        d.ok = !d.tracks.isEmpty();
                    } else {
                        d.error = mErr;
                    }
                    if (callback) callback(std::move(d));
                });
                return;
            }
            OnlineAlbumDetail d;
            d.error = err.isEmpty() ? tr("Failed to load album.") : err;
            if (callback) callback(std::move(d));
        });
        return;
    }

    if (!cfg.metingUrl.isEmpty()) {
        const QUrl mUrl = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("album"), aid);
        asyncGet(mUrl, cfg.cookie, kDefaultTimeoutMs, true, [aid, callback](bool ok, int, const QByteArray& body, const QUrl&, const QString& err) {
            OnlineAlbumDetail d;
            if (ok) {
                d.tracks = parseMetingTracksJson(body);
                d.info.id = aid;
                d.info.trackCount = static_cast<int>(d.tracks.size());
                d.ok = !d.tracks.isEmpty();
            } else {
                d.error = err;
            }
            if (callback) callback(std::move(d));
        });
        return;
    }

    OnlineAlbumDetail d;
    d.error = tr("Backend URL is not configured.");
    if (callback) callback(std::move(d));
}

void OnlineMusicService::fetchArtistDetail(const QString& artistId,
                                           std::function<void(OnlineArtistDetail detail)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString aid = artistId.trimmed();
    if (aid.isEmpty() || cfg.neteaseUrl.isEmpty()) {
        OnlineArtistDetail d;
        d.error = aid.isEmpty() ? tr("Invalid artist ID.") : tr("Artist detail requires NeteaseCloudMusicApi URL.");
        if (callback) callback(std::move(d));
        return;
    }

    const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/artists"), {{QStringLiteral("id"), aid}});
    asyncGet(url, cfg.cookie, kDefaultTimeoutMs, true,
             [this, cfg, aid, callback](bool ok, int status, const QByteArray& body, const QUrl&, const QString& err) {
        if (ok) {
            OnlineArtistDetail d = parseNeteaseArtistDetailJson(body);
            if (d.ok) {
                if (callback) callback(std::move(d));
                return;
            }
        }
        if (status == 404 || !ok) {
            const QUrl topUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/artist/top/song"), {{QStringLiteral("id"), aid}});
            asyncGet(topUrl, cfg.cookie, kDefaultTimeoutMs, true, [aid, callback](bool ok2, int, const QByteArray& body2, const QUrl&, const QString& err2) {
                OnlineArtistDetail d;
                if (ok2) {
                    d.tracks = parseNeteaseSongsJson(body2, &d.error);
                    d.info.id = aid;
                    d.ok = d.error.isEmpty();
                } else {
                    d.error = err2;
                }
                if (callback) callback(std::move(d));
            });
            return;
        }
        OnlineArtistDetail d;
        d.error = err;
        if (callback) callback(std::move(d));
    });
}

void OnlineMusicService::fetchLyrics(const QString& remoteId,
                                     std::function<void(OnlineLyricsResult result)> callback) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString rid = remoteId.trimmed();
    if (rid.isEmpty()) {
        if (callback) callback({});
        return;
    }

    auto fallbackMetingLrc = [this, cfg, rid, callback]() {
        if (cfg.metingUrl.isEmpty()) {
            if (callback) callback({});
            return;
        }
        const QUrl mUrl = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("lrc"), rid);
        asyncGet(mUrl, cfg.cookie, 8000, true, [callback](bool ok, int, const QByteArray& body, const QUrl&, const QString&) {
            OnlineLyricsResult r;
            if (ok && !body.trimmed().isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(body);
                if (doc.isObject()) {
                    r.lrc = doc.object().value(QStringLiteral("lyric")).toString();
                } else {
                    r.lrc = QString::fromUtf8(body);
                }
                r.sourceLabel = QStringLiteral("Meting LRC");
                r.ok = !r.lrc.trimmed().isEmpty();
            }
            if (callback) callback(std::move(r));
        });
    };

    if (!cfg.neteaseUrl.isEmpty()) {
        const QUrl url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/lyric/new"), {{QStringLiteral("id"), rid}});
        asyncGet(url, cfg.cookie, 8000, true,
                 [this, cfg, rid, callback, fallbackMetingLrc](bool ok, int status, const QByteArray& body, const QUrl&, const QString&) {
            if (status == 404) {
                const QUrl fb = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/lyric"), {{QStringLiteral("id"), rid}});
                asyncGet(fb, cfg.cookie, 8000, true, [cfg, callback, fallbackMetingLrc](bool ok2, int, const QByteArray& body2, const QUrl&, const QString&) {
                    if (ok2) {
                        OnlineLyricsResult r = parseNeteaseLyricsJson(body2);
                        if (r.ok && (!r.yrc.trimmed().isEmpty() || !r.lrc.trimmed().isEmpty())) {
                            if (callback) callback(std::move(r));
                            return;
                        }
                    }
                    if (cfg.autoFallback) fallbackMetingLrc();
                    else if (callback) callback({});
                });
                return;
            }
            if (ok) {
                OnlineLyricsResult r = parseNeteaseLyricsJson(body);
                if (r.ok && (!r.yrc.trimmed().isEmpty() || !r.lrc.trimmed().isEmpty())) {
                    if (callback) callback(std::move(r));
                    return;
                }
            }
            if (cfg.autoFallback) fallbackMetingLrc();
            else if (callback) callback({});
        });
        return;
    }

    fallbackMetingLrc();
}

void OnlineMusicService::fetchCoverBytes(const QUrl& coverUrl,
                                         std::function<void(bool ok, QByteArray data)> callback) {
    if (!coverUrl.isValid() || coverUrl.isEmpty()) {
        if (callback) callback(false, {});
        return;
    }
    asyncGet(coverUrl, {}, 8000, true, [callback](bool ok, int status, const QByteArray& body, const QUrl&, const QString&) {
        if (!ok || status >= 400 || body.isEmpty() || body.size() > kMaxCoverBytes) {
            if (callback) callback(false, {});
            return;
        }
        if (callback) callback(true, body);
    });
}

ResolvedStreamInfo OnlineMusicService::resolveViaNeteaseBlocking(const ConfigSnapshot& cfg,
                                                                 const QString& remoteId,
                                                                 const QString& quality,
                                                                 const std::atomic<bool>* abortFlag,
                                                                 int timeoutMs) {
    ResolvedStreamInfo out;
    if (cfg.neteaseUrl.isEmpty() || !cacheManager_) {
        out.error = QStringLiteral("Netease API URL not configured");
        return out;
    }

    const QString level = qualityToNeteaseLevel(quality);
    const QUrl v1Url = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/song/url/v1"), {
        {QStringLiteral("id"), remoteId},
        {QStringLiteral("level"), level}
    });
    BlockingHttpResult res = cacheManager_->runBlockingHttpGet(v1Url, cfg.cookie, timeoutMs, abortFlag, true);
    if (res.aborted || res.timedOut) {
        out.error = res.aborted ? QStringLiteral("Aborted") : QStringLiteral("Stream URL lookup timed out");
        return out;
    }

    if (res.statusCode == 404) {
        const QUrl legacyUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/song/url"), {
            {QStringLiteral("id"), remoteId},
            {QStringLiteral("br"), QString::number(qualityToBitrate(quality))}
        });
        res = cacheManager_->runBlockingHttpGet(legacyUrl, cfg.cookie, timeoutMs, abortFlag, true);
    }

    if (!res.ok) {
        out.error = res.errorString.isEmpty() ? QStringLiteral("Netease URL API failed") : res.errorString;
        return out;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(res.body);
    if (!doc.isObject()) {
        out.error = QStringLiteral("Invalid Netease URL response");
        return out;
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(200);
    if (code != 200) {
        out.error = extractJsonMessage(res.body);
        if (out.error.isEmpty()) out.error = QStringLiteral("Netease URL error %1").arg(code);
        return out;
    }

    const QJsonArray dataArr = root.value(QStringLiteral("data")).toArray();
    if (dataArr.isEmpty() || !dataArr.first().isObject()) {
        out.error = QStringLiteral("Empty song URL data");
        return out;
    }

    const QJsonObject item = dataArr.first().toObject();
    const QString urlStr = item.value(QStringLiteral("url")).toString().trimmed();
    const QString resolvedLevel = item.value(QStringLiteral("level")).toString(level);
    const QString codecType = item.value(QStringLiteral("type")).toString();
    const int brBps = item.value(QStringLiteral("br")).toInt(0);
    const QJsonValue trialVal = item.value(QStringLiteral("freeTrialInfo"));
    const bool isTrial = trialVal.isObject() && !trialVal.isNull();

    if (urlStr.isEmpty()) {
        out.error = QStringLiteral("Track stream URL unavailable (copyright or VIP required)");
        return out;
    }

    out.ok = true;
    out.streamUrl = urlStr;
    out.backendUsed = QStringLiteral("netease");
    out.qualityResolved = resolvedLevel;
    out.codecHint = codecType;
    out.bitrateKbps = brBps > 0 ? (brBps / 1000) : 0;
    out.formatBadge = formatBadgeForQuality(resolvedLevel, codecType);
    out.isTrial = isTrial;
    return out;
}

ResolvedStreamInfo OnlineMusicService::resolveViaNeteaseMatchBlocking(const ConfigSnapshot& cfg,
                                                                      const QString& remoteId,
                                                                      const QString& quality,
                                                                      const std::atomic<bool>* abortFlag,
                                                                      int timeoutMs) {
    ResolvedStreamInfo out;
    if (cfg.neteaseUrl.isEmpty() || !cfg.unblockTrialFallback || !cacheManager_) {
        out.error = QStringLiteral("Trial unblock fallback disabled or Netease API unconfigured");
        return out;
    }

    auto tryMatchUrl = [&](const QUrl& url, int budgetMs, bool* routeNotFoundOut) -> QString {
        if (routeNotFoundOut) *routeNotFoundOut = false;
        const BlockingHttpResult res = cacheManager_->runBlockingHttpGet(url, cfg.cookie, budgetMs, abortFlag, true);
        if (res.statusCode == 404 && routeNotFoundOut) {
            *routeNotFoundOut = true;
        }
        if (res.aborted || res.timedOut || !res.ok) {
            return {};
        }
        const QJsonDocument doc = QJsonDocument::fromJson(res.body);
        if (!doc.isObject()) return {};
        const QJsonObject root = doc.object();
        const int code = root.value(QStringLiteral("code")).toInt(200);
        if (code != 200) return {};

        auto acceptHttp = [](QString candidate) -> QString {
            candidate = candidate.trimmed();
            if (candidate.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                || candidate.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
                return candidate;
            }
            return {};
        };

        const QString proxyUrl = acceptHttp(root.value(QStringLiteral("proxyUrl")).toString());
        if (!proxyUrl.isEmpty()) return proxyUrl;

        const QJsonValue dataVal = root.value(QStringLiteral("data"));
        if (dataVal.isString()) {
            const QString u = acceptHttp(dataVal.toString());
            if (!u.isEmpty()) return u;
        } else if (dataVal.isObject()) {
            const QString u = acceptHttp(dataVal.toObject().value(QStringLiteral("url")).toString());
            if (!u.isEmpty()) return u;
        } else if (dataVal.isArray() && !dataVal.toArray().isEmpty()) {
            const QJsonValue first = dataVal.toArray().first();
            if (first.isString()) {
                const QString u = acceptHttp(first.toString());
                if (!u.isEmpty()) return u;
            } else if (first.isObject()) {
                const QString u = acceptHttp(first.toObject().value(QStringLiteral("url")).toString());
                if (!u.isEmpty()) return u;
            }
        }
        return {};
    };

    bool routeNotFound = false;
    const QUrl fastUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/song/url/match"), {
        {QStringLiteral("id"), remoteId},
        {QStringLiteral("source"), QStringLiteral("unm")}
    });
    QString resolvedUrl = tryMatchUrl(fastUrl, std::min(timeoutMs, 6500), &routeNotFound);

    if (resolvedUrl.isEmpty() && !routeNotFound && !(abortFlag && abortFlag->load(std::memory_order_acquire))) {
        const QUrl generalUrl = buildNeteaseUrl(cfg.neteaseUrl, QStringLiteral("/song/url/match"), {
            {QStringLiteral("id"), remoteId}
        });
        resolvedUrl = tryMatchUrl(generalUrl, std::min(timeoutMs, 8000), &routeNotFound);
    }

    if (resolvedUrl.isEmpty()) {
        out.error = QStringLiteral("No unblocked full stream found via /song/url/match");
        return out;
    }

    out.ok = true;
    out.streamUrl = resolvedUrl;
    out.backendUsed = QStringLiteral("netease-match");
    out.isTrial = false;
    out.qualityResolved = quality;
    out.codecHint = StreamCacheManager::inferExtension({}, QUrl(resolvedUrl));
    out.formatBadge = formatBadgeForQuality(quality, out.codecHint);
    return out;
}

ResolvedStreamInfo OnlineMusicService::resolveViaMetingBlocking(const ConfigSnapshot& cfg,
                                                                const QString& remoteId,
                                                                const QString& quality,
                                                                const std::atomic<bool>* abortFlag,
                                                                int timeoutMs) {
    ResolvedStreamInfo out;
    if (cfg.metingUrl.isEmpty() || !cacheManager_) {
        out.error = QStringLiteral("Meting API URL not configured");
        return out;
    }

    const int br = qualityToBitrate(quality);
    const QUrl url = buildMetingUrl(cfg.metingUrl, QStringLiteral("netease"), QStringLiteral("url"), remoteId, br);
    const BlockingHttpResult res = cacheManager_->runBlockingHttpGet(url, cfg.cookie, timeoutMs, abortFlag, false);
    if (res.aborted || res.timedOut) {
        out.error = res.aborted ? QStringLiteral("Aborted") : QStringLiteral("Meting URL lookup timed out");
        return out;
    }

    QString resolvedUrl;
    if (res.statusCode >= 300 && res.statusCode < 400 && res.redirectLocation.isValid()) {
        resolvedUrl = res.redirectLocation.toString();
    } else if (res.ok) {
        const QByteArray trimmed = res.body.trimmed();
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed);
        if (doc.isObject()) {
            resolvedUrl = doc.object().value(QStringLiteral("url")).toString().trimmed();
        } else if (doc.isArray() && !doc.array().isEmpty() && doc.array().first().isObject()) {
            resolvedUrl = doc.array().first().toObject().value(QStringLiteral("url")).toString().trimmed();
        } else if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
            resolvedUrl = QString::fromUtf8(trimmed);
        }
    }

    if (resolvedUrl.isEmpty()) {
        out.error = res.errorString.isEmpty()
            ? QStringLiteral("Meting returned no playable stream URL")
            : res.errorString;
        return out;
    }

    out.ok = true;
    out.streamUrl = resolvedUrl;
    out.backendUsed = QStringLiteral("meting");
    out.qualityResolved = quality;
    out.codecHint = StreamCacheManager::inferExtension({}, QUrl(resolvedUrl));
    out.formatBadge = formatBadgeForQuality(quality, out.codecHint);
    return out;
}

ResolvedStreamInfo OnlineMusicService::resolveStreamBlocking(const TrackInfo& track,
                                                             const QString& qualityOverride,
                                                             const std::atomic<bool>* abortFlag,
                                                             int timeoutMs) {
    const ConfigSnapshot cfg = configSnapshot();
    const QString quality = qualityOverride.isEmpty() ? cfg.quality : qualityOverride;

    if (track.remoteId.isEmpty()) {
        ResolvedStreamInfo err;
        err.error = QStringLiteral("Missing remote track ID");
        return err;
    }

    const bool metingFirst = (cfg.priority == QStringLiteral("meting") && !cfg.metingUrl.isEmpty());
    ResolvedStreamInfo primary;

    if (metingFirst) {
        primary = resolveViaMetingBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
        if (primary.ok) return primary;
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) return primary;
        if (cfg.autoFallback && !cfg.neteaseUrl.isEmpty()) {
            auto fb = resolveViaNeteaseBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
            if (fb.ok && !fb.isTrial) return fb;
            if (!(abortFlag && abortFlag->load(std::memory_order_acquire)) && cfg.unblockTrialFallback) {
                auto matchRes = resolveViaNeteaseMatchBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
                if (matchRes.ok) return matchRes;
            }
            if (fb.ok) return fb;
        }
        return primary;
    }

    // Netease first (or Meting-only if Netease URL empty)
    if (!cfg.neteaseUrl.isEmpty()) {
        primary = resolveViaNeteaseBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
        if (primary.ok && !primary.isTrial) {
            return primary;
        }
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) {
            return primary;
        }
        if (cfg.autoFallback) {
            if (cfg.unblockTrialFallback) {
                auto matchRes = resolveViaNeteaseMatchBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
                if (matchRes.ok) return matchRes;
            }
            if (!(abortFlag && abortFlag->load(std::memory_order_acquire)) && !cfg.metingUrl.isEmpty()) {
                auto metingRes = resolveViaMetingBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
                if (metingRes.ok) return metingRes;
            }
        }
        if (primary.ok) {
            return primary;
        }
        // Quality downgrade fallback on Netease if requested lossless/hires failed
        if (cfg.autoFallback && quality != QStringLiteral("standard")) {
            static const QStringList kLevels = {
                QStringLiteral("lossless"), QStringLiteral("exhigh"), QStringLiteral("standard")
            };
            const int startIdx = kLevels.indexOf(quality) + 1;
            for (int i = std::max(0, startIdx); i < kLevels.size(); ++i) {
                if (abortFlag && abortFlag->load(std::memory_order_acquire)) break;
                auto down = resolveViaNeteaseBlocking(cfg, track.remoteId, kLevels[i], abortFlag, timeoutMs);
                if (down.ok) return down;
            }
        }
        return primary;
    }

    if (!cfg.metingUrl.isEmpty()) {
        return resolveViaMetingBlocking(cfg, track.remoteId, quality, abortFlag, timeoutMs);
    }

    ResolvedStreamInfo err;
    err.error = tr("No online music backend configured in Settings.");
    return err;
}

} // namespace virela
