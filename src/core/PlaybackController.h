#pragma once

#include "audio/AudioTypes.h"
#include "audio/PlaybackEngine.h"

#include <QAbstractListModel>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace sonora {

class AppSettings;
class ThemeManager;
class PlatformBridge;

class QueueListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TrackIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        DurationMsRole,
        FormatBadgeRole,
        CoverHashRole,
        FilePathRole,
        IsCurrentRole
    };

    explicit QueueListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTracks(const QVector<TrackInfo>& tracks, int currentIndex);
    void setCurrentIndex(int currentIndex);

private:
    QVector<TrackInfo> tracks_;
    int currentIndex_ = -1;
};

class PlaybackController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool isStopped READ isStopped NOTIFY stateChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY stateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)

    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)

    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentTrackChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(qint64 trackId READ trackId NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackFormatBadge READ trackFormatBadge NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackTechSummary READ trackTechSummary NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackCoverUrl READ trackCoverUrl NOTIFY currentTrackChanged)
    Q_PROPERTY(QString trackFilePath READ trackFilePath NOTIFY currentTrackChanged)
    Q_PROPERTY(bool trackIsFavorite READ trackIsFavorite NOTIFY currentTrackChanged)
    Q_PROPERTY(QVariantMap currentTrackMap READ currentTrackMap NOTIFY currentTrackChanged)

    Q_PROPERTY(QString outputDeviceName READ outputDeviceName NOTIFY outputStatusChanged)
    Q_PROPERTY(QString outputHardwareFormat READ outputHardwareFormat NOTIFY outputStatusChanged)
    Q_PROPERTY(int outputSampleRate READ outputSampleRate NOTIFY outputStatusChanged)
    Q_PROPERTY(bool exclusiveActive READ exclusiveActive NOTIFY outputStatusChanged)
    Q_PROPERTY(bool bitPerfectActive READ bitPerfectActive NOTIFY outputStatusChanged)
    Q_PROPERTY(QString outputStatusBadge READ outputStatusBadge NOTIFY outputStatusChanged)

    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QVariantList eqPresets READ eqPresets CONSTANT)
    Q_PROPERTY(QObject* queueModel READ queueModel CONSTANT)

    Q_PROPERTY(QString memoryUsageText READ memoryUsageText NOTIFY memoryStatsChanged)
    Q_PROPERTY(QString toastMessage READ toastMessage NOTIFY toastChanged)

public:
    PlaybackController(PlaybackEngine* engine,
                       AppSettings* settings,
                       ThemeManager* theme,
                       PlatformBridge* platform = nullptr,
                       QObject* parent = nullptr);

    PlaybackEngine* engine() const { return engine_; }

    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    int playbackState() const;
    qint64 positionMs() const { return positionMs_; }
    qint64 durationMs() const { return durationMs_; }

    double volume() const;
    void setVolume(double v);
    bool muted() const;
    void setMuted(bool m);
    bool shuffle() const;
    void setShuffle(bool s);
    int repeatMode() const;
    void setRepeatMode(int m);

    bool hasTrack() const { return !currentTrack_.filePath.isEmpty(); }
    int currentIndex() const { return currentIndex_; }
    int queueCount() const;
    qint64 trackId() const { return currentTrack_.id; }
    QString trackTitle() const { return currentTrack_.displayTitle(); }
    QString trackArtist() const { return currentTrack_.artist; }
    QString trackAlbum() const { return currentTrack_.album; }
    QString trackFormatBadge() const { return currentTrack_.formatBadge; }
    QString trackTechSummary() const { return currentTrack_.techSummary(); }
    QString trackCoverUrl() const;
    QString trackFilePath() const { return currentTrack_.filePath; }
    bool trackIsFavorite() const { return currentTrack_.isFavorite; }
    QVariantMap currentTrackMap() const { return currentTrack_.toVariantMap(); }
    const TrackInfo& currentTrackInfo() const { return currentTrack_; }

    QString outputDeviceName() const { return outputStatus_.deviceName; }
    QString outputHardwareFormat() const { return outputStatus_.hardwareFormat; }
    int outputSampleRate() const { return outputStatus_.sampleRate; }
    bool exclusiveActive() const { return outputStatus_.exclusiveActive; }
    bool bitPerfectActive() const { return outputStatus_.bitPerfectActive; }
    QString outputStatusBadge() const;

    QVariantList audioDevices() const { return audioDevices_; }
    QVariantList eqPresets() const;
    QObject* queueModel() { return &queueModel_; }

    QString memoryUsageText() const { return memoryUsageText_; }
    QString toastMessage() const { return toastMessage_; }

    // Playback transport invokables
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seekMs(qint64 ms);
    Q_INVOKABLE void seekRatio(double ratio);
    Q_INVOKABLE void cycleRepeatMode();

    Q_INVOKABLE void playQueueIndex(int index);
    Q_INVOKABLE void removeFromQueue(int index);
    Q_INVOKABLE void clearQueue();

    Q_INVOKABLE void openUrls(const QList<QUrl>& urls, bool appendOnly = false);
    Q_INVOKABLE void playTracks(const QVector<TrackInfo>& tracks, int startIndex = 0);
    Q_INVOKABLE void enqueueTracks(const QVector<TrackInfo>& tracks);
    Q_INVOKABLE void playNextTrack(const TrackInfo& track);

    Q_INVOKABLE void refreshAudioDevices();
    Q_INVOKABLE void refreshMemoryStats();
    Q_INVOKABLE void showToast(const QString& message);
    Q_INVOKABLE void clearToast();
    Q_INVOKABLE void setCurrentTrackFavoriteState(bool fav);
    Q_INVOKABLE static QString formatTime(qint64 ms);

    static bool isSupportedAudioExtension(const QString& extLower);

    // Optional hook for Phase 2 CUE / M3U expansion when dropping files
    using UrlExpanderFn = std::function<QVector<TrackInfo>(const QList<QUrl>&)>;
    void setUrlExpander(UrlExpanderFn fn) { urlExpander_ = std::move(fn); }

    using CoverLoaderFn = std::function<QImage(const QString& coverHash)>;
    void setCoverLoader(CoverLoaderFn fn) { coverLoader_ = std::move(fn); }

signals:
    void stateChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void mutedChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void currentTrackChanged();
    void queueChanged();
    void outputStatusChanged();
    void audioDevicesChanged();
    void memoryStatsChanged();
    void toastChanged();
    void eqCurveChanged();

private:
    void syncSettingsToEngine();
    void updateCoverAndAmbient(const TrackInfo& track);
    QVector<TrackInfo> defaultExpandUrls(const QList<QUrl>& urls) const;

    PlaybackEngine* engine_ = nullptr;
    AppSettings* settings_ = nullptr;
    ThemeManager* theme_ = nullptr;
    PlatformBridge* platform_ = nullptr;

    QueueListModel queueModel_;
    TrackInfo currentTrack_;
    int currentIndex_ = -1;
    qint64 positionMs_ = 0;
    qint64 durationMs_ = 0;
    ActiveOutputStatus outputStatus_;
    QVariantList audioDevices_;
    QString memoryUsageText_;
    QString toastMessage_;
    QString trackCoverUrl_;
    UrlExpanderFn urlExpander_;
    CoverLoaderFn coverLoader_;
};

} // namespace sonora
