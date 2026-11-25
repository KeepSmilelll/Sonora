#include "core/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace sonora {

QString AppSettings::defaultDataDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.local/share/Sonora");
    }
    QDir().mkpath(base);
    return base;
}

AppSettings::AppSettings(const QString& overrideConfigPath, QObject* parent)
    : QObject(parent)
    , configPath_(overrideConfigPath.isEmpty()
                      ? (defaultDataDir() + QStringLiteral("/settings.json"))
                      : overrideConfigPath) {
    load();
}

void AppSettings::load() {
    QFile f(configPath_);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();

    themeMode_ = obj.value(QStringLiteral("themeMode")).toString(themeMode_);
    backdropMode_ = obj.value(QStringLiteral("backdropMode")).toString(backdropMode_);
    language_ = obj.value(QStringLiteral("language")).toString(language_);
    volume_ = std::clamp(obj.value(QStringLiteral("volume")).toDouble(volume_), 0.0, 1.0);
    shuffle_ = obj.value(QStringLiteral("shuffle")).toBool(shuffle_);
    repeatMode_ = std::clamp(obj.value(QStringLiteral("repeatMode")).toInt(repeatMode_), 0, 2);

    outputDeviceId_ = obj.value(QStringLiteral("outputDeviceId")).toString();
    wasapiExclusive_ = obj.value(QStringLiteral("wasapiExclusive")).toBool(wasapiExclusive_);
    autoNativeSampleRate_ = obj.value(QStringLiteral("autoNativeSampleRate")).toBool(autoNativeSampleRate_);
    bitPerfectBypass_ = obj.value(QStringLiteral("bitPerfectBypass")).toBool(bitPerfectBypass_);
    dsdTargetSampleRate_ = obj.value(QStringLiteral("dsdTargetSampleRate")).toInt(dsdTargetSampleRate_);

    replayGainMode_ = std::clamp(obj.value(QStringLiteral("replayGainMode")).toInt(replayGainMode_), 0, 2);
    replayGainPreampDb_ = std::clamp(obj.value(QStringLiteral("replayGainPreampDb")).toDouble(replayGainPreampDb_), -15.0, 15.0);

    eqEnabled_ = obj.value(QStringLiteral("eqEnabled")).toBool(eqEnabled_);
    eqPreampDb_ = std::clamp(obj.value(QStringLiteral("eqPreampDb")).toDouble(eqPreampDb_), -15.0, 15.0);
    eqPresetId_ = obj.value(QStringLiteral("eqPresetId")).toString(eqPresetId_);
    const QJsonArray bands = obj.value(QStringLiteral("eqBands")).toArray();
    if (bands.size() == static_cast<qsizetype>(BiquadEq::kBandCount)) {
        for (size_t i = 0; i < BiquadEq::kBandCount; ++i) {
            eqBands_[i] = std::clamp(bands[static_cast<qsizetype>(i)].toDouble(0.0), -12.0, 12.0);
        }
    }

    closeToTray_ = obj.value(QStringLiteral("closeToTray")).toBool(closeToTray_);
    globalHotkeysEnabled_ = obj.value(QStringLiteral("globalHotkeysEnabled")).toBool(globalHotkeysEnabled_);
    visualizerEnabled_ = obj.value(QStringLiteral("visualizerEnabled")).toBool(visualizerEnabled_);

    libraryFolders_.clear();
    const QJsonArray folders = obj.value(QStringLiteral("libraryFolders")).toArray();
    for (const QJsonValue& v : folders) {
        const QString p = v.toString().trimmed();
        if (!p.isEmpty() && !libraryFolders_.contains(p)) {
            libraryFolders_.push_back(p);
        }
    }
}

void AppSettings::save() const {
    QDir().mkpath(QFileInfo(configPath_).absolutePath());
    QJsonObject obj;
    obj.insert(QStringLiteral("themeMode"), themeMode_);
    obj.insert(QStringLiteral("backdropMode"), backdropMode_);
    obj.insert(QStringLiteral("language"), language_);
    obj.insert(QStringLiteral("volume"), volume_);
    obj.insert(QStringLiteral("shuffle"), shuffle_);
    obj.insert(QStringLiteral("repeatMode"), repeatMode_);

    obj.insert(QStringLiteral("outputDeviceId"), outputDeviceId_);
    obj.insert(QStringLiteral("wasapiExclusive"), wasapiExclusive_);
    obj.insert(QStringLiteral("autoNativeSampleRate"), autoNativeSampleRate_);
    obj.insert(QStringLiteral("bitPerfectBypass"), bitPerfectBypass_);
    obj.insert(QStringLiteral("dsdTargetSampleRate"), dsdTargetSampleRate_);

    obj.insert(QStringLiteral("replayGainMode"), replayGainMode_);
    obj.insert(QStringLiteral("replayGainPreampDb"), replayGainPreampDb_);

    obj.insert(QStringLiteral("eqEnabled"), eqEnabled_);
    obj.insert(QStringLiteral("eqPreampDb"), eqPreampDb_);
    obj.insert(QStringLiteral("eqPresetId"), eqPresetId_);
    QJsonArray bands;
    for (double g : eqBands_) bands.append(g);
    obj.insert(QStringLiteral("eqBands"), bands);

    obj.insert(QStringLiteral("closeToTray"), closeToTray_);
    obj.insert(QStringLiteral("globalHotkeysEnabled"), globalHotkeysEnabled_);
    obj.insert(QStringLiteral("visualizerEnabled"), visualizerEnabled_);

    QJsonArray folders;
    for (const QString& p : libraryFolders_) folders.append(p);
    obj.insert(QStringLiteral("libraryFolders"), folders);

    QFile f(configPath_);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}

OutputConfig AppSettings::toOutputConfig() const {
    OutputConfig cfg;
    cfg.deviceId = outputDeviceId_;
    cfg.mode = wasapiExclusive_ ? OutputMode::WasapiExclusive : OutputMode::WasapiShared;
    cfg.autoNativeSampleRate = autoNativeSampleRate_;
    cfg.bitPerfectBypass = bitPerfectBypass_;
    cfg.dsdTargetSampleRate = dsdTargetSampleRate_;
    return cfg;
}

void AppSettings::setThemeMode(const QString& mode) {
    if (themeMode_ == mode) return;
    themeMode_ = mode;
    save();
    emit themeModeChanged();
}

void AppSettings::setBackdropMode(const QString& mode) {
    if (backdropMode_ == mode) return;
    backdropMode_ = mode;
    save();
    emit backdropModeChanged();
}

void AppSettings::setLanguage(const QString& lang) {
    if (language_ == lang) return;
    language_ = lang;
    save();
    emit languageChanged();
}

void AppSettings::setVolume(double v) {
    const double clamped = std::clamp(v, 0.0, 1.0);
    if (std::abs(volume_ - clamped) < 1e-4) return;
    volume_ = clamped;
    save();
    emit volumeChanged();
}

void AppSettings::setShuffle(bool s) {
    if (shuffle_ == s) return;
    shuffle_ = s;
    save();
    emit shuffleChanged();
}

void AppSettings::setRepeatMode(int m) {
    const int clamped = std::clamp(m, 0, 2);
    if (repeatMode_ == clamped) return;
    repeatMode_ = clamped;
    save();
    emit repeatModeChanged();
}

void AppSettings::setOutputDeviceId(const QString& id) {
    if (outputDeviceId_ == id) return;
    outputDeviceId_ = id;
    save();
    emit audioOutputSettingsChanged();
}

void AppSettings::setWasapiExclusive(bool ex) {
    if (wasapiExclusive_ == ex) return;
    wasapiExclusive_ = ex;
    save();
    emit audioOutputSettingsChanged();
}

void AppSettings::setAutoNativeSampleRate(bool autoRate) {
    if (autoNativeSampleRate_ == autoRate) return;
    autoNativeSampleRate_ = autoRate;
    save();
    emit audioOutputSettingsChanged();
}

void AppSettings::setBitPerfectBypass(bool bypass) {
    if (bitPerfectBypass_ == bypass) return;
    bitPerfectBypass_ = bypass;
    save();
    emit audioOutputSettingsChanged();
}

void AppSettings::setDsdTargetSampleRate(int sr) {
    if (dsdTargetSampleRate_ == sr) return;
    dsdTargetSampleRate_ = sr;
    save();
    emit audioOutputSettingsChanged();
}

void AppSettings::setReplayGainMode(int m) {
    const int clamped = std::clamp(m, 0, 2);
    if (replayGainMode_ == clamped) return;
    replayGainMode_ = clamped;
    save();
    emit replayGainChanged();
}

void AppSettings::setReplayGainPreampDb(double db) {
    const double clamped = std::clamp(db, -15.0, 15.0);
    if (std::abs(replayGainPreampDb_ - clamped) < 1e-4) return;
    replayGainPreampDb_ = clamped;
    save();
    emit replayGainChanged();
}

void AppSettings::setEqEnabled(bool en) {
    if (eqEnabled_ == en) return;
    eqEnabled_ = en;
    save();
    emit eqChanged();
}

void AppSettings::setEqPreampDb(double db) {
    const double clamped = std::clamp(db, -15.0, 15.0);
    if (std::abs(eqPreampDb_ - clamped) < 1e-4) return;
    eqPreampDb_ = clamped;
    save();
    emit eqChanged();
}

void AppSettings::setEqPresetId(const QString& presetId) {
    if (eqPresetId_ == presetId) return;
    eqPresetId_ = presetId;
    save();
    emit eqChanged();
}

QVariantList AppSettings::eqBandsVariant() const {
    QVariantList out;
    out.reserve(static_cast<qsizetype>(BiquadEq::kBandCount));
    for (double g : eqBands_) out.append(g);
    return out;
}

void AppSettings::setEqBandGain(int index, double gainDb) {
    if (index < 0 || index >= static_cast<int>(BiquadEq::kBandCount)) return;
    const double clamped = std::clamp(gainDb, -12.0, 12.0);
    if (std::abs(eqBands_[static_cast<size_t>(index)] - clamped) < 1e-4) return;
    eqBands_[static_cast<size_t>(index)] = clamped;
    eqPresetId_ = QStringLiteral("custom");
    save();
    emit eqChanged();
}

void AppSettings::applyEqPreset(const QString& presetId) {
    for (const auto& preset : BiquadEq::builtInPresets()) {
        if (preset.id == presetId) {
            eqPresetId_ = preset.id;
            eqPreampDb_ = preset.preampDb;
            eqBands_ = preset.gainsDb;
            save();
            emit eqChanged();
            return;
        }
    }
}

void AppSettings::setCloseToTray(bool v) {
    if (closeToTray_ == v) return;
    closeToTray_ = v;
    save();
    emit desktopSettingsChanged();
}

void AppSettings::setGlobalHotkeysEnabled(bool v) {
    if (globalHotkeysEnabled_ == v) return;
    globalHotkeysEnabled_ = v;
    save();
    emit desktopSettingsChanged();
}

void AppSettings::setVisualizerEnabled(bool v) {
    if (visualizerEnabled_ == v) return;
    visualizerEnabled_ = v;
    save();
    emit visualizerEnabledChanged();
}

void AppSettings::setLibraryFolders(const QStringList& folders) {
    if (libraryFolders_ == folders) return;
    libraryFolders_ = folders;
    save();
    emit libraryFoldersChanged();
}

void AppSettings::addLibraryFolder(const QString& folder) {
    const QString cleaned = QDir::cleanPath(folder.trimmed());
    if (cleaned.isEmpty() || libraryFolders_.contains(cleaned)) return;
    libraryFolders_.push_back(cleaned);
    save();
    emit libraryFoldersChanged();
}

void AppSettings::removeLibraryFolder(const QString& folder) {
    const QString cleaned = QDir::cleanPath(folder.trimmed());
    if (libraryFolders_.removeAll(cleaned) > 0) {
        save();
        emit libraryFoldersChanged();
    }
}

} // namespace sonora
