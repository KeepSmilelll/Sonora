#include "core/ThemeManager.h"
#include "core/AppSettings.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QStyleHints>
#include <algorithm>
#include <array>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace sonora {

ThemeManager::ThemeManager(AppSettings* settings, QQmlEngine* qmlEngine, QObject* parent)
    : QObject(parent)
    , settings_(settings)
    , qmlEngine_(qmlEngine)
    , nativeBackdropSupported_(detectWindows11BackdropSupport()) {
    resolveTheme();
    resetAmbientColors();
    if (settings_) {
        applyLanguage(settings_->language());
    }

    if (auto* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this]() {
            if (themeMode() == QStringLiteral("system")) {
                resolveTheme();
                emit themeChanged();
            }
        });
    }
}

bool ThemeManager::detectWindows11BackdropSupport() {
#ifdef _WIN32
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (fn) {
            OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (fn(&rovi) == 0) {
                return rovi.dwMajorVersion >= 10 && rovi.dwBuildNumber >= 22621;
            }
        }
    }
#endif
    return false;
}

QString ThemeManager::themeMode() const {
    return settings_ ? settings_->themeMode() : QStringLiteral("dark");
}

void ThemeManager::setThemeMode(const QString& mode) {
    if (!settings_) return;
    settings_->setThemeMode(mode);
    resolveTheme();
    emit themeChanged();
}

QString ThemeManager::backdropMode() const {
    return settings_ ? settings_->backdropMode() : QStringLiteral("ambient");
}

void ThemeManager::setBackdropMode(const QString& mode) {
    if (!settings_) return;
    settings_->setBackdropMode(mode);
    emit backdropChanged();
}

bool ThemeManager::nativeBackdropActive() const {
    const QString m = backdropMode();
    return nativeBackdropSupported_ && (m == QStringLiteral("mica") || m == QStringLiteral("acrylic"));
}

QString ThemeManager::language() const {
    return settings_ ? settings_->language() : QStringLiteral("zh_CN");
}

void ThemeManager::setLanguage(const QString& lang) {
    if (!settings_) return;
    settings_->setLanguage(lang);
    applyLanguage(lang);
    emit languageChanged();
}

void ThemeManager::toggleDarkLight() {
    setThemeMode(isDark_ ? QStringLiteral("light") : QStringLiteral("dark"));
}

void ThemeManager::resolveTheme() {
    const QString mode = themeMode();
    if (mode == QStringLiteral("dark")) {
        isDark_ = true;
    } else if (mode == QStringLiteral("light")) {
        isDark_ = false;
    } else {
        const auto* hints = QGuiApplication::styleHints();
        isDark_ = !hints || hints->colorScheme() != Qt::ColorScheme::Light;
    }
}

void ThemeManager::applyLanguage(const QString& lang) {
    if (translatorInstalled_) {
        QCoreApplication::removeTranslator(&translator_);
        translatorInstalled_ = false;
    }
    const QString qmPath = QStringLiteral(":/i18n/sonora_%1.qm").arg(lang);
    if (translator_.load(qmPath)) {
        QCoreApplication::installTranslator(&translator_);
        translatorInstalled_ = true;
    }
    if (qmlEngine_) {
        qmlEngine_->retranslate();
    }
}

void ThemeManager::resetAmbientColors() {
    if (isDark_) {
        ambientPrimary_ = QColor(42, 48, 72);
        ambientSecondary_ = QColor(24, 28, 42);
        accentColor_ = QColor(250, 72, 102); // Apple Music rose-coral accent
    } else {
        ambientPrimary_ = QColor(232, 236, 248);
        ambientSecondary_ = QColor(244, 236, 240);
        accentColor_ = QColor(232, 46, 78);
    }
    emit ambientColorsChanged();
}

void ThemeManager::updateFromCoverImage(const QImage& cover) {
    if (cover.isNull()) {
        resetAmbientColors();
        return;
    }

    // Downscale to tiny 32x32 sample grid (< 4 KB temporary memory)
    const QImage tiny = cover.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                            .convertToFormat(QImage::Format_RGB32);

    // 12-bucket hue histogram weighted by chroma to extract harmonious primary & secondary hues
    struct Bucket {
        double r = 0, g = 0, b = 0, weight = 0;
    };
    std::array<Bucket, 12> buckets{};
    double avgR = 0, avgG = 0, avgB = 0;
    int total = 0;

    for (int y = 0; y < tiny.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(tiny.constScanLine(y));
        for (int x = 0; x < tiny.width(); ++x) {
            const QColor c(line[x]);
            avgR += c.red();
            avgG += c.green();
            avgB += c.blue();
            ++total;

            float h = 0, s = 0, l = 0;
            c.getHslF(&h, &s, &l);
            if (h < 0.0f || s < 0.14f || l < 0.12f || l > 0.88f) {
                continue;
            }
            const int bIdx = std::clamp(static_cast<int>(h * 12.0f), 0, 11);
            const double w = static_cast<double>(s) * (1.0 - std::abs(2.0 * l - 1.0));
            buckets[bIdx].r += c.red() * w;
            buckets[bIdx].g += c.green() * w;
            buckets[bIdx].b += c.blue() * w;
            buckets[bIdx].weight += w;
        }
    }

    int best1 = -1, best2 = -1;
    for (int i = 0; i < 12; ++i) {
        if (buckets[i].weight > 0.01) {
            if (best1 < 0 || buckets[i].weight > buckets[best1].weight) {
                best2 = best1;
                best1 = i;
            } else if (best2 < 0 || buckets[i].weight > buckets[best2].weight) {
                best2 = i;
            }
        }
    }

    auto colorFromBucket = [&](int idx) -> QColor {
        if (idx >= 0 && buckets[idx].weight > 0.01) {
            return QColor(
                static_cast<int>(buckets[idx].r / buckets[idx].weight),
                static_cast<int>(buckets[idx].g / buckets[idx].weight),
                static_cast<int>(buckets[idx].b / buckets[idx].weight));
        }
        if (total > 0) {
            return QColor(static_cast<int>(avgR / total),
                          static_cast<int>(avgG / total),
                          static_cast<int>(avgB / total));
        }
        return QColor(250, 72, 102);
    };

    QColor p1 = colorFromBucket(best1);
    QColor p2 = (best2 >= 0) ? colorFromBucket(best2) : p1.darker(135);

    // Clamp saturation/lightness so ambient backdrop never overpowers text readability
    float h1 = 0, s1 = 0, l1 = 0;
    p1.getHslF(&h1, &s1, &l1);
    float h2 = 0, s2 = 0, l2 = 0;
    p2.getHslF(&h2, &s2, &l2);

    if (h1 < 0.0f) h1 = 0.62f;
    if (h2 < 0.0f) h2 = std::fmod(h1 + 0.12f, 1.0f);

    if (isDark_) {
        ambientPrimary_ = QColor::fromHslF(h1, std::clamp(s1, 0.22f, 0.58f), 0.22f);
        ambientSecondary_ = QColor::fromHslF(h2, std::clamp(s2, 0.18f, 0.48f), 0.14f);
        accentColor_ = QColor::fromHslF(h1, std::clamp(s1, 0.55f, 0.85f), 0.62f);
    } else {
        ambientPrimary_ = QColor::fromHslF(h1, std::clamp(s1, 0.18f, 0.42f), 0.88f);
        ambientSecondary_ = QColor::fromHslF(h2, std::clamp(s2, 0.15f, 0.36f), 0.92f);
        accentColor_ = QColor::fromHslF(h1, std::clamp(s1, 0.55f, 0.82f), 0.44f);
    }
    emit ambientColorsChanged();
}

} // namespace sonora
