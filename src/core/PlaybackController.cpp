#include "core/PlaybackController.h"
#include "audio/FFmpegDecoder.h"
#include "core/AppSettings.h"
#include "core/MemoryStats.h"
#include "core/ThemeManager.h"
#include "platform/PlatformBridge.h"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>
#include <algorithm>

namespace sonora {

int QueueListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(tracks_.size());
}

QVariant QueueListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= tracks_.size()) {
        return {};
    }
    const TrackInfo& t = tracks_[index.row()];
    switch (role) {
    case TrackIdRole:     return QVariant::fromValue(t.id);
    case TitleRole:       return t.displayTitle();
    case ArtistRole:      return t.artist;
    case AlbumRole:       return t.album;
    case DurationMsRole:  return QVariant::fromValue(t.durationMs);
    case FormatBadgeRole: return t.formatBadge;
    case CoverHashRole:   return t.coverHash;
    case FilePathRole:    return t.filePath;
    case IsCurrentRole:   return index.row() == currentIndex_;
    default:              return {};
    }
}

QHash<int, QByteArray> QueueListModel::roleNames() const {
    return {
        {TrackIdRole, "trackId"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {DurationMsRole, "durationMs"},
        {FormatBadgeRole, "formatBadge"},
        {CoverHashRole, "coverHash"},
        {FilePathRole, "filePath"},
        {IsCurrentRole, "isCurrent"}
    };
}

void QueueListModel::setTracks(const QVector<TrackInfo>& tracks, int currentIndex) {
    beginResetModel();
    tracks_ = tracks;
    currentIndex_ = currentIndex;
    endResetModel();
}

void QueueListModel::setCurrentIndex(int currentIndex) {
    if (currentIndex_ == currentIndex) return;
    const int old = currentIndex_;
    currentIndex_ = currentIndex;
    if (old >= 0 && old < tracks_.size()) {
        emit dataChanged(index(old), index(old), {IsCurrentRole});
    }
    if (currentIndex_ >= 0 && currentIndex_ < tracks_.size()) {
        emit dataChanged(index(currentIndex_), index(currentIndex_), {IsCurrentRole});
    }
}

PlaybackController::PlaybackController(PlaybackEngine* engine,
                                       AppSettings* settings,
                                       ThemeManager* theme,
                                       PlatformBridge* platform,
                                       QObject* parent)
    : QObject(parent)
    , engine_(engine)
    , settings_(settings)
    , theme_(theme)
    , platform_(platform) {
    syncSettingsToEngine();
    refreshAudioDevices();
    refreshMemoryStats();

    connect(engine_, &PlaybackEngine::stateChanged, this, [this](PlaybackState st) {
        emit stateChanged();
        if (platform_) {
            platform_->setTrayPlayingState(st == PlaybackState::Playing);
            platform_->updateMediaSessionState(st, positionMs_, durationMs_);
        }
    });

    connect(engine_, &PlaybackEngine::positionChanged, this, [this](qint64 pos) {
        positionMs_ = pos;
        emit positionChanged();
    });

    connect(engine_, &PlaybackEngine::durationChanged, this, [this](qint64 dur) {
        durationMs_ = dur;
        emit durationChanged();
        if (platform_) {
            platform_->updateMediaSessionState(engine_->state(), positionMs_, durationMs_);
        }
    });

    connect(engine_, &PlaybackEngine::currentTrackChanged, this, [this](const TrackInfo& track, int idx) {
        currentTrack_ = track;
        currentIndex_ = idx;
        queueModel_.setCurrentIndex(idx);
        updateCoverAndAmbient(track);
        emit currentTrackChanged();
        if (platform_) {
            const QString tip = track.artist.isEmpty()
                ? QStringLiteral("Sonora — %1").arg(track.displayTitle())
                : QStringLiteral("Sonora — %1 - %2").arg(track.displayTitle(), track.artist);
            platform_->updateTrayTooltip(tip);
        }
    });

    connect(engine_, &PlaybackEngine::queueChanged, this, [this]() {
        queueModel_.setTracks(engine_->queue(), engine_->currentIndex());
        emit queueChanged();
    });

    connect(engine_, &PlaybackEngine::outputStatusChanged, this, [this](const ActiveOutputStatus& st) {
        outputStatus_ = st;
        emit outputStatusChanged();
        if (!st.fallbackNotice.isEmpty()) {
            showToast(st.fallbackNotice);
        }
    });

    connect(engine_, &PlaybackEngine::errorOccurred, this, [this](const QString& msg) {
        showToast(msg);
    });

    if (settings_) {
        connect(settings_, &AppSettings::audioOutputSettingsChanged, this, [this]() {
            engine_->setOutputConfig(settings_->toOutputConfig());
        });
        connect(settings_, &AppSettings::replayGainChanged, this, [this]() {
            engine_->setReplayGainPreampDb(settings_->replayGainPreampDb());
            engine_->setReplayGainMode(static_cast<ReplayGainMode>(settings_->replayGainMode()));
        });
        connect(settings_, &AppSettings::eqChanged, this, [this]() {
            auto& eq = engine_->equalizer();
            eq.setEnabled(settings_->eqEnabled());
            eq.setPreampDb(settings_->eqPreampDb());
            eq.setAllBandGainsDb(settings_->eqBands());
            emit eqCurveChanged();
        });
        connect(settings_, &AppSettings::desktopSettingsChanged, this, [this]() {
            if (platform_) {
                platform_->setGlobalHotkeysEnabled(settings_->globalHotkeysEnabled());
            }
        });
    }

    if (platform_) {
        connect(platform_, &PlatformBridge::mediaPlayRequested, this, &PlaybackController::play);
        connect(platform_, &PlatformBridge::mediaPauseRequested, this, &PlaybackController::pause);
        connect(platform_, &PlatformBridge::mediaTogglePlayPauseRequested, this, &PlaybackController::togglePlayPause);
        connect(platform_, &PlatformBridge::mediaStopRequested, this, &PlaybackController::stop);
        connect(platform_, &PlatformBridge::mediaNextRequested, this, &PlaybackController::next);
        connect(platform_, &PlatformBridge::mediaPreviousRequested, this, &PlaybackController::previous);
        connect(platform_, &PlatformBridge::mediaVolumeUpRequested, this, [this]() {
            setVolume(std::min(1.0, volume() + 0.05));
        });
        connect(platform_, &PlatformBridge::mediaVolumeDownRequested, this, [this]() {
            setVolume(std::max(0.0, volume() - 0.05));
        });
    }
}

void PlaybackController::syncSettingsToEngine() {
    if (!engine_ || !settings_) return;
    engine_->setVolume(settings_->volume());
    engine_->setShuffle(settings_->shuffle());
    engine_->setRepeatMode(static_cast<RepeatMode>(settings_->repeatMode()));
    engine_->setOutputConfig(settings_->toOutputConfig());
    engine_->setReplayGainPreampDb(settings_->replayGainPreampDb());
    engine_->setReplayGainMode(static_cast<ReplayGainMode>(settings_->replayGainMode()));

    auto& eq = engine_->equalizer();
    eq.setEnabled(settings_->eqEnabled());
    eq.setPreampDb(settings_->eqPreampDb());
    eq.setAllBandGainsDb(settings_->eqBands());
}

bool PlaybackController::isPlaying() const {
    return engine_ && engine_->state() == PlaybackState::Playing;
}

bool PlaybackController::isPaused() const {
    return engine_ && engine_->state() == PlaybackState::Paused;
}

bool PlaybackController::isStopped() const {
    return !engine_ || engine_->state() == PlaybackState::Stopped;
}

int PlaybackController::playbackState() const {
    return engine_ ? static_cast<int>(engine_->state()) : 0;
}

double PlaybackController::volume() const {
    return engine_ ? engine_->volume() : 0.85;
}

void PlaybackController::setVolume(double v) {
    const double clamped = std::clamp(v, 0.0, 1.0);
    if (engine_) engine_->setVolume(clamped);
    if (settings_) settings_->setVolume(clamped);
    emit volumeChanged();
}

bool PlaybackController::muted() const {
    return engine_ && engine_->isMuted();
}

void PlaybackController::setMuted(bool m) {
    if (engine_) engine_->setMuted(m);
    emit mutedChanged();
}

bool PlaybackController::shuffle() const {
    return engine_ && engine_->isShuffle();
}

void PlaybackController::setShuffle(bool s) {
    if (engine_) engine_->setShuffle(s);
    if (settings_) settings_->setShuffle(s);
    emit shuffleChanged();
}

int PlaybackController::repeatMode() const {
    return engine_ ? static_cast<int>(engine_->repeatMode()) : 0;
}

void PlaybackController::setRepeatMode(int m) {
    const int clamped = std::clamp(m, 0, 2);
    if (engine_) engine_->setRepeatMode(static_cast<RepeatMode>(clamped));
    if (settings_) settings_->setRepeatMode(clamped);
    emit repeatModeChanged();
}

void PlaybackController::cycleRepeatMode() {
    setRepeatMode((repeatMode() + 1) % 3);
}

int PlaybackController::queueCount() const {
    return queueModel_.rowCount();
}

QString PlaybackController::trackCoverUrl() const {
    if (!currentTrack_.coverHash.isEmpty()) {
        return QStringLiteral("image://cover/%1?size=360").arg(currentTrack_.coverHash);
    }
    return inlineCoverDataUrl_;
}

QString PlaybackController::outputStatusBadge() const {
    if (outputStatus_.sampleRate <= 0) return {};
    const double khz = static_cast<double>(outputStatus_.sampleRate) / 1000.0;
    const QString sr = (outputStatus_.sampleRate % 1000 == 0)
        ? QString::number(outputStatus_.sampleRate / 1000) + QStringLiteral(" kHz")
        : QString::number(khz, 'f', 1) + QStringLiteral(" kHz");
    if (outputStatus_.exclusiveActive) {
        return outputStatus_.bitPerfectActive
            ? QStringLiteral("WASAPI Exclusive · Bit-Perfect · %1").arg(sr)
            : QStringLiteral("WASAPI Exclusive · %1 · %2").arg(sr, outputStatus_.hardwareFormat);
    }
    return QStringLiteral("Shared · %1").arg(sr);
}

QVariantList PlaybackController::eqPresets() const {
    QVariantList list;
    for (const auto& p : BiquadEq::builtInPresets()) {
        QVariantList gains;
        for (double g : p.gainsDb) gains.append(g);
        list.append(QVariantMap{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("preampDb"), p.preampDb},
            {QStringLiteral("gains"), gains}
        });
    }
    return list;
}

void PlaybackController::play() { if (engine_) engine_->play(); }
void PlaybackController::pause() { if (engine_) engine_->pause(); }
void PlaybackController::togglePlayPause() { if (engine_) engine_->togglePlayPause(); }
void PlaybackController::stop() { if (engine_) engine_->stop(); }
void PlaybackController::next() { if (engine_) engine_->next(); }
void PlaybackController::previous() { if (engine_) engine_->previous(); }
void PlaybackController::seekMs(qint64 ms) { if (engine_) engine_->seekMs(ms); }

void PlaybackController::seekRatio(double ratio) {
    if (durationMs_ > 0) {
        seekMs(static_cast<qint64>(std::clamp(ratio, 0.0, 1.0) * static_cast<double>(durationMs_)));
    }
}

void PlaybackController::playQueueIndex(int index) {
    if (engine_) engine_->playIndex(index);
}

void PlaybackController::removeFromQueue(int index) {
    if (engine_) engine_->removeFromQueue(index);
}

void PlaybackController::clearQueue() {
    if (engine_) engine_->clearQueue();
}

void PlaybackController::playTracks(const QVector<TrackInfo>& tracks, int startIndex) {
    if (engine_ && !tracks.isEmpty()) {
        engine_->setQueue(tracks, startIndex, true);
    }
}

void PlaybackController::enqueueTracks(const QVector<TrackInfo>& tracks) {
    if (engine_ && !tracks.isEmpty()) {
        engine_->appendToQueue(tracks);
    }
}

void PlaybackController::playNextTrack(const TrackInfo& track) {
    if (engine_) {
        engine_->insertNext(track);
    }
}

bool PlaybackController::isSupportedAudioExtension(const QString& extLower) {
    static const QSet<QString> kExts = {
        QStringLiteral("flac"), QStringLiteral("wav"), QStringLiteral("aiff"),
        QStringLiteral("aif"),  QStringLiteral("alac"), QStringLiteral("ape"),
        QStringLiteral("wv"),   QStringLiteral("tta"),  QStringLiteral("mp3"),
        QStringLiteral("m4a"),  QStringLiteral("aac"),  QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("wma"),  QStringLiteral("dsf"),
        QStringLiteral("dff")
    };
    return kExts.contains(extLower);
}

QVector<TrackInfo> PlaybackController::defaultExpandUrls(const QList<QUrl>& urls) const {
    QVector<TrackInfo> tracks;
    auto addFile = [&](const QString& path) {
        const QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) return;
        if (!isSupportedAudioExtension(fi.suffix().toLower())) return;
        TrackInfo t;
        if (!FFmpegDecoder::probeFile(fi.absoluteFilePath(), t)) {
            t.filePath = fi.absoluteFilePath();
            t.title = fi.completeBaseName();
        }
        tracks.push_back(std::move(t));
    };

    for (const QUrl& url : urls) {
        const QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
        const QFileInfo fi(local);
        if (fi.isDir()) {
            QDirIterator it(local, QDir::Files, QDirIterator::Subdirectories);
            QStringList files;
            while (it.hasNext()) {
                files.push_back(it.next());
            }
            files.sort(Qt::CaseInsensitive);
            for (const QString& f : files) {
                addFile(f);
            }
        } else {
            addFile(local);
        }
    }
    return tracks;
}

void PlaybackController::openUrls(const QList<QUrl>& urls, bool appendOnly) {
    if (urls.isEmpty() || !engine_) return;
    QVector<TrackInfo> tracks = urlExpander_ ? urlExpander_(urls) : defaultExpandUrls(urls);
    if (tracks.isEmpty()) {
        showToast(tr("No supported audio files found."));
        return;
    }
    if (appendOnly) {
        engine_->appendToQueue(tracks);
        showToast(tr("Added %1 track(s) to queue").arg(tracks.size()));
    } else {
        engine_->setQueue(tracks, 0, true);
    }
}

void PlaybackController::refreshAudioDevices() {
    audioDevices_.clear();
    if (engine_) {
        const auto devs = engine_->enumerateDevices();
        for (const auto& d : devs) {
            audioDevices_.append(QVariantMap{
                {QStringLiteral("id"), d.id},
                {QStringLiteral("name"), d.name},
                {QStringLiteral("isDefault"), d.isDefault}
            });
        }
    }
    emit audioDevicesChanged();
}

void PlaybackController::refreshMemoryStats() {
    const MemorySnapshot mem = queryProcessMemory();
    memoryUsageText_ = QStringLiteral("%1 MB RSS / %2 MB Private")
                           .arg(mem.workingSetMb, 0, 'f', 1)
                           .arg(mem.privateBytesMb, 0, 'f', 1);
    emit memoryStatsChanged();
}

void PlaybackController::showToast(const QString& message) {
    toastMessage_ = message;
    emit toastChanged();
}

void PlaybackController::clearToast() {
    if (toastMessage_.isEmpty()) return;
    toastMessage_.clear();
    emit toastChanged();
}

void PlaybackController::setCurrentTrackFavoriteState(bool fav) {
    if (currentTrack_.isFavorite == fav) return;
    currentTrack_.isFavorite = fav;
    emit currentTrackChanged();
}

QString PlaybackController::formatTime(qint64 ms) {
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 hours = totalSec / 3600;
    const qint64 mins = (totalSec % 3600) / 60;
    const qint64 secs = totalSec % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(mins)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

void PlaybackController::updateCoverAndAmbient(const TrackInfo& track) {
    inlineCoverDataUrl_.clear();
    QImage coverImg;

    // 0. Fast path: reuse already-cached thumbnail from CoverArtProvider LRU (zero audio reopen)
    if (!track.coverHash.isEmpty() && coverLoader_) {
        coverImg = coverLoader_(track.coverHash);
    }

    // 1. Embedded cover packet fallback (for ad-hoc drag-and-drop unindexed files)
    if (coverImg.isNull()) {
        const QByteArray embedded = FFmpegDecoder::extractEmbeddedCover(track.filePath);
        if (!embedded.isEmpty()) {
            QBuffer buf;
            buf.setData(embedded);
            buf.open(QIODevice::ReadOnly);
            QImageReader reader(&buf);
            reader.setScaledSize(QSize(320, 320));
            coverImg = reader.read();
        }
    }

    // 2. Folder cover fallback (cover.jpg, folder.png, front.jpg, album.jpg)
    if (coverImg.isNull() && !track.filePath.isEmpty()) {
        const QDir dir = QFileInfo(track.filePath).dir();
        static const QStringList kNames = {
            QStringLiteral("cover.jpg"),  QStringLiteral("cover.png"),  QStringLiteral("cover.webp"),
            QStringLiteral("folder.jpg"), QStringLiteral("folder.png"), QStringLiteral("front.jpg"),
            QStringLiteral("front.png"),  QStringLiteral("album.jpg"),  QStringLiteral("album.png")
        };
        for (const QString& n : kNames) {
            const QString candidate = dir.filePath(n);
            if (QFileInfo::exists(candidate)) {
                QImageReader reader(candidate);
                reader.setScaledSize(QSize(320, 320));
                coverImg = reader.read();
                if (!coverImg.isNull()) break;
            }
        }
    }

    if (!coverImg.isNull()) {
        if (track.coverHash.isEmpty()) {
            // Compact JPEG data URL for non-indexed drag-and-drop tracks (kept tiny < 25 KB)
            QByteArray jpegBytes;
            QBuffer out(&jpegBytes);
            out.open(QIODevice::WriteOnly);
            coverImg.save(&out, "JPG", 82);
            inlineCoverDataUrl_ = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpegBytes.toBase64());
        }
        if (theme_) {
            theme_->updateFromCoverImage(coverImg);
        }
    } else if (theme_) {
        theme_->resetAmbientColors();
    }

    if (platform_) {
        platform_->updateMediaSessionTrack(track, coverImg);
    }
}

} // namespace sonora
