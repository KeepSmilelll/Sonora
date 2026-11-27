#pragma once

#include "audio/AudioTypes.h"
#include "audio/dsp/BiquadEq.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <array>

namespace virela {

class AppSettings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString backdropMode READ backdropMode WRITE setBackdropMode NOTIFY backdropModeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)

    Q_PROPERTY(QString outputDeviceId READ outputDeviceId WRITE setOutputDeviceId NOTIFY audioOutputSettingsChanged)
    Q_PROPERTY(bool wasapiExclusive READ wasapiExclusive WRITE setWasapiExclusive NOTIFY audioOutputSettingsChanged)
    Q_PROPERTY(bool autoNativeSampleRate READ autoNativeSampleRate WRITE setAutoNativeSampleRate NOTIFY audioOutputSettingsChanged)
    Q_PROPERTY(bool bitPerfectBypass READ bitPerfectBypass WRITE setBitPerfectBypass NOTIFY audioOutputSettingsChanged)
    Q_PROPERTY(int dsdTargetSampleRate READ dsdTargetSampleRate WRITE setDsdTargetSampleRate NOTIFY audioOutputSettingsChanged)

    Q_PROPERTY(int replayGainMode READ replayGainMode WRITE setReplayGainMode NOTIFY replayGainChanged)
    Q_PROPERTY(double replayGainPreampDb READ replayGainPreampDb WRITE setReplayGainPreampDb NOTIFY replayGainChanged)

    Q_PROPERTY(bool eqEnabled READ eqEnabled WRITE setEqEnabled NOTIFY eqChanged)
    Q_PROPERTY(double eqPreampDb READ eqPreampDb WRITE setEqPreampDb NOTIFY eqChanged)
    Q_PROPERTY(QString eqPresetId READ eqPresetId WRITE setEqPresetId NOTIFY eqChanged)
    Q_PROPERTY(QVariantList eqBands READ eqBandsVariant NOTIFY eqChanged)

    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY desktopSettingsChanged)
    Q_PROPERTY(bool globalHotkeysEnabled READ globalHotkeysEnabled WRITE setGlobalHotkeysEnabled NOTIFY desktopSettingsChanged)
    Q_PROPERTY(bool visualizerEnabled READ visualizerEnabled WRITE setVisualizerEnabled NOTIFY visualizerEnabledChanged)
    Q_PROPERTY(QStringList libraryFolders READ libraryFolders WRITE setLibraryFolders NOTIFY libraryFoldersChanged)

public:
    explicit AppSettings(const QString& overrideConfigPath = {}, QObject* parent = nullptr);

    void load();
    void save() const;

    static QString defaultDataDir();
    QString configPath() const { return configPath_; }

    QString themeMode() const { return themeMode_; }
    void setThemeMode(const QString& mode);

    QString backdropMode() const { return backdropMode_; }
    void setBackdropMode(const QString& mode);

    QString language() const { return language_; }
    void setLanguage(const QString& lang);

    double volume() const { return volume_; }
    void setVolume(double v);

    bool shuffle() const { return shuffle_; }
    void setShuffle(bool s);

    int repeatMode() const { return repeatMode_; }
    void setRepeatMode(int m);

    QString outputDeviceId() const { return outputDeviceId_; }
    void setOutputDeviceId(const QString& id);

    bool wasapiExclusive() const { return wasapiExclusive_; }
    void setWasapiExclusive(bool ex);

    bool autoNativeSampleRate() const { return autoNativeSampleRate_; }
    void setAutoNativeSampleRate(bool autoRate);

    bool bitPerfectBypass() const { return bitPerfectBypass_; }
    void setBitPerfectBypass(bool bypass);

    int dsdTargetSampleRate() const { return dsdTargetSampleRate_; }
    void setDsdTargetSampleRate(int sr);

    OutputConfig toOutputConfig() const;

    int replayGainMode() const { return replayGainMode_; }
    void setReplayGainMode(int m);

    double replayGainPreampDb() const { return replayGainPreampDb_; }
    void setReplayGainPreampDb(double db);

    bool eqEnabled() const { return eqEnabled_; }
    void setEqEnabled(bool en);

    double eqPreampDb() const { return eqPreampDb_; }
    void setEqPreampDb(double db);

    QString eqPresetId() const { return eqPresetId_; }
    void setEqPresetId(const QString& presetId);

    const std::array<double, BiquadEq::kBandCount>& eqBands() const { return eqBands_; }
    QVariantList eqBandsVariant() const;
    Q_INVOKABLE void setEqBandGain(int index, double gainDb);
    Q_INVOKABLE void applyEqPreset(const QString& presetId);

    bool closeToTray() const { return closeToTray_; }
    void setCloseToTray(bool v);

    bool globalHotkeysEnabled() const { return globalHotkeysEnabled_; }
    void setGlobalHotkeysEnabled(bool v);

    bool visualizerEnabled() const { return visualizerEnabled_; }
    void setVisualizerEnabled(bool v);

    QStringList libraryFolders() const { return libraryFolders_; }
    void setLibraryFolders(const QStringList& folders);
    Q_INVOKABLE void addLibraryFolder(const QString& folder);
    Q_INVOKABLE void removeLibraryFolder(const QString& folder);

signals:
    void themeModeChanged();
    void backdropModeChanged();
    void languageChanged();
    void volumeChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void audioOutputSettingsChanged();
    void replayGainChanged();
    void eqChanged();
    void desktopSettingsChanged();
    void visualizerEnabledChanged();
    void libraryFoldersChanged();

private:
    QString configPath_;
    QString themeMode_ = QStringLiteral("system");      // "system" | "dark" | "light"
    QString backdropMode_ = QStringLiteral("mica");     // "mica" | "acrylic" | "ambient" | "solid"
    QString language_ = QStringLiteral("zh_CN");        // "zh_CN" | "en_US"
    double volume_ = 0.85;
    bool shuffle_ = false;
    int repeatMode_ = 0;

    QString outputDeviceId_;
    bool wasapiExclusive_ = false;
    bool autoNativeSampleRate_ = true;
    bool bitPerfectBypass_ = true;
    int dsdTargetSampleRate_ = 176400;

    int replayGainMode_ = 0;
    double replayGainPreampDb_ = 0.0;

    bool eqEnabled_ = false;
    double eqPreampDb_ = 0.0;
    QString eqPresetId_ = QStringLiteral("flat");
    std::array<double, BiquadEq::kBandCount> eqBands_{};

    bool closeToTray_ = true;
    bool globalHotkeysEnabled_ = true;
    bool visualizerEnabled_ = true;
    QStringList libraryFolders_;
};

} // namespace virela
