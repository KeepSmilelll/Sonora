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
        {CodecRole, "codec"}
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
        connect(player_->engine(), &PlaybackEngine::trackPlaybackStarted, this, [this](qint64 trackId) {
            if (db_ && trackId >= 0) {
                db_->recordPlayback(trackId);
                if (currentSection_ == QStringLiteral("recent")) {
                    reloadTracksForCurrentSection();
                }
            }
        });
        player_->setUrlExpander([this](const QList<QUrl>& urls) {
            return expandDroppedUrls(urls);
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
    reloadTracksForCurrentSection();
    albumsModel_.setAlbums(db_->queryAlbums(searchQuery_));
    artistsModel_.setArtists(db_->queryArtists(searchQuery_));
    reloadPlaylists();
    reloadFolderBrowser();
    emit libraryUpdated();
}

void LibraryController::loadSection(const QString& section) {
    currentSection_ = section;
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

void LibraryController::toggleFavorite(qint64 trackId, bool currentFav) {
    if (!db_ || trackId < 0) return;
    const bool nextFav = !currentFav;
    if (db_->setFavorite(trackId, nextFav)) {
        tracksModel_.updateFavoriteForTrack(trackId, nextFav);
        detailTracksModel_.updateFavoriteForTrack(trackId, nextFav);
        if (player_ && player_->trackId() == trackId) {
            player_->setCurrentTrackFavoriteState(nextFav);
        }
        if (currentSection_ == QStringLiteral("favorites") && !nextFav) {
            reloadTracksForCurrentSection();
        }
    }
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
        if (activePlaylistId_ == playlistId && currentSection_ == QStringLiteral("playlist")) {
            reloadTracksForCurrentSection();
        }
        if (player_) player_->showToast(tr("Added to playlist"));
    }
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
