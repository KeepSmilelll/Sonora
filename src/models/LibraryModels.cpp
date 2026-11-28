#include "models/LibraryModels.h"
#include "audio/FFmpegDecoder.h"
#include "core/AppSettings.h"
#include "core/PlaybackController.h"
#include "library/CoverArtProvider.h"
#include "library/CueParser.h"
#include "library/LibraryScanner.h"
#include "library/M3uPlaylistIO.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace virela {

namespace {

TrackInfo trackFromVariantMap(const QVariantMap& m) {
    TrackInfo t;
    const QVariant idVar = m.value(QStringLiteral("id"));
    if (idVar.isValid()) {
        bool ok = false;
        const qint64 parsed = idVar.toLongLong(&ok);
        if (ok) t.id = parsed;
    }
    t.filePath = m.value(QStringLiteral("filePath")).toString();
    t.isCue = m.value(QStringLiteral("isCue")).toBool();
    t.cuePath = m.value(QStringLiteral("cuePath")).toString();
    t.startMs = m.value(QStringLiteral("startMs")).toLongLong();
    t.endMs = m.value(QStringLiteral("endMs")).toLongLong();
    t.title = m.value(QStringLiteral("title")).toString();
    t.artist = m.value(QStringLiteral("artist")).toString();
    t.album = m.value(QStringLiteral("album")).toString();
    t.albumArtist = m.value(QStringLiteral("albumArtist")).toString();
    t.trackNumber = m.value(QStringLiteral("trackNumber")).toInt();
    t.discNumber = m.value(QStringLiteral("discNumber")).toInt();
    t.year = m.value(QStringLiteral("year")).toInt();
    t.genre = m.value(QStringLiteral("genre")).toString();
    t.durationMs = m.value(QStringLiteral("durationMs")).toLongLong();
    t.formatBadge = m.value(QStringLiteral("formatBadge")).toString();
    t.coverHash = m.value(QStringLiteral("coverHash")).toString();
    t.isFavorite = m.value(QStringLiteral("isFavorite")).toBool();
    const QString src = m.value(QStringLiteral("source")).toString().trimmed();
    t.source = src.isEmpty() ? QStringLiteral("local") : src;
    t.remoteId = m.value(QStringLiteral("remoteId")).toString().trimmed();
    t.remoteAlbumId = m.value(QStringLiteral("remoteAlbumId")).toString().trimmed();
    t.remoteArtistId = m.value(QStringLiteral("remoteArtistId")).toString().trimmed();
    t.coverUrl = m.value(QStringLiteral("coverUrl")).toString().trimmed();
    t.audioQuality = m.value(QStringLiteral("audioQuality")).toString().trimmed();
    return t;
}

} // namespace

// ---- TrackListModel ----

int TrackListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(tracks_.size());
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= tracks_.size()) return {};
    const TrackInfo& t = tracks_[index.row()];
    switch (role) {
    case TrackIdRole:     return QVariant::fromValue(t.id);
    case FilePathRole:    return t.filePath;
    case TitleRole:       return t.displayTitle();
    case ArtistRole:      return t.artist;
    case AlbumRole:       return t.album;
    case TrackNumberRole: return t.trackNumber;
    case DiscNumberRole:  return t.discNumber;
    case YearRole:        return t.year;
    case DurationMsRole:  return QVariant::fromValue(t.durationMs);
    case FormatBadgeRole: return t.formatBadge;
    case TechSummaryRole: return t.techSummary();
    case CoverHashRole:   return t.coverHash;
    case IsFavoriteRole:  return t.isFavorite;
    case IsCueRole:       return t.isCue;
    case SampleRateRole:  return t.sampleRate;
    case BitDepthRole:    return t.bitDepth;
    case CodecRole:       return t.codec;
    case SourceRole:      return t.source;
    case RemoteIdRole:    return t.remoteId;
    case CoverUrlRole:    return t.coverUrl;
    case IsOnlineRole:    return t.isOnline();
    case TrackKeyRole:    return t.trackKey();
    default:              return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const {
    return {
        {TrackIdRole, "trackId"},
        {FilePathRole, "filePath"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {TrackNumberRole, "trackNumber"},
        {DiscNumberRole, "discNumber"},
        {YearRole, "year"},
        {DurationMsRole, "durationMs"},
        {FormatBadgeRole, "formatBadge"},
        {TechSummaryRole, "techSummary"},
        {CoverHashRole, "coverHash"},
        {IsFavoriteRole, "isFavorite"},
        {IsCueRole, "isCue"},
        {SampleRateRole, "sampleRate"},
        {BitDepthRole, "bitDepth"},
        {CodecRole, "codec"},
        {SourceRole, "source"},
        {RemoteIdRole, "remoteId"},
        {CoverUrlRole, "coverUrl"},
        {IsOnlineRole, "isOnline"},
        {TrackKeyRole, "trackKey"}
    };
}

void TrackListModel::setTracks(QVector<TrackInfo> tracks) {
    beginResetModel();
    tracks_ = std::move(tracks);
    endResetModel();
    emit countChanged();
}

void TrackListModel::updateFavoriteForTrack(int64_t trackId, bool favorite) {
    for (int i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].id == trackId) {
            tracks_[i].isFavorite = favorite;
            emit dataChanged(index(i), index(i), {IsFavoriteRole});
        }
    }
}

void TrackListModel::updateFavoriteForKey(const QString& trackKey, bool favorite) {
    if (trackKey.isEmpty()) return;
    for (int i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].trackKey() == trackKey) {
            tracks_[i].isFavorite = favorite;
            emit dataChanged(index(i), index(i), {IsFavoriteRole});
        }
    }
}

QVariantMap TrackListModel::trackMapAt(int row) const {
    if (row < 0 || row >= tracks_.size()) return {};
    return tracks_[row].toVariantMap();
}

// ---- AlbumListModel ----

int AlbumListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(albums_.size());
}

QVariant AlbumListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= albums_.size()) return {};
    const AlbumSummary& a = albums_[index.row()];
    switch (role) {
    case AlbumRole:           return a.album.isEmpty() ? QStringLiteral("Unknown Album") : a.album;
    case AlbumArtistRole:     return a.albumArtist.isEmpty() ? QStringLiteral("Unknown Artist") : a.albumArtist;
    case YearRole:            return a.year;
    case TrackCountRole:      return a.trackCount;
    case TotalDurationMsRole: return QVariant::fromValue(a.totalDurationMs);
    case CoverHashRole:       return a.coverHash;
    case FormatBadgeRole:     return a.formatBadge;
    default:                  return {};
    }
}

QHash<int, QByteArray> AlbumListModel::roleNames() const {
    return {
        {AlbumRole, "album"},
        {AlbumArtistRole, "albumArtist"},
        {YearRole, "year"},
        {TrackCountRole, "trackCount"},
        {TotalDurationMsRole, "totalDurationMs"},
        {CoverHashRole, "coverHash"},
        {FormatBadgeRole, "formatBadge"}
    };
}

void AlbumListModel::setAlbums(QVector<AlbumSummary> albums) {
    beginResetModel();
    albums_ = std::move(albums);
    endResetModel();
    emit countChanged();
}

// ---- ArtistListModel ----

int ArtistListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(artists_.size());
}

QVariant ArtistListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= artists_.size()) return {};
    const ArtistSummary& a = artists_[index.row()];
    switch (role) {
    case ArtistRole:     return a.artist.isEmpty() ? QStringLiteral("Unknown Artist") : a.artist;
    case AlbumCountRole: return a.albumCount;
    case TrackCountRole: return a.trackCount;
    case CoverHashRole:  return a.coverHash;
    default:             return {};
    }
}

QHash<int, QByteArray> ArtistListModel::roleNames() const {
    return {
        {ArtistRole, "artist"},
        {AlbumCountRole, "albumCount"},
        {TrackCountRole, "trackCount"},
        {CoverHashRole, "coverHash"}
    };
}

void ArtistListModel::setArtists(QVector<ArtistSummary> artists) {
    beginResetModel();
    artists_ = std::move(artists);
    endResetModel();
    emit countChanged();
}

// ---- LibraryController ----

LibraryController::LibraryController(LibraryDatabase* db,
                                     LibraryScanner* scanner,
                                     CoverArtProvider* covers,
                                     AppSettings* settings,
                                     PlaybackController* player,
                                     QObject* parent)
    : QObject(parent)
    , db_(db)
    , scanner_(scanner)
    , covers_(covers)
    , settings_(settings)
    , player_(player) {
    if (scanner_) {
        connect(scanner_, &LibraryScanner::scanningChanged, this, &LibraryController::scanningChanged);
        connect(scanner_, &LibraryScanner::progressChanged, this, &LibraryController::scanningChanged);
        connect(scanner_, &LibraryScanner::scanFinished, this, [this](int count) {
            refreshAll();
            if (player_ && count > 0) {
                player_->showToast(tr("Library updated: %1 track(s) indexed").arg(count));
            }
        });
    }

    if (player_ && player_->engine()) {
        connect(player_->engine(), &PlaybackEngine::trackStartedPlayback, this, [this](const TrackInfo& track) {
            if (!db_ || !db_->isOpen()) return;
            bool recorded = false;
            if (track.isOnline()) {
                recorded = db_->recordOnlinePlayback(track);
            } else if (track.id >= 0) {
                recorded = db_->recordPlayback(track.id);
            }
            if (recorded) {
                if (currentSection_ == QStringLiteral("recent") && tracksLoaded_) {
                    reloadTracksForCurrentSection();
                } else {
                    tracksDirty_ = true;
                }
                reloadHomeSummaries();
                emit libraryUpdated();
            }
        });
        connect(player_->engine(), &PlaybackEngine::currentTrackChanged, this, [this](const TrackInfo& track, int) {
            if (track.isOnline() && db_ && db_->isOpen() && player_) {
                player_->setCurrentTrackFavoriteState(db_->isOnlineTrackFavorite(track.source, track.remoteId));
            }
        });
        player_->setUrlExpander([this](const QList<QUrl>& urls) {
            return expandDroppedUrls(urls);
        });
    }

    if (settings_) {
        connect(settings_, &AppSettings::onlineSettingsChanged, this, [this]() {
            downloadsDirty_ = true;
            if (downloadsLoaded_) {
                reloadDownloadedTracks();
            }
        });
    }

    refreshAll();
    if (settings_ && scanner_ && !settings_->libraryFolders().isEmpty()) {
        scanner_->setWatchedFolders(settings_->libraryFolders());
        scanner_->startScan(settings_->libraryFolders());
    }
}

bool LibraryController::isScanning() const {
    return scanner_ && scanner_->isScanning();
}

int LibraryController::scanProgress() const {
    return scanner_ ? scanner_->scannedCount() : 0;
}

int LibraryController::scanTotal() const {
    return scanner_ ? scanner_->totalCount() : 0;
}

void LibraryController::setSearchQuery(const QString& q) {
    if (searchQuery_ == q) return;
    searchQuery_ = q;
    emit filterChanged();
    refreshAll();
}

void LibraryController::setSortBy(const QString& sort) {
    if (sortBy_ == sort) {
        sortAscending_ = !sortAscending_;
    } else {
        sortBy_ = sort;
        sortAscending_ = true;
    }
    emit filterChanged();
    reloadTracksForCurrentSection();
}

void LibraryController::setSortAscending(bool asc) {
    if (sortAscending_ == asc) return;
    sortAscending_ = asc;
    emit filterChanged();
    reloadTracksForCurrentSection();
}

void LibraryController::refreshAll() {
    if (!db_ || !db_->isOpen()) return;
    totalTrackCount_ = db_->totalTrackCount();
    tracksDirty_ = true;
    albumsDirty_ = true;
    artistsDirty_ = true;
    downloadsDirty_ = true;

    if (tracksLoaded_) {
        reloadTracksForCurrentSection();
        tracksDirty_ = false;
    }
    if (albumsLoaded_) {
        albumsModel_.setAlbums(db_->queryAlbums(searchQuery_));
        albumsDirty_ = false;
    }
    if (artistsLoaded_) {
        artistsModel_.setArtists(db_->queryArtists(searchQuery_));
        artistsDirty_ = false;
    }
    if (downloadsLoaded_) {
        reloadDownloadedTracks();
        downloadsDirty_ = false;
    }

    reloadPlaylists();
    reloadPinnedPlaylists();
    if (currentSection_ == QStringLiteral("folders")) {
        reloadFolderBrowser();
    }
    reloadHomeSummaries();
    emit libraryUpdated();
}

void LibraryController::ensureTracksLoaded() {
    if (tracksLoaded_ && !tracksDirty_) return;
    if (!db_ || !db_->isOpen()) return;
    tracksLoaded_ = true;
    tracksDirty_ = false;
    reloadTracksForCurrentSection();
}

void LibraryController::ensureAlbumsLoaded() {
    if (albumsLoaded_ && !albumsDirty_) return;
    if (!db_ || !db_->isOpen()) return;
    albumsLoaded_ = true;
    albumsDirty_ = false;
    albumsModel_.setAlbums(db_->queryAlbums(searchQuery_));
}

void LibraryController::ensureArtistsLoaded() {
    if (artistsLoaded_ && !artistsDirty_) return;
    if (!db_ || !db_->isOpen()) return;
    artistsLoaded_ = true;
    artistsDirty_ = false;
    artistsModel_.setArtists(db_->queryArtists(searchQuery_));
}

void LibraryController::ensureDownloadsLoaded() {
    if (downloadsLoaded_ && !downloadsDirty_) return;
    downloadsLoaded_ = true;
    downloadsDirty_ = false;
    reloadDownloadedTracks();
}

QObject* LibraryController::tracksModel() {
    ensureTracksLoaded();
    return &tracksModel_;
}

QObject* LibraryController::albumsModel() {
    ensureAlbumsLoaded();
    return &albumsModel_;
}

QObject* LibraryController::artistsModel() {
    ensureArtistsLoaded();
    return &artistsModel_;
}

QObject* LibraryController::downloadedTracksModel() {
    ensureDownloadsLoaded();
    return &downloadedTracksModel_;
}

void LibraryController::releaseInactiveModels(const QString& activeSection) {
    const bool keepTracks = (activeSection == QStringLiteral("local")
                             || activeSection == QStringLiteral("favorites")
                             || activeSection == QStringLiteral("recent")
                             || activeSection == QStringLiteral("playlist"));
    const bool keepDownloads = (activeSection == QStringLiteral("downloads"));

    if (!keepTracks && tracksLoaded_) {
        tracksModel_.setTracks({});
        tracksLoaded_ = false;
        tracksDirty_ = true;
    }
    if (activeSection != QStringLiteral("local")) {
        if (albumsLoaded_) {
            albumsModel_.setAlbums({});
            albumsLoaded_ = false;
            albumsDirty_ = true;
        }
        if (artistsLoaded_) {
            artistsModel_.setArtists({});
            artistsLoaded_ = false;
            artistsDirty_ = true;
        }
        if (detailTracksModel_.count() > 0) {
            detailTracksModel_.setTracks({});
        }
    }
    if (!keepDownloads && downloadsLoaded_) {
        downloadedTracksModel_.setTracks({});
        downloadsLoaded_ = false;
        downloadsDirty_ = true;
    }
}

void LibraryController::reloadHomeSummaries() {
    if (!db_ || !db_->isOpen()) {
        recentTracksSummary_.clear();
        recentAlbumsSummary_.clear();
        hasRecentPlays_ = false;
        return;
    }

    QVector<TrackInfo> recent = db_->queryRecentlyPlayed(8);
    if (!recent.isEmpty()) {
        hasRecentPlays_ = true;
    } else {
        hasRecentPlays_ = false;
        recent = db_->queryRecentlyAdded(8);
    }

    QVariantList tracksOut;
    tracksOut.reserve(recent.size());
    for (const TrackInfo& t : recent) {
        tracksOut.append(t.toVariantMap());
    }
    recentTracksSummary_ = std::move(tracksOut);

    const QVector<AlbumSummary> albums = db_->queryAlbums({});
    const int albumCount = std::min<int>(8, static_cast<int>(albums.size()));
    QVariantList albumsOut;
    albumsOut.reserve(albumCount);
    for (int i = 0; i < albumCount; ++i) {
        const AlbumSummary& a = albums[i];
        albumsOut.append(QVariantMap{
            {QStringLiteral("album"), a.album},
            {QStringLiteral("artist"), a.albumArtist},
            {QStringLiteral("year"), a.year},
            {QStringLiteral("coverHash"), a.coverHash},
            {QStringLiteral("trackCount"), a.trackCount},
            {QStringLiteral("totalDurationMs"), QVariant::fromValue(a.totalDurationMs)}
        });
    }
    recentAlbumsSummary_ = std::move(albumsOut);
}

void LibraryController::reloadDownloadedTracks() {
    if (!db_ || !db_->isOpen()) {
        downloadedTracksModel_.setTracks({});
        return;
    }
    const QString dlDir = settings_ ? settings_->onlineDownloadDir().trimmed() : AppSettings::defaultDownloadDir();
    if (dlDir.isEmpty()) {
        downloadedTracksModel_.setTracks({});
        return;
    }
    downloadedTracksModel_.setTracks(db_->queryFolderTracks(QDir::cleanPath(dlDir)));
}

void LibraryController::loadSection(const QString& section) {
    currentSection_ = section;
    tracksLoaded_ = true;
    tracksDirty_ = false;
    reloadTracksForCurrentSection();
    if (section == QStringLiteral("folders")) {
        reloadFolderBrowser();
    }
}

void LibraryController::reloadTracksForCurrentSection() {
    if (!db_ || !db_->isOpen()) return;
    if (currentSection_ == QStringLiteral("favorites")) {
        tracksModel_.setTracks(db_->queryFavoriteTracks());
    } else if (currentSection_ == QStringLiteral("recent")) {
        tracksModel_.setTracks(db_->queryRecentlyPlayed(150));
    } else if (currentSection_ == QStringLiteral("playlist") && activePlaylistId_ >= 0) {
        tracksModel_.setTracks(db_->queryPlaylistTracks(activePlaylistId_));
    } else {
        tracksModel_.setTracks(db_->queryTracks(searchQuery_, sortBy_, sortAscending_));
    }
}

void LibraryController::openAlbum(const QString& album, const QString& albumArtist) {
    if (!db_) return;
    const QString rawAlbum = (album == QStringLiteral("Unknown Album")) ? QString() : album;
    const QString rawArtist = (albumArtist == QStringLiteral("Unknown Artist")) ? QString() : albumArtist;
    detailTracksModel_.setTracks(db_->queryAlbumTracks(rawAlbum, rawArtist));
}

void LibraryController::openArtist(const QString& artist) {
    if (!db_) return;
    const QString rawArtist = (artist == QStringLiteral("Unknown Artist")) ? QString() : artist;
    detailTracksModel_.setTracks(db_->queryArtistTracks(rawArtist));
}

void LibraryController::openFolder(const QString& folderPath) {
    currentFolderPath_ = QDir::cleanPath(folderPath);
    reloadFolderBrowser();
}

void LibraryController::goUpFolder() {
    if (currentFolderPath_.isEmpty()) return;
    if (settings_ && settings_->libraryFolders().contains(currentFolderPath_)) {
        currentFolderPath_.clear();
    } else {
        QDir d(currentFolderPath_);
        if (!d.cdUp()) {
            currentFolderPath_.clear();
        } else {
            const QString up = QDir::cleanPath(d.absolutePath());
            // Stop at root list if we stepped outside watched roots
            bool insideWatched = false;
            if (settings_) {
                for (const QString& root : settings_->libraryFolders()) {
                    const QString cleanRoot = QDir::cleanPath(root);
                    if (up == cleanRoot || up.startsWith(cleanRoot + QLatin1Char('/'))) {
                        insideWatched = true;
                        break;
                    }
                }
            }
            currentFolderPath_ = insideWatched ? up : QString();
        }
    }
    reloadFolderBrowser();
}

void LibraryController::reloadFolderBrowser() {
    folderEntries_.clear();
    if (currentFolderPath_.isEmpty()) {
        if (settings_) {
            for (const QString& root : settings_->libraryFolders()) {
                folderEntries_.append(QVariantMap{
                    {QStringLiteral("name"), QFileInfo(root).fileName().isEmpty() ? root : QFileInfo(root).fileName()},
                    {QStringLiteral("path"), root},
                    {QStringLiteral("isRoot"), true}
                });
            }
        }
        detailTracksModel_.setTracks({});
    } else {
        QDir dir(currentFolderPath_);
        const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& sd : subdirs) {
            folderEntries_.append(QVariantMap{
                {QStringLiteral("name"), sd.fileName()},
                {QStringLiteral("path"), sd.absoluteFilePath()},
                {QStringLiteral("isRoot"), false}
            });
        }
        if (db_) {
            detailTracksModel_.setTracks(db_->queryFolderTracks(currentFolderPath_));
        }
    }
    emit folderChanged();
}

void LibraryController::openPlaylist(qint64 playlistId) {
    activePlaylistId_ = playlistId;
    currentSection_ = QStringLiteral("playlist");
    reloadTracksForCurrentSection();
}

void LibraryController::reloadPlaylists() {
    playlists_.clear();
    if (!db_) return;
    for (const auto& p : db_->listPlaylists()) {
        playlists_.append(QVariantMap{
            {QStringLiteral("id"), QVariant::fromValue(p.id)},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("trackCount"), p.trackCount}
        });
    }
    emit playlistsChanged();
}

void LibraryController::reloadPinnedPlaylists() {
    pinnedPlaylists_.clear();
    if (db_ && db_->isOpen()) {
        for (const auto& p : db_->listPinnedOnlinePlaylists()) {
            pinnedPlaylists_.append(QVariantMap{
                {QStringLiteral("id"), QVariant::fromValue(p.id)},
                {QStringLiteral("source"), p.source},
                {QStringLiteral("remoteId"), p.remoteId},
                {QStringLiteral("name"), p.name},
                {QStringLiteral("creator"), p.creator},
                {QStringLiteral("coverUrl"), p.coverUrl},
                {QStringLiteral("trackCount"), p.trackCount}
            });
        }
    }
    emit pinnedPlaylistsChanged();
}

void LibraryController::playFromTracksModel(int index) {
    if (player_ && !tracksModel_.tracks().isEmpty()) {
        player_->playTracks(tracksModel_.tracks(), index);
    }
}

void LibraryController::playFromDetailModel(int index) {
    if (player_ && !detailTracksModel_.tracks().isEmpty()) {
        player_->playTracks(detailTracksModel_.tracks(), index);
    }
}

void LibraryController::enqueueFromTracksModel(int index) {
    if (!player_ || index < 0 || index >= tracksModel_.tracks().size()) return;
    player_->enqueueTracks({tracksModel_.tracks()[index]});
}

void LibraryController::playNextFromTracksModel(int index) {
    if (!player_ || index < 0 || index >= tracksModel_.tracks().size()) return;
    player_->playNextTrack(tracksModel_.tracks()[index]);
}

void LibraryController::playFromDownloadedModel(int index) {
    if (player_ && index >= 0 && index < downloadedTracksModel_.tracks().size()) {
        player_->playTracks(downloadedTracksModel_.tracks(), index);
    }
}

void LibraryController::playNextFromDownloadedModel(int index) {
    if (!player_ || index < 0 || index >= downloadedTracksModel_.tracks().size()) return;
    player_->playNextTrack(downloadedTracksModel_.tracks()[index]);
}

void LibraryController::enqueueFromDownloadedModel(int index) {
    if (!player_ || index < 0 || index >= downloadedTracksModel_.tracks().size()) return;
    player_->enqueueTracks({downloadedTracksModel_.tracks()[index]});
}

QVariantMap LibraryController::quickSearchLocal(const QString& query,
                                                int maxTracks,
                                                int maxAlbums,
                                                int maxArtists) const {
    QVariantMap out;
    out.insert(QStringLiteral("totalTracks"), 0);
    out.insert(QStringLiteral("tracks"), QVariantList{});
    out.insert(QStringLiteral("albums"), QVariantList{});
    out.insert(QStringLiteral("artists"), QVariantList{});

    const QString q = query.trimmed();
    if (!db_ || !db_->isOpen() || q.isEmpty()) {
        return out;
    }

    const QVector<TrackInfo> matchedTracks = db_->queryTracks(q, QStringLiteral("title"), true);
    const QVector<AlbumSummary> matchedAlbums = db_->queryAlbums(q);
    const QVector<ArtistSummary> matchedArtists = db_->queryArtists(q);

    QVariantList trackList;
    const int trackCap = std::min<int>(std::max(0, maxTracks), static_cast<int>(matchedTracks.size()));
    trackList.reserve(trackCap);
    for (int i = 0; i < trackCap; ++i) {
        const TrackInfo& t = matchedTracks[i];
        trackList.append(QVariantMap{
            {QStringLiteral("trackId"), QVariant::fromValue(t.id)},
            {QStringLiteral("title"), t.displayTitle()},
            {QStringLiteral("artist"), t.artist},
            {QStringLiteral("album"), t.album},
            {QStringLiteral("durationMs"), QVariant::fromValue(t.durationMs)},
            {QStringLiteral("formatBadge"), t.formatBadge},
            {QStringLiteral("coverHash"), t.coverHash}
        });
    }

    QVariantList albumList;
    const int albumCap = std::min<int>(std::max(0, maxAlbums), static_cast<int>(matchedAlbums.size()));
    albumList.reserve(albumCap);
    for (int i = 0; i < albumCap; ++i) {
        const AlbumSummary& a = matchedAlbums[i];
        albumList.append(QVariantMap{
            {QStringLiteral("album"), a.album.isEmpty() ? QStringLiteral("Unknown Album") : a.album},
            {QStringLiteral("albumArtist"), a.albumArtist.isEmpty() ? QStringLiteral("Unknown Artist") : a.albumArtist},
            {QStringLiteral("year"), a.year},
            {QStringLiteral("trackCount"), a.trackCount},
            {QStringLiteral("coverHash"), a.coverHash},
            {QStringLiteral("formatBadge"), a.formatBadge}
        });
    }

    QVariantList artistList;
    const int artistCap = std::min<int>(std::max(0, maxArtists), static_cast<int>(matchedArtists.size()));
    artistList.reserve(artistCap);
    for (int i = 0; i < artistCap; ++i) {
        const ArtistSummary& ar = matchedArtists[i];
        artistList.append(QVariantMap{
            {QStringLiteral("artist"), ar.artist.isEmpty() ? QStringLiteral("Unknown Artist") : ar.artist},
            {QStringLiteral("albumCount"), ar.albumCount},
            {QStringLiteral("trackCount"), ar.trackCount},
            {QStringLiteral("coverHash"), ar.coverHash}
        });
    }

    out.insert(QStringLiteral("totalTracks"), static_cast<int>(matchedTracks.size()));
    out.insert(QStringLiteral("tracks"), trackList);
    out.insert(QStringLiteral("albums"), albumList);
    out.insert(QStringLiteral("artists"), artistList);
    return out;
}

void LibraryController::playTrackById(qint64 trackId) {
    if (!db_ || !db_->isOpen() || !player_ || trackId < 0) return;
    TrackInfo t;
    if (db_->findTrackById(trackId, t)) {
        player_->playTracks({t}, 0);
    }
}

void LibraryController::toggleFavorite(qint64 trackId, bool currentFav) {
    if (!db_ || trackId < 0) return;
    const bool nextFav = !currentFav;
    if (db_->setFavorite(trackId, nextFav)) {
        tracksModel_.updateFavoriteForTrack(trackId, nextFav);
        detailTracksModel_.updateFavoriteForTrack(trackId, nextFav);
        downloadedTracksModel_.updateFavoriteForTrack(trackId, nextFav);
        if (player_ && player_->trackId() == trackId) {
            player_->setCurrentTrackFavoriteState(nextFav);
        }
        if (currentSection_ == QStringLiteral("favorites") && tracksLoaded_ && !nextFav) {
            reloadTracksForCurrentSection();
        } else {
            tracksDirty_ = true;
        }
        emit favoritesChanged();
    }
}

bool LibraryController::toggleTrackFavorite(const QVariantMap& trackMap) {
    if (!db_ || !db_->isOpen()) return false;
    const TrackInfo track = trackFromVariantMap(trackMap);
    const QString key = track.trackKey();

    if (track.isOnline()) {
        const bool curFav = db_->isOnlineTrackFavorite(track.source, track.remoteId);
        const bool nextFav = !curFav;
        int64_t persistedId = -1;
        if (!db_->setOnlineTrackFavorite(track, nextFav, &persistedId)) {
            return curFav;
        }
        tracksModel_.updateFavoriteForKey(key, nextFav);
        detailTracksModel_.updateFavoriteForKey(key, nextFav);
        downloadedTracksModel_.updateFavoriteForKey(key, nextFav);
        if (player_ && player_->trackKey() == key) {
            player_->setCurrentTrackFavoriteState(nextFav);
        }
        if (currentSection_ == QStringLiteral("favorites") && tracksLoaded_) {
            reloadTracksForCurrentSection();
        } else {
            tracksDirty_ = true;
        }
        emit favoritesChanged();
        if (player_) {
            player_->showToast(nextFav ? tr("Added to Favorites") : tr("Removed from Favorites"));
        }
        return nextFav;
    }

    if (track.id >= 0) {
        bool curFav = track.isFavorite;
        TrackInfo existing;
        if (db_->findTrackById(track.id, existing)) {
            curFav = existing.isFavorite;
        }
        toggleFavorite(track.id, curFav);
        if (player_) {
            player_->showToast(!curFav ? tr("Added to Favorites") : tr("Removed from Favorites"));
        }
        return !curFav;
    }
    return false;
}

bool LibraryController::isTrackFavorite(const QString& source, const QString& remoteId) const {
    return db_ && db_->isOpen() && !remoteId.trimmed().isEmpty()
        && db_->isOnlineTrackFavorite(source, remoteId);
}

qint64 LibraryController::createPlaylist(const QString& name) {
    if (!db_) return -1;
    const int64_t id = db_->createPlaylist(name);
    if (id >= 0) {
        reloadPlaylists();
    }
    return id;
}

void LibraryController::deletePlaylist(qint64 playlistId) {
    if (!db_) return;
    if (db_->deletePlaylist(playlistId)) {
        if (activePlaylistId_ == playlistId) {
            activePlaylistId_ = -1;
            currentSection_ = QStringLiteral("tracks");
            reloadTracksForCurrentSection();
        }
        reloadPlaylists();
    }
}

void LibraryController::addTrackToPlaylist(qint64 playlistId, qint64 trackId) {
    if (!db_ || playlistId < 0 || trackId < 0) return;
    if (db_->addTrackToPlaylist(playlistId, trackId)) {
        reloadPlaylists();
        if (activePlaylistId_ == playlistId && currentSection_ == QStringLiteral("playlist") && tracksLoaded_) {
            reloadTracksForCurrentSection();
        }
        if (player_) player_->showToast(tr("Added to playlist"));
    }
}

void LibraryController::addTrackMapToPlaylist(qint64 playlistId, const QVariantMap& trackMap) {
    if (!db_ || !db_->isOpen() || playlistId < 0) return;
    TrackInfo track = trackFromVariantMap(trackMap);
    int64_t tid = track.id;
    if (track.isOnline()) {
        tid = db_->upsertOnlineTrack(track);
    } else if (tid < 0 && !track.filePath.isEmpty()) {
        TrackInfo existing;
        if (db_->findTrackByPath(track.filePath, existing)) {
            tid = existing.id;
        }
    }
    if (tid >= 0) {
        addTrackToPlaylist(playlistId, tid);
    }
}

bool LibraryController::pinOnlinePlaylist(const QString& source,
                                          const QString& remoteId,
                                          const QString& name,
                                          const QString& creator,
                                          const QString& coverUrl,
                                          int trackCount) {
    if (!db_ || !db_->isOpen()) return false;
    PinnedOnlinePlaylist p;
    p.source = source.trimmed().isEmpty() ? QStringLiteral("netease") : source.trimmed();
    p.remoteId = remoteId.trimmed();
    p.name = name.trimmed();
    p.creator = creator.trimmed();
    p.coverUrl = coverUrl.trimmed();
    p.trackCount = trackCount;
    if (!db_->pinOnlinePlaylist(p)) return false;
    reloadPinnedPlaylists();
    if (player_) player_->showToast(tr("Pinned playlist \"%1\"").arg(p.name));
    return true;
}

bool LibraryController::unpinOnlinePlaylist(const QString& source, const QString& remoteId) {
    if (!db_ || !db_->isOpen()) return false;
    if (!db_->unpinOnlinePlaylist(source, remoteId)) return false;
    reloadPinnedPlaylists();
    if (player_) player_->showToast(tr("Unpinned playlist"));
    return true;
}

bool LibraryController::isOnlinePlaylistPinned(const QString& source, const QString& remoteId) const {
    return db_ && db_->isOpen() && db_->isOnlinePlaylistPinned(source, remoteId);
}

void LibraryController::removeTrackFromPlaylist(qint64 playlistId, int position) {
    if (!db_) return;
    if (db_->removePlaylistItem(playlistId, position)) {
        reloadPlaylists();
        if (activePlaylistId_ == playlistId) {
            reloadTracksForCurrentSection();
        }
    }
}

void LibraryController::importM3uPlaylist(const QUrl& fileUrl) {
    if (!db_) return;
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const auto entries = M3uPlaylistIO::loadFile(path);
    if (entries.isEmpty()) {
        if (player_) player_->showToast(tr("No entries found in playlist file."));
        return;
    }

    const QString plName = QFileInfo(path).completeBaseName();
    const int64_t plId = db_->createPlaylist(plName.isEmpty() ? tr("Imported Playlist") : plName);
    if (plId < 0) return;

    int added = 0;
    for (const auto& entry : entries) {
        if (!QFileInfo::exists(entry.filePath)) continue;
        TrackInfo t;
        if (!db_->findTrackByPath(entry.filePath, t)) {
            if (FFmpegDecoder::probeFile(entry.filePath, t)) {
                if (covers_) t.coverHash = covers_->indexCoverForAudioFile(entry.filePath);
                db_->upsertTrack(t);
            }
        }
        if (t.id >= 0) {
            db_->addTrackToPlaylist(plId, t.id);
            ++added;
        }
    }
    refreshAll();
    if (player_) {
        player_->showToast(tr("Imported playlist \"%1\" (%2 tracks)").arg(plName).arg(added));
    }
}

void LibraryController::exportM3uPlaylist(qint64 playlistId, const QUrl& dstFileUrl) {
    if (!db_) return;
    const QString path = dstFileUrl.isLocalFile() ? dstFileUrl.toLocalFile() : dstFileUrl.toString();
    const auto tracks = db_->queryPlaylistTracks(playlistId);
    const bool ok = M3uPlaylistIO::saveFile(path, tracks, false);
    if (player_) {
        player_->showToast(ok ? tr("Exported M3U8 playlist successfully") : tr("Failed to export playlist"));
    }
}

void LibraryController::addWatchedFolder(const QUrl& folderUrl) {
    if (!settings_) return;
    const QString local = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (local.isEmpty() || !QDir(local).exists()) return;
    settings_->addLibraryFolder(local);
    if (scanner_) {
        scanner_->setWatchedFolders(settings_->libraryFolders());
        scanner_->startScan(settings_->libraryFolders());
    }
    reloadFolderBrowser();
}

void LibraryController::removeWatchedFolder(const QString& folderPath) {
    if (!settings_) return;
    settings_->removeLibraryFolder(folderPath);
    if (db_) {
        db_->removeTracksOutsideFolders(settings_->libraryFolders());
    }
    if (scanner_) {
        scanner_->setWatchedFolders(settings_->libraryFolders());
    }
    if (currentFolderPath_ == folderPath) {
        currentFolderPath_.clear();
    }
    refreshAll();
}

void LibraryController::rescanLibrary() {
    if (scanner_ && settings_ && !settings_->libraryFolders().isEmpty()) {
        scanner_->startScan(settings_->libraryFolders(), true);
    }
}

bool LibraryController::ingestLocalFile(const QString& audioFilePath) {
    return ingestDownloadedTrack(audioFilePath, {}, {});
}

bool LibraryController::ingestDownloadedTrack(const QString& audioFilePath,
                                              const QString& onlineSource,
                                              const QString& onlineRemoteId) {
    if (!db_ || !db_->isOpen()) return false;
    const QFileInfo fi(audioFilePath);
    if (!fi.exists() || !fi.isFile()) return false;
    if (!PlaybackController::isSupportedAudioExtension(fi.suffix().toLower())) return false;

    const QString absPath = fi.absoluteFilePath();
    const QString absDir = QDir::cleanPath(fi.absolutePath());
    if (settings_) {
        bool covered = false;
        for (const QString& folder : settings_->libraryFolders()) {
            QString cleanFolder = QDir::cleanPath(folder);
            if (absDir == cleanFolder || absDir.startsWith(cleanFolder + QLatin1Char('/'))) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            settings_->addLibraryFolder(absDir);
            if (scanner_) {
                scanner_->setWatchedFolders(settings_->libraryFolders());
            }
        }
    }

    TrackInfo info;
    if (!FFmpegDecoder::probeFile(absPath, info)) {
        return false;
    }
    if (covers_) {
        info.coverHash = covers_->indexCoverForAudioFile(absPath);
    }

    const QString rid = onlineRemoteId.trimmed();
    const bool ok = !rid.isEmpty()
        ? db_->upgradeOnlineTrackToLocal(onlineSource, rid, info)
        : db_->upsertTrack(info);
    if (!ok) {
        return false;
    }
    refreshAll();
    return true;
}

bool LibraryController::findUpgradedLocalTrack(const QString& remoteId, TrackInfo& out) const {
    if (!db_ || !db_->isOpen() || remoteId.trimmed().isEmpty()) return false;
    if (!db_->findLocalTrackByRemoteId(remoteId, out)) return false;
    return !out.filePath.isEmpty() && QFileInfo::exists(out.filePath);
}

QVector<TrackInfo> LibraryController::expandDroppedUrls(const QList<QUrl>& urls) {
    QVector<TrackInfo> tracks;
    auto appendAudioFile = [&](const QString& path) {
        const QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) return;
        const QString ext = fi.suffix().toLower();

        if (ext == QStringLiteral("cue")) {
            const CueParseResult cue = CueParser::parseFile(fi.absoluteFilePath());
            if (cue.valid) {
                QString coverHash;
                if (covers_ && !cue.tracks.isEmpty()) {
                    coverHash = covers_->indexCoverForAudioFile(cue.tracks.first().filePath);
                }
                for (TrackInfo t : cue.tracks) {
                    t.coverHash = coverHash;
                    tracks.push_back(std::move(t));
                }
            }
            return;
        }
        if (ext == QStringLiteral("m3u") || ext == QStringLiteral("m3u8")) {
            const auto entries = M3uPlaylistIO::loadFile(fi.absoluteFilePath());
            for (const auto& e : entries) {
                if (QFileInfo::exists(e.filePath)) {
                    TrackInfo t;
                    if (db_ && db_->findTrackByPath(e.filePath, t)) {
                        tracks.push_back(std::move(t));
                    } else if (FFmpegDecoder::probeFile(e.filePath, t)) {
                        if (covers_) t.coverHash = covers_->indexCoverForAudioFile(e.filePath);
                        tracks.push_back(std::move(t));
                    }
                }
            }
            return;
        }
        if (!PlaybackController::isSupportedAudioExtension(ext)) return;

        TrackInfo t;
        if (db_ && db_->findTrackByPath(fi.absoluteFilePath(), t)) {
            tracks.push_back(std::move(t));
            return;
        }
        if (FFmpegDecoder::probeFile(fi.absoluteFilePath(), t)) {
            if (covers_) t.coverHash = covers_->indexCoverForAudioFile(fi.absoluteFilePath());
            tracks.push_back(std::move(t));
        }
    };

    for (const QUrl& url : urls) {
        const QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
        const QFileInfo fi(local);
        if (fi.isDir()) {
            QDirIterator it(local, QDir::Files, QDirIterator::Subdirectories);
            QStringList files;
            while (it.hasNext()) files.push_back(it.next());
            files.sort(Qt::CaseInsensitive);
            for (const QString& f : files) appendAudioFile(f);
        } else {
            appendAudioFile(local);
        }
    }
    return tracks;
}

} // namespace virela
