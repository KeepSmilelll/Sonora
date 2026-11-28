#include "online/OnlineMusicController.h"
#include "core/AppSettings.h"
#include "core/PlaybackController.h"
#include "library/CoverArtProvider.h"
#include "lyrics/LyricsController.h"
#include "lyrics/LyricsParser.h"
#include "models/LibraryModels.h"
#include "online/AudioMetadataWriter.h"
#include "online/LocalBackendManager.h"
#include "online/StreamCacheManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <algorithm>

namespace virela {

namespace {

QString sanitizeFilenamePart(QString text) {
    text = text.trimmed();
    static const QRegularExpression badChars(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1F]"));
    text.replace(badChars, QStringLiteral("_"));
    while (text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char(' '))) {
        text.chop(1);
    }
    return text.left(120);
}

QString buildDownloadDestinationPath(const QString& dirPath, const TrackInfo& meta, const QString& ext) {
    QDir dir(dirPath);
    QString artist = sanitizeFilenamePart(meta.artist);
    QString title = sanitizeFilenamePart(meta.title);
    if (title.isEmpty()) title = sanitizeFilenamePart(meta.remoteId);
    if (title.isEmpty()) title = QStringLiteral("Track");

    const QString stem = artist.isEmpty() ? title : (artist + QStringLiteral(" - ") + title);
    const QString cleanExt = ext.isEmpty() ? QStringLiteral("mp3") : ext;
    return dir.filePath(stem + QLatin1Char('.') + cleanExt);
}

} // namespace

// ============================================================================
// Models
// ============================================================================

int OnlineTrackListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(tracks_.size());
}

QVariant OnlineTrackListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= tracks_.size()) return {};
    const TrackInfo& t = tracks_[index.row()];
    const QString key = t.trackKey();
    switch (role) {
    case TitleRole:          return t.displayTitle();
    case ArtistRole:         return t.artist;
    case AlbumRole:          return t.album;
    case DurationMsRole:     return QVariant::fromValue(t.durationMs);
    case FormatBadgeRole:    return t.formatBadge;
    case CoverUrlRole:       return OnlineMusicService::sizedCoverUrl(t.coverUrl, 120);
    case SourceRole:         return t.source;
    case RemoteIdRole:       return t.remoteId;
    case RemoteAlbumIdRole:  return t.remoteAlbumId;
    case RemoteArtistIdRole: return t.remoteArtistId;
    case TrackKeyRole:       return key;
    case DownloadStatusRole: return statusMap_.value(key, QStringLiteral("none"));
    default:                 return {};
    }
}

QHash<int, QByteArray> OnlineTrackListModel::roleNames() const {
    return {
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {DurationMsRole, "durationMs"},
        {FormatBadgeRole, "formatBadge"},
        {CoverUrlRole, "coverUrl"},
        {SourceRole, "source"},
        {RemoteIdRole, "remoteId"},
        {RemoteAlbumIdRole, "remoteAlbumId"},
        {RemoteArtistIdRole, "remoteArtistId"},
        {TrackKeyRole, "trackKey"},
        {DownloadStatusRole, "downloadStatus"}
    };
}

void OnlineTrackListModel::setTracks(QVector<TrackInfo> tracks) {
    beginResetModel();
    tracks_ = std::move(tracks);
    endResetModel();
    emit countChanged();
}

void OnlineTrackListModel::setDownloadStatus(const QString& trackKey, const QString& status) {
    if (trackKey.isEmpty()) return;
    statusMap_.insert(trackKey, status);
    for (int i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].trackKey() == trackKey) {
            emit dataChanged(index(i), index(i), {DownloadStatusRole});
        }
    }
}

QString OnlineTrackListModel::downloadStatus(const QString& trackKey) const {
    return statusMap_.value(trackKey, QStringLiteral("none"));
}

QVariantMap OnlineTrackListModel::trackMapAt(int row) const {
    if (row < 0 || row >= tracks_.size()) return {};
    return tracks_[row].toVariantMap();
}

int OnlineAlbumListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(albums_.size());
}

QVariant OnlineAlbumListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= albums_.size()) return {};
    const OnlineAlbumItem& a = albums_[index.row()];
    switch (role) {
    case AlbumIdRole:    return a.id;
    case NameRole:       return a.name;
    case ArtistRole:     return a.artist;
    case ArtistIdRole:   return a.artistId;
    case CoverUrlRole:   return OnlineMusicService::sizedCoverUrl(a.coverUrl, 320);
    case YearRole:       return a.year;
    case TrackCountRole: return a.trackCount;
    default:             return {};
    }
}

QHash<int, QByteArray> OnlineAlbumListModel::roleNames() const {
    return {
        {AlbumIdRole, "albumId"},
        {NameRole, "name"},
        {ArtistRole, "artist"},
        {ArtistIdRole, "artistId"},
        {CoverUrlRole, "coverUrl"},
        {YearRole, "year"},
        {TrackCountRole, "trackCount"}
    };
}

void OnlineAlbumListModel::setAlbums(QVector<OnlineAlbumItem> albums) {
    beginResetModel();
    albums_ = std::move(albums);
    endResetModel();
    emit countChanged();
}

int OnlineArtistListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(artists_.size());
}

QVariant OnlineArtistListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= artists_.size()) return {};
    const OnlineArtistItem& a = artists_[index.row()];
    switch (role) {
    case ArtistIdRole:   return a.id;
    case NameRole:       return a.name;
    case CoverUrlRole:   return OnlineMusicService::sizedCoverUrl(a.coverUrl, 320);
    case AlbumCountRole: return a.albumCount;
    case TrackCountRole: return a.trackCount;
    case BriefDescRole:  return a.briefDesc;
    default:             return {};
    }
}

QHash<int, QByteArray> OnlineArtistListModel::roleNames() const {
    return {
        {ArtistIdRole, "artistId"},
        {NameRole, "name"},
        {CoverUrlRole, "coverUrl"},
        {AlbumCountRole, "albumCount"},
        {TrackCountRole, "trackCount"},
        {BriefDescRole, "briefDesc"}
    };
}

void OnlineArtistListModel::setArtists(QVector<OnlineArtistItem> artists) {
    beginResetModel();
    artists_ = std::move(artists);
    endResetModel();
    emit countChanged();
}

int OnlinePlaylistListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(playlists_.size());
}

QVariant OnlinePlaylistListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= playlists_.size()) return {};
    const OnlinePlaylistItem& p = playlists_[index.row()];
    switch (role) {
    case PlaylistIdRole:  return p.id;
    case NameRole:        return p.name;
    case CreatorRole:     return p.creator;
    case CoverUrlRole:    return OnlineMusicService::sizedCoverUrl(p.coverUrl, 320);
    case TrackCountRole:  return p.trackCount;
    case PlayCountRole:   return QVariant::fromValue(p.playCount);
    case DescriptionRole: return p.description;
    default:              return {};
    }
}

QHash<int, QByteArray> OnlinePlaylistListModel::roleNames() const {
    return {
        {PlaylistIdRole, "playlistId"},
        {NameRole, "name"},
        {CreatorRole, "creator"},
        {CoverUrlRole, "coverUrl"},
        {TrackCountRole, "trackCount"},
        {PlayCountRole, "playCount"},
        {DescriptionRole, "description"}
    };
}

void OnlinePlaylistListModel::setPlaylists(QVector<OnlinePlaylistItem> playlists) {
    beginResetModel();
    playlists_ = std::move(playlists);
    endResetModel();
    emit countChanged();
}

// ============================================================================
// DownloadJob state
// ============================================================================

struct OnlineMusicController::DownloadJob {
    TrackInfo track;
    QString trackKey;
    QString cacheKey;
    QString srcAudioPath;
    QString ext;
    QByteArray coverBytes;
    OnlineLyricsResult lyrics;
    bool audioReady = false;
    bool coverDone = false;
    bool lyricsDone = false;
    bool finalized = false;
    std::shared_ptr<StreamCacheSession> session;
};

// ============================================================================
// OnlineMusicController
// ============================================================================

OnlineMusicController::OnlineMusicController(OnlineMusicService* service,
                                             StreamCacheManager* cacheManager,
                                             AppSettings* settings,
                                             PlaybackController* player,
                                             LibraryController* library,
                                             LyricsController* lyrics,
                                             CoverArtProvider* covers,
                                             QObject* parent)
    : QObject(parent)
    , service_(service)
    , cache_(cacheManager)
    , settings_(settings)
    , player_(player)
    , library_(library)
    , lyrics_(lyrics)
    , covers_(covers) {
    downloadPool_.setMaxThreadCount(2);

    if (settings_) {
        lastDiscoverNeteaseUrl_ = settings_->onlineNeteaseApiUrl().trimmed();
        connect(settings_, &AppSettings::onlineSettingsChanged, this, [this]() {
            const QString currentNetease = settings_->onlineNeteaseApiUrl().trimmed();
            if (currentNetease != lastDiscoverNeteaseUrl_) {
                lastDiscoverNeteaseUrl_ = currentNetease;
                discoverLoaded_ = false;
                discoverError_.clear();
                emit discoverChanged();
            }
            emit backendConfigChanged();
            refreshCacheStats();
        });
    }
    if (cache_) {
        connect(cache_, &StreamCacheManager::cacheStatsChanged, this, &OnlineMusicController::refreshCacheStats);
        connect(cache_, &StreamCacheManager::sessionCompleted, this, [this](const QString& key, const QString& finalPath) {
            auto it = activeDownloadsByCacheKey_.find(key);
            if (it == activeDownloadsByCacheKey_.end()) return;
            auto job = it.value();
            if (!job || job->audioReady) return;
            job->srcAudioPath = finalPath;
            job->ext = QFileInfo(finalPath).suffix().toLower();
            job->audioReady = true;
            tryFinalizeDownloadJob(job);
        });
        connect(cache_, &StreamCacheManager::sessionFailed, this, [this](const QString& key, const QString& err) {
            auto it = activeDownloadsByCacheKey_.find(key);
            if (it == activeDownloadsByCacheKey_.end()) return;
            auto job = it.value();
            activeDownloadsByCacheKey_.erase(it);
            if (!job || job->finalized) return;
            job->finalized = true;
            setTrackDownloadStatus(job->trackKey, QStringLiteral("error"));
            upsertDownloadTask(job->track, QStringLiteral("error"), err);
            pendingDownloadKeys_.remove(job->trackKey);
            activeDownloadCount_ = std::max(0, activeDownloadCount_ - 1);
            emit downloadsChanged();
            if (player_) {
                player_->showToast(tr("Download failed: %1").arg(err));
            }
            pumpDownloadQueue();
        });
    }

    refreshCacheStats();
}

bool OnlineMusicController::isBackendConfigured() const {
    if (!settings_) return false;
    return !settings_->onlineNeteaseApiUrl().trimmed().isEmpty()
        || !settings_->onlineMetingApiUrl().trimmed().isEmpty();
}

bool OnlineMusicController::isNeteaseConfigured() const {
    return settings_ && !settings_->onlineNeteaseApiUrl().trimmed().isEmpty();
}

void OnlineMusicController::ensureDiscoverLoaded() {
    if (discoverLoading_ || discoverLoaded_) return;
    if (!isNeteaseConfigured()) {
        discoverError_.clear();
        emit discoverChanged();
        return;
    }
    refreshDiscover();
}

void OnlineMusicController::refreshDiscover() {
    const quint64 seq = ++discoverSeq_;
    if (!service_ || !isNeteaseConfigured()) {
        discoverLoading_ = false;
        discoverLoaded_ = false;
        discoverError_.clear();
        hotSearches_.clear();
        recommendedPlaylistsModel_.setPlaylists({});
        newSongsModel_.setTracks({});
        emit discoverChanged();
        return;
    }

    lastDiscoverNeteaseUrl_ = settings_->onlineNeteaseApiUrl().trimmed();
    discoverLoading_ = true;
    discoverError_.clear();
    emit discoverChanged();

    struct DiscoverState {
        int remaining = 3;
        int successes = 0;
        QString firstError;
        QVariantList hotList;
        QVector<OnlinePlaylistItem> playlists;
        QVector<TrackInfo> newSongs;
    };
    auto state = std::make_shared<DiscoverState>();

    auto finalizeIfDone = [this, seq, state]() {
        if (--state->remaining > 0) return;
        if (seq != discoverSeq_) return;

        if (!state->hotList.isEmpty()) {
            hotSearches_ = std::move(state->hotList);
        }
        if (!state->playlists.isEmpty()) {
            recommendedPlaylistsModel_.setPlaylists(state->playlists);
        }
        if (!state->newSongs.isEmpty()) {
            newSongsModel_.setTracks(state->newSongs);
            syncModelDownloadStatuses(newSongsModel_, state->newSongs);
        }

        discoverLoading_ = false;
        discoverLoaded_ = (state->successes > 0);
        if (state->successes == 0) {
            discoverError_ = !state->firstError.isEmpty()
                ? state->firstError
                : tr("Unable to load online discovery data.");
        } else {
            discoverError_.clear();
        }
        emit discoverChanged();
    };

    service_->fetchHotSearch(18, [state, finalizeIfDone](bool ok, QVector<OnlineHotSearchItem> items, QString err) {
        if (ok && !items.isEmpty()) {
            ++state->successes;
            QVariantList list;
            list.reserve(items.size());
            for (const auto& item : items) {
                list.append(QVariantMap{
                    {QStringLiteral("word"), item.searchWord},
                    {QStringLiteral("content"), item.content},
                    {QStringLiteral("iconUrl"), item.iconUrl},
                    {QStringLiteral("score"), item.score},
                    {QStringLiteral("iconType"), item.iconType}
                });
            }
            state->hotList = std::move(list);
        } else if (!err.isEmpty() && state->firstError.isEmpty()) {
            state->firstError = err;
        }
        finalizeIfDone();
    });

    service_->fetchRecommendedPlaylists(12, [state, finalizeIfDone](bool ok, QVector<OnlinePlaylistItem> playlists, QString err) {
        if (ok && !playlists.isEmpty()) {
            ++state->successes;
            state->playlists = std::move(playlists);
        } else if (!err.isEmpty() && state->firstError.isEmpty()) {
            state->firstError = err;
        }
        finalizeIfDone();
    });

    service_->fetchNewSongs(16, [state, finalizeIfDone](bool ok, QVector<TrackInfo> tracks, QString err) {
        if (ok && !tracks.isEmpty()) {
            ++state->successes;
            state->newSongs = std::move(tracks);
        } else if (!err.isEmpty() && state->firstError.isEmpty()) {
            state->firstError = err;
        }
        finalizeIfDone();
    });
}

void OnlineMusicController::setSearchQuery(const QString& q) {
    if (searchQuery_ == q) return;
    searchQuery_ = q;
    emit searchStateChanged();
}

void OnlineMusicController::setSearchType(const QString& t) {
    QString valid = t;
    if (valid != QStringLiteral("songs") && valid != QStringLiteral("albums")
        && valid != QStringLiteral("artists") && valid != QStringLiteral("playlists")) {
        valid = QStringLiteral("songs");
    }
    if (searchType_ == valid) return;
    searchType_ = valid;
    emit searchStateChanged();
}

void OnlineMusicController::setBusy(bool b) {
    if (busy_ == b) return;
    busy_ = b;
    emit busyChanged();
}

void OnlineMusicController::setError(const QString& err) {
    if (errorMessage_ == err) return;
    errorMessage_ = err;
    emit errorChanged();
}

void OnlineMusicController::search(const QString& query, const QString& type) {
    if (!service_) return;
    const QString q = query.trimmed();

    if (q.contains(QStringLiteral("playlist"), Qt::CaseInsensitive)) {
        const QString playlistId = OnlineMusicService::extractPlaylistId(q);
        if (!playlistId.isEmpty()) {
            openPlaylist(playlistId);
            return;
        }
    }

    closeDetail();
    const quint64 seq = ++searchSeq_;
    setSearchQuery(q);
    if (!type.isEmpty()) setSearchType(type);
    setError({});

    if (q.isEmpty()) {
        songsModel_.setTracks({});
        albumsModel_.setAlbums({});
        artistsModel_.setArtists({});
        playlistsModel_.setPlaylists({});
        setBusy(false);
        return;
    }

    setBusy(true);

    if (searchType_ == QStringLiteral("albums")) {
        songsModel_.setTracks({});
        artistsModel_.setArtists({});
        playlistsModel_.setPlaylists({});
        albumsModel_.setAlbums({});
        service_->searchAlbums(q, 40, 0, [this, seq](bool ok, QVector<OnlineAlbumItem> albums, QString err) {
            if (seq != searchSeq_) return;
            setBusy(false);
            if (!ok) {
                setError(err);
                return;
            }
            albumsModel_.setAlbums(std::move(albums));
        });
    } else if (searchType_ == QStringLiteral("artists")) {
        songsModel_.setTracks({});
        albumsModel_.setAlbums({});
        playlistsModel_.setPlaylists({});
        artistsModel_.setArtists({});
        service_->searchArtists(q, 40, 0, [this, seq](bool ok, QVector<OnlineArtistItem> artists, QString err) {
            if (seq != searchSeq_) return;
            setBusy(false);
            if (!ok) {
                setError(err);
                return;
            }
            artistsModel_.setArtists(std::move(artists));
        });
    } else if (searchType_ == QStringLiteral("playlists")) {
        songsModel_.setTracks({});
        albumsModel_.setAlbums({});
        artistsModel_.setArtists({});
        playlistsModel_.setPlaylists({});
        service_->searchPlaylists(q, 40, 0, [this, seq](bool ok, QVector<OnlinePlaylistItem> playlists, QString err) {
            if (seq != searchSeq_) return;
            setBusy(false);
            if (!ok) {
                setError(err);
                return;
            }
            playlistsModel_.setPlaylists(std::move(playlists));
        });
    } else {
        albumsModel_.setAlbums({});
        artistsModel_.setArtists({});
        playlistsModel_.setPlaylists({});
        songsModel_.setTracks({});
        service_->searchSongs(q, 50, 0, [this, seq](bool ok, QVector<TrackInfo> tracks, QString err) {
            if (seq != searchSeq_) return;
            setBusy(false);
            if (!ok) {
                setError(err);
                return;
            }
            songsModel_.setTracks(tracks);
            syncModelDownloadStatuses(songsModel_, tracks);
        });
    }
}

void OnlineMusicController::parseAndOpenPlaylist(const QString& playlistIdOrUrl) {
    const QString pid = OnlineMusicService::extractPlaylistId(playlistIdOrUrl);
    if (pid.isEmpty()) {
        setError(tr("Could not recognize a Netease playlist ID or URL."));
        if (player_) player_->showToast(errorMessage_);
        return;
    }
    openPlaylist(pid);
}

QString OnlineMusicController::detectPlaylistLinkId(const QString& input) const {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return {};

    static const QRegularExpression kPureDigits(QStringLiteral("^\\d+$"));
    if (kPureDigits.match(trimmed).hasMatch()) {
        return trimmed.size() >= 6 ? trimmed : QString();
    }

    const bool hasHint = trimmed.contains(QStringLiteral("music.163.com"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("playlist"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("http://"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("https://"), Qt::CaseInsensitive);
    if (!hasHint) {
        return {};
    }
    return OnlineMusicService::extractPlaylistId(trimmed);
}

void OnlineMusicController::openAlbum(const QString& albumId,
                                      const QString& title,
                                      const QString& artist,
                                      const QString& coverUrl) {
    if (!service_) return;
    const quint64 seq = ++detailSeq_;
    inDetail_ = true;
    detailType_ = QStringLiteral("album");
    detailRemoteId_ = albumId;
    detailTitle_ = title;
    detailSubtitle_ = artist;
    detailCoverUrl_ = coverUrl;
    detailDescription_.clear();
    detailTracksModel_.setTracks({});
    emit detailChanged();

    setBusy(true);
    setError({});
    service_->fetchAlbumDetail(albumId, [this, seq](OnlineAlbumDetail detail) {
        if (seq != detailSeq_) return;
        setBusy(false);
        if (!detail.ok) {
            setError(detail.error);
            return;
        }
        if (!detail.info.name.isEmpty()) detailTitle_ = detail.info.name;
        if (!detail.info.artist.isEmpty()) {
            detailSubtitle_ = detail.info.year > 0
                ? QStringLiteral("%1 · %2").arg(detail.info.artist).arg(detail.info.year)
                : detail.info.artist;
        }
        if (!detail.info.coverUrl.isEmpty()) detailCoverUrl_ = detail.info.coverUrl;
        detailDescription_ = detail.info.description;
        detailTracksModel_.setTracks(detail.tracks);
        syncModelDownloadStatuses(detailTracksModel_, detail.tracks);
        emit detailChanged();
    });
}

void OnlineMusicController::openArtist(const QString& artistId,
                                       const QString& name,
                                       const QString& coverUrl) {
    if (!service_) return;
    const quint64 seq = ++detailSeq_;
    inDetail_ = true;
    detailType_ = QStringLiteral("artist");
    detailRemoteId_ = artistId;
    detailTitle_ = name;
    detailSubtitle_ = tr("Top Tracks");
    detailCoverUrl_ = coverUrl;
    detailDescription_.clear();
    detailTracksModel_.setTracks({});
    emit detailChanged();

    setBusy(true);
    setError({});
    service_->fetchArtistDetail(artistId, [this, seq](OnlineArtistDetail detail) {
        if (seq != detailSeq_) return;
        setBusy(false);
        if (!detail.ok) {
            setError(detail.error);
            return;
        }
        if (!detail.info.name.isEmpty()) detailTitle_ = detail.info.name;
        if (!detail.info.coverUrl.isEmpty()) detailCoverUrl_ = detail.info.coverUrl;
        detailDescription_ = detail.info.briefDesc;
        detailSubtitle_ = tr("%1 track(s)").arg(detail.tracks.size());
        detailTracksModel_.setTracks(detail.tracks);
        syncModelDownloadStatuses(detailTracksModel_, detail.tracks);
        emit detailChanged();
    });
}

void OnlineMusicController::openPlaylist(const QString& playlistId,
                                         const QString& title,
                                         const QString& creator,
                                         const QString& coverUrl,
                                         const QString& description) {
    if (!service_) return;
    ++searchSeq_;
    const quint64 seq = ++detailSeq_;
    inDetail_ = true;
    detailType_ = QStringLiteral("playlist");
    detailRemoteId_ = playlistId;
    detailTitle_ = title.isEmpty() ? tr("Playlist #%1").arg(playlistId) : title;
    detailSubtitle_ = creator;
    detailCoverUrl_ = coverUrl;
    detailDescription_ = description;
    detailTracksModel_.setTracks({});
    emit detailChanged();

    setBusy(true);
    setError({});
    service_->fetchPlaylistDetail(playlistId, [this, seq](OnlinePlaylistDetail detail) {
        if (seq != detailSeq_) return;
        setBusy(false);
        if (!detail.ok) {
            setError(detail.error);
            return;
        }
        if (!detail.info.name.isEmpty()) detailTitle_ = detail.info.name;
        if (!detail.info.creator.isEmpty()) {
            detailSubtitle_ = tr("by %1 · %2 track(s)").arg(detail.info.creator).arg(detail.tracks.size());
        } else {
            detailSubtitle_ = tr("%1 track(s)").arg(detail.tracks.size());
        }
        if (!detail.info.coverUrl.isEmpty()) detailCoverUrl_ = detail.info.coverUrl;
        if (!detail.info.description.isEmpty()) detailDescription_ = detail.info.description;
        detailTracksModel_.setTracks(detail.tracks);
        syncModelDownloadStatuses(detailTracksModel_, detail.tracks);
        emit detailChanged();
    });
}

void OnlineMusicController::closeDetail() {
    if (!inDetail_) return;
    ++detailSeq_;
    inDetail_ = false;
    detailType_.clear();
    detailRemoteId_.clear();
    detailTitle_.clear();
    detailSubtitle_.clear();
    detailCoverUrl_.clear();
    detailDescription_.clear();
    detailTracksModel_.setTracks({});
    setBusy(false);
    setError({});
    emit detailChanged();
}

TrackInfo OnlineMusicController::resolvePlaybackTrack(const TrackInfo& track) const {
    if (track.isOnline() && library_) {
        TrackInfo upgraded;
        if (library_->findUpgradedLocalTrack(track.remoteId, upgraded)) {
            return upgraded;
        }
    }
    return track;
}

QVector<TrackInfo> OnlineMusicController::resolvePlaybackTracks(const QVector<TrackInfo>& tracks) const {
    if (!library_) return tracks;
    QVector<TrackInfo> resolved;
    resolved.reserve(tracks.size());
    for (const TrackInfo& t : tracks) {
        resolved.push_back(resolvePlaybackTrack(t));
    }
    return resolved;
}

void OnlineMusicController::syncModelDownloadStatuses(OnlineTrackListModel& model, const QVector<TrackInfo>& tracks) {
    for (const TrackInfo& t : tracks) {
        const QString key = t.trackKey();
        QString status = trackDownloadStatus_.value(key);
        if (status.isEmpty() && library_ && t.isOnline()) {
            TrackInfo dummy;
            if (library_->findUpgradedLocalTrack(t.remoteId, dummy)) {
                status = QStringLiteral("done");
                trackDownloadStatus_.insert(key, status);
            }
        }
        if (!status.isEmpty()) {
            model.setDownloadStatus(key, status);
        }
    }
}

void OnlineMusicController::playSongAt(int index) {
    if (!player_ || index < 0 || index >= songsModel_.tracks().size()) return;
    player_->playTracks(resolvePlaybackTracks(songsModel_.tracks()), index);
}

void OnlineMusicController::playNextSongAt(int index) {
    if (!player_ || index < 0 || index >= songsModel_.tracks().size()) return;
    player_->playNextTrack(resolvePlaybackTrack(songsModel_.tracks()[index]));
    player_->showToast(tr("Added to Play Next"));
}

void OnlineMusicController::enqueueSongAt(int index) {
    if (!player_ || index < 0 || index >= songsModel_.tracks().size()) return;
    player_->enqueueTracks({resolvePlaybackTrack(songsModel_.tracks()[index])});
    player_->showToast(tr("Added to queue"));
}

void OnlineMusicController::playDetailAt(int index) {
    if (!player_ || index < 0 || index >= detailTracksModel_.tracks().size()) return;
    player_->playTracks(resolvePlaybackTracks(detailTracksModel_.tracks()), index);
}

void OnlineMusicController::playAllDetail() {
    playDetailAt(0);
}

void OnlineMusicController::playNextDetailAt(int index) {
    if (!player_ || index < 0 || index >= detailTracksModel_.tracks().size()) return;
    player_->playNextTrack(resolvePlaybackTrack(detailTracksModel_.tracks()[index]));
    player_->showToast(tr("Added to Play Next"));
}

void OnlineMusicController::enqueueDetailAt(int index) {
    if (!player_ || index < 0 || index >= detailTracksModel_.tracks().size()) return;
    player_->enqueueTracks({resolvePlaybackTrack(detailTracksModel_.tracks()[index])});
    player_->showToast(tr("Added to queue"));
}

void OnlineMusicController::enqueueAllDetail() {
    if (!player_ || detailTracksModel_.tracks().isEmpty()) return;
    player_->enqueueTracks(resolvePlaybackTracks(detailTracksModel_.tracks()));
    player_->showToast(tr("Added %1 track(s) to queue").arg(detailTracksModel_.tracks().size()));
}

void OnlineMusicController::playNewSongAt(int index) {
    if (!player_ || index < 0 || index >= newSongsModel_.tracks().size()) return;
    player_->playTracks(resolvePlaybackTracks(newSongsModel_.tracks()), index);
}

void OnlineMusicController::playNextNewSongAt(int index) {
    if (!player_ || index < 0 || index >= newSongsModel_.tracks().size()) return;
    player_->playNextTrack(resolvePlaybackTrack(newSongsModel_.tracks()[index]));
    player_->showToast(tr("Added to Play Next"));
}

void OnlineMusicController::enqueueNewSongAt(int index) {
    if (!player_ || index < 0 || index >= newSongsModel_.tracks().size()) return;
    player_->enqueueTracks({resolvePlaybackTrack(newSongsModel_.tracks()[index])});
    player_->showToast(tr("Added to queue"));
}

QString OnlineMusicController::currentTrackDownloadStatus() const {
    if (activeTrackKey_.isEmpty()) return QStringLiteral("none");
    return trackDownloadStatus_.value(activeTrackKey_, QStringLiteral("none"));
}

QVariantList OnlineMusicController::downloadTasks() const {
    QVariantList list;
    list.reserve(downloadTasks_.size());
    for (const auto& rec : downloadTasks_) {
        list.append(QVariantMap{
            {QStringLiteral("trackKey"), rec.trackKey},
            {QStringLiteral("title"), rec.track.displayTitle()},
            {QStringLiteral("artist"), rec.track.artist},
            {QStringLiteral("album"), rec.track.album},
            {QStringLiteral("coverUrl"), OnlineMusicService::sizedCoverUrl(rec.track.coverUrl, 120)},
            {QStringLiteral("formatBadge"), rec.track.formatBadge},
            {QStringLiteral("durationMs"), QVariant::fromValue(rec.track.durationMs)},
            {QStringLiteral("status"), rec.status},
            {QStringLiteral("error"), rec.error},
            {QStringLiteral("savedFilePath"), rec.savedFilePath},
            {QStringLiteral("trackMap"), rec.track.toVariantMap()}
        });
    }
    return list;
}

void OnlineMusicController::upsertDownloadTask(const TrackInfo& track,
                                               const QString& status,
                                               const QString& error,
                                               const QString& savedPath) {
    const QString key = track.trackKey();
    if (key.isEmpty()) return;
    for (int i = 0; i < downloadTasks_.size(); ++i) {
        if (downloadTasks_[i].trackKey == key) {
            downloadTasks_[i].track = track;
            downloadTasks_[i].status = status;
            downloadTasks_[i].error = error;
            if (!savedPath.isEmpty()) {
                downloadTasks_[i].savedFilePath = savedPath;
            } else if (status == QStringLiteral("queued") || status == QStringLiteral("downloading")) {
                downloadTasks_[i].savedFilePath.clear();
            }
            emit downloadsChanged();
            return;
        }
    }
    DownloadTaskRecord rec;
    rec.track = track;
    rec.trackKey = key;
    rec.status = status;
    rec.error = error;
    rec.savedFilePath = savedPath;
    downloadTasks_.prepend(std::move(rec));
    if (downloadTasks_.size() > 100) {
        downloadTasks_.resize(100);
    }
    emit downloadsChanged();
}

void OnlineMusicController::setTrackDownloadStatus(const QString& trackKey, const QString& status) {
    if (trackKey.isEmpty()) return;
    trackDownloadStatus_.insert(trackKey, status);
    songsModel_.setDownloadStatus(trackKey, status);
    detailTracksModel_.setDownloadStatus(trackKey, status);
    newSongsModel_.setDownloadStatus(trackKey, status);
    for (auto& rec : downloadTasks_) {
        if (rec.trackKey == trackKey) {
            rec.status = status;
            break;
        }
    }
    emit downloadsChanged();
}

void OnlineMusicController::downloadSongAt(int index) {
    if (index < 0 || index >= songsModel_.tracks().size()) return;
    enqueueDownloadTrack(songsModel_.tracks()[index]);
}

void OnlineMusicController::downloadDetailAt(int index) {
    if (index < 0 || index >= detailTracksModel_.tracks().size()) return;
    enqueueDownloadTrack(detailTracksModel_.tracks()[index]);
}

void OnlineMusicController::downloadNewSongAt(int index) {
    if (index < 0 || index >= newSongsModel_.tracks().size()) return;
    enqueueDownloadTrack(newSongsModel_.tracks()[index]);
}

void OnlineMusicController::downloadAllDetail() {
    const auto& tracks = detailTracksModel_.tracks();
    if (tracks.isEmpty()) return;
    int queued = 0;
    for (const TrackInfo& t : tracks) {
        if (t.isOnline() && !pendingDownloadKeys_.contains(t.trackKey())) {
            enqueueDownloadTrack(t);
            ++queued;
        }
    }
    if (player_ && queued > 0) {
        player_->showToast(tr("Queued %1 track(s) for download").arg(queued));
    }
}

void OnlineMusicController::downloadCurrentTrack() {
    if (!player_) return;
    const TrackInfo cur = player_->currentTrackInfo();
    if (!cur.isOnline()) {
        player_->showToast(tr("Current track is already a local file."));
        return;
    }
    enqueueDownloadTrack(cur);
}

void OnlineMusicController::enqueueDownloadTrack(const TrackInfo& track) {
    if (!track.isOnline()) return;
    const QString key = track.trackKey();
    if (pendingDownloadKeys_.contains(key)) {
        return;
    }
    pendingDownloadKeys_.insert(key);
    setTrackDownloadStatus(key, QStringLiteral("queued"));
    upsertDownloadTask(track, QStringLiteral("queued"));
    downloadQueue_.push_back(track);
    pumpDownloadQueue();
}

void OnlineMusicController::retryDownload(const QString& trackKey) {
    for (const auto& rec : downloadTasks_) {
        if (rec.trackKey == trackKey && rec.status == QStringLiteral("error")) {
            enqueueDownloadTrack(rec.track);
            return;
        }
    }
}

void OnlineMusicController::clearFinishedDownloads() {
    bool removedAny = false;
    for (int i = downloadTasks_.size() - 1; i >= 0; --i) {
        const QString& st = downloadTasks_[i].status;
        if (st == QStringLiteral("done") || st == QStringLiteral("error")) {
            downloadTasks_.removeAt(i);
            removedAny = true;
        }
    }
    if (removedAny) {
        emit downloadsChanged();
    }
}

void OnlineMusicController::playDownloadTask(const QString& trackKey) {
    if (!player_) return;
    for (const auto& rec : downloadTasks_) {
        if (rec.trackKey == trackKey) {
            if (!rec.savedFilePath.isEmpty() && QFileInfo::exists(rec.savedFilePath)) {
                player_->openUrls({QUrl::fromLocalFile(rec.savedFilePath)}, false);
            } else {
                player_->playTracks({rec.track}, 0);
            }
            return;
        }
    }
}

void OnlineMusicController::pumpDownloadQueue() {
    while (activeDownloadCount_ < 2 && !downloadQueue_.isEmpty()) {
        const TrackInfo next = downloadQueue_.takeFirst();
        startDownloadJob(next);
    }
}

void OnlineMusicController::startDownloadJob(const TrackInfo& track) {
    if (!service_ || !cache_ || !settings_) {
        pendingDownloadKeys_.remove(track.trackKey());
        return;
    }

    auto job = std::make_shared<DownloadJob>();
    job->track = track;
    job->trackKey = track.trackKey();

    const QString quality = settings_->onlineAudioQuality();
    job->cacheKey = StreamCacheManager::makeCacheKey(track.source, track.remoteId, quality);

    ++activeDownloadCount_;
    emit downloadsChanged();
    setTrackDownloadStatus(job->trackKey, QStringLiteral("downloading"));
    upsertDownloadTask(track, QStringLiteral("downloading"));

    // 1. Fetch cover bytes concurrently (optional)
    if (track.coverUrl.isEmpty()) {
        job->coverDone = true;
    } else {
        const QString dlCoverUrl = OnlineMusicService::sizedCoverUrl(track.coverUrl, 800);
        service_->fetchCoverBytes(QUrl(dlCoverUrl), [this, job](bool ok, QByteArray data) {
            if (ok) job->coverBytes = std::move(data);
            job->coverDone = true;
            tryFinalizeDownloadJob(job);
        });
    }

    // 2. Fetch lyrics concurrently (optional)
    service_->fetchLyrics(track.remoteId, [this, job](OnlineLyricsResult res) {
        job->lyrics = std::move(res);
        job->lyricsDone = true;
        tryFinalizeDownloadJob(job);
    });

    // 3. Zero-traffic check: completed cache file -> active stream session -> resolve fresh URL
    const QString completedPath = cache_->findCompletedCacheFile(job->cacheKey);
    const bool looksLikeStaleTrial = !completedPath.isEmpty()
        && track.durationMs >= 90000
        && QFileInfo(completedPath).size() < 850000;
    if (!completedPath.isEmpty() && !looksLikeStaleTrial) {
        job->srcAudioPath = completedPath;
        job->ext = QFileInfo(completedPath).suffix().toLower();
        job->audioReady = true;
        tryFinalizeDownloadJob(job);
        return;
    }

    if (!looksLikeStaleTrial) {
        if (auto existingSession = cache_->activeSessionForKey(job->cacheKey)) {
            existingSession->setDownloadPinned(true);
            job->session = existingSession;
            activeDownloadsByCacheKey_.insert(job->cacheKey, job);
            if (existingSession->isComplete()) {
                const QString fp = existingSession->finalPath();
                if (!fp.isEmpty()) {
                    activeDownloadsByCacheKey_.remove(job->cacheKey);
                    job->srcAudioPath = fp;
                    job->ext = QFileInfo(fp).suffix().toLower();
                    job->audioReady = true;
                    tryFinalizeDownloadJob(job);
                }
            }
            return;
        }
    }

    // Resolve fresh direct URL on background pool and stream into pinned cache session
    auto* task = QRunnable::create([this, job, quality]() {
        const ResolvedStreamInfo info = service_->resolveStreamBlocking(job->track, quality);
        const QString cookie = service_->currentCookie();
        QMetaObject::invokeMethod(this, [this, job, info, cookie]() {
            if (job->finalized) return;
            if (!info.ok || info.streamUrl.isEmpty()) {
                job->finalized = true;
                setTrackDownloadStatus(job->trackKey, QStringLiteral("error"));
                upsertDownloadTask(job->track, QStringLiteral("error"), info.error);
                pendingDownloadKeys_.remove(job->trackKey);
                activeDownloadCount_ = std::max(0, activeDownloadCount_ - 1);
                emit downloadsChanged();
                if (player_) {
                    player_->showToast(tr("Download failed: %1").arg(info.error));
                }
                pumpDownloadQueue();
                return;
            }

            if (!info.isTrial) {
                const QString completedNow = cache_->findCompletedCacheFile(job->cacheKey);
                const bool staleNow = !completedNow.isEmpty()
                    && job->track.durationMs >= 90000
                    && QFileInfo(completedNow).size() < 850000;
                if (!completedNow.isEmpty() && !staleNow) {
                    job->srcAudioPath = completedNow;
                    job->ext = QFileInfo(completedNow).suffix().toLower();
                    job->audioReady = true;
                    tryFinalizeDownloadJob(job);
                    return;
                }
            }

            const QString sessionKey = info.isTrial ? (job->cacheKey + QStringLiteral("_trial")) : job->cacheKey;
            const QString streamCookie = (info.backendUsed == QStringLiteral("netease")) ? cookie : QString();
            if (info.isTrial) {
                job->cacheKey = sessionKey;
            }
            auto session = cache_->openOrStartSession(sessionKey, QUrl(info.streamUrl), streamCookie, info.codecHint);
            session->setDownloadPinned(true);
            job->session = session;
            activeDownloadsByCacheKey_.insert(sessionKey, job);
            if (session->isComplete()) {
                activeDownloadsByCacheKey_.remove(sessionKey);
                job->srcAudioPath = session->finalPath();
                job->ext = QFileInfo(job->srcAudioPath).suffix().toLower();
                job->audioReady = true;
                tryFinalizeDownloadJob(job);
            }
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    downloadPool_.start(task);
}

void OnlineMusicController::tryFinalizeDownloadJob(const std::shared_ptr<DownloadJob>& job) {
    if (!job || job->finalized || !job->audioReady || !job->coverDone || !job->lyricsDone) {
        return;
    }
    job->finalized = true;
    activeDownloadsByCacheKey_.remove(job->cacheKey);

    const QString dlDir = settings_ ? settings_->onlineDownloadDir() : AppSettings::defaultDownloadDir();
    const QString dstPath = buildDownloadDestinationPath(dlDir, job->track, job->ext);
    const QString primaryEmbeddedLrc = !job->lyrics.yrc.trimmed().isEmpty()
        ? job->lyrics.yrc
        : job->lyrics.lrc;

    auto* task = QRunnable::create([this, job, dstPath, primaryEmbeddedLrc]() {
        QString writeErr;
        const bool ok = AudioMetadataWriter::writeMetadataAndCover(
            job->srcAudioPath,
            dstPath,
            job->track,
            job->coverBytes,
            primaryEmbeddedLrc,
            &writeErr);

        if (ok) {
            QString sidecarExt;
            const QString sidecarBody = LyricsParser::composeSidecarLyrics(
                job->lyrics.yrc, job->lyrics.lrc, job->lyrics.tlyric, &sidecarExt);
            if (!sidecarBody.isEmpty() && !sidecarExt.isEmpty()) {
                const QFileInfo fi(dstPath);
                const QString lrcPath = fi.dir().filePath(fi.completeBaseName() + QLatin1Char('.') + sidecarExt);
                QSaveFile lf(lrcPath);
                if (lf.open(QIODevice::WriteOnly)) {
                    lf.write(sidecarBody.toUtf8());
                    lf.commit();
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, job, dstPath, ok, writeErr]() {
            pendingDownloadKeys_.remove(job->trackKey);
            activeDownloadCount_ = std::max(0, activeDownloadCount_ - 1);
            emit downloadsChanged();

            if (ok) {
                if (library_) {
                    library_->ingestDownloadedTrack(dstPath, job->track.source, job->track.remoteId);
                }
                setTrackDownloadStatus(job->trackKey, QStringLiteral("done"));
                upsertDownloadTask(job->track, QStringLiteral("done"), {}, dstPath);
                if (player_) {
                    player_->showToast(tr("Downloaded & added to library: %1").arg(job->track.displayTitle()));
                }
            } else {
                setTrackDownloadStatus(job->trackKey, QStringLiteral("error"));
                upsertDownloadTask(job->track, QStringLiteral("error"), writeErr);
                if (player_) {
                    player_->showToast(tr("Failed to save download: %1").arg(writeErr));
                }
            }
            pumpDownloadQueue();
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    downloadPool_.start(task);
}

void OnlineMusicController::testConnections() {
    if (!service_ || testingConnection_) return;
    testingConnection_ = true;
    neteaseTestStatus_ = tr("Testing...");
    metingTestStatus_ = tr("Testing...");
    emit connectionTestChanged();

    service_->testConnections([this](BackendTestResult netease, BackendTestResult meting) {
        testingConnection_ = false;
        neteaseTestOk_ = netease.ok;
        neteaseTestStatus_ = netease.message;
        metingTestOk_ = meting.ok;
        metingTestStatus_ = meting.message;
        emit connectionTestChanged();
    });
}

void OnlineMusicController::refreshCacheStats() {
    if (!cache_) return;
    const double usedMb = static_cast<double>(cache_->currentCacheBytes()) / (1024.0 * 1024.0);
    const double maxMb = static_cast<double>(cache_->maxCacheBytes()) / (1024.0 * 1024.0);
    cacheUsageText_ = QStringLiteral("%1 MB / %2 MB")
        .arg(usedMb, 0, 'f', 1)
        .arg(maxMb, 0, 'f', 0);
    emit cacheStatsChanged();
}

void OnlineMusicController::clearStreamCache() {
    if (!cache_) return;
    cache_->clearCache();
    refreshCacheStats();
    if (player_) {
        player_->showToast(tr("Online stream cache cleared"));
    }
}

void OnlineMusicController::setDownloadDirFromUrl(const QUrl& folderUrl) {
    if (!settings_) return;
    const QString local = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (!local.isEmpty()) {
        settings_->setOnlineDownloadDir(local);
    }
}

void OnlineMusicController::setLocalBackendManager(LocalBackendManager* manager) {
    if (localBackends_ == manager) return;
    if (localBackends_) {
        disconnect(localBackends_, nullptr, this, nullptr);
    }
    localBackends_ = manager;
    if (localBackends_) {
        connect(localBackends_, &LocalBackendManager::statusChanged,
                this, &OnlineMusicController::localServicesChanged);
    }
    emit localServicesChanged();
}

bool OnlineMusicController::neteaseServiceRunning() const {
    return localBackends_ && localBackends_->neteaseRunning();
}

bool OnlineMusicController::metingServiceRunning() const {
    return localBackends_ && localBackends_->metingRunning();
}

QString OnlineMusicController::localServicesStatus() const {
    return localBackends_ ? localBackends_->statusText() : tr("Local service manager unavailable");
}

void OnlineMusicController::restartLocalServices() {
    if (!localBackends_) return;
    localBackends_->setLifecycleActive(true);
    localBackends_->restartServices();
    emit localServicesChanged();
}

void OnlineMusicController::setNeteaseServiceDirFromUrl(const QUrl& folderUrl) {
    if (!settings_) return;
    const QString local = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (!local.isEmpty()) {
        settings_->setOnlineNeteaseServiceDir(local);
    }
}

QString OnlineMusicController::detailCoverUrl() const {
    return OnlineMusicService::sizedCoverUrl(detailCoverUrl_, 320);
}

QString OnlineMusicController::sizedCoverUrl(const QString& rawUrl, int sizePx) const {
    return OnlineMusicService::sizedCoverUrl(rawUrl, sizePx);
}

void OnlineMusicController::trimMemory() {
    urlToCoverHash_.clear();
    if (!inDetail_ && detailTracksModel_.count() > 0) {
        detailTracksModel_.setTracks({});
    }
    if (searchQuery_.trimmed().isEmpty()) {
        songsModel_.setTracks({});
        albumsModel_.setAlbums({});
        artistsModel_.setArtists({});
        playlistsModel_.setPlaylists({});
    }
}

void OnlineMusicController::setMetingServiceDirFromUrl(const QUrl& folderUrl) {
    if (!settings_) return;
    const QString local = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (!local.isEmpty()) {
        settings_->setOnlineMetingServiceDir(local);
    }
}

void OnlineMusicController::onCurrentTrackChanged(const TrackInfo& track) {
    const QString reqKey = track.trackKey();
    activeTrackKey_ = reqKey;
    emit downloadsChanged();

    if (!track.isOnline()) {
        if (lyrics_) {
            lyrics_->loadForTrack(track.filePath, track.embeddedLyrics);
        }
        return;
    }

    if (lyrics_) {
        lyrics_->clear();
    }

    if (service_) {
        service_->fetchLyrics(track.remoteId, [this, reqKey](OnlineLyricsResult res) {
            if (activeTrackKey_ != reqKey || !lyrics_) return;
            if (!res.ok) return;
            const QString primary = !res.yrc.trimmed().isEmpty() ? res.yrc : res.lrc;
            if (!primary.trimmed().isEmpty()) {
                lyrics_->loadRawLyrics(primary, res.tlyric, res.sourceLabel);
            }
        });

        if (!track.coverUrl.isEmpty() && covers_ && player_) {
            const QString coverUrl = OnlineMusicService::sizedCoverUrl(track.coverUrl, 500);
            const QString cachedHash = urlToCoverHash_.value(coverUrl);
            if (!cachedHash.isEmpty()) {
                const QImage img = covers_->loadCover(cachedHash, 360);
                if (!img.isNull()) {
                    player_->applyExternalCover(reqKey, cachedHash, img);
                    return;
                }
            }
            service_->fetchCoverBytes(QUrl(coverUrl), [this, reqKey, coverUrl](bool ok, QByteArray bytes) {
                if (!ok || bytes.isEmpty() || !covers_) return;
                const QString hash = covers_->storeRawImageBytes(bytes);
                if (hash.isEmpty()) return;
                if (urlToCoverHash_.size() >= 32) {
                    urlToCoverHash_.clear();
                }
                urlToCoverHash_.insert(coverUrl, hash);
                if (activeTrackKey_ != reqKey || !player_) return;
                const QImage img = covers_->loadCover(hash, 360);
                if (!img.isNull()) {
                    player_->applyExternalCover(reqKey, hash, img);
                }
            });
        }
    }
}

} // namespace virela
