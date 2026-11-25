#pragma once

#include <QColor>
#include <QImage>
#include <QObject>
#include <QTranslator>

class QQmlEngine;

namespace sonora {

class AppSettings;

class ThemeManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)
    Q_PROPERTY(QString backdropMode READ backdropMode WRITE setBackdropMode NOTIFY backdropChanged)
    Q_PROPERTY(bool nativeBackdropSupported READ nativeBackdropSupported CONSTANT)
    Q_PROPERTY(bool nativeBackdropActive READ nativeBackdropActive NOTIFY backdropChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    Q_PROPERTY(QColor ambientPrimary READ ambientPrimary NOTIFY ambientColorsChanged)
    Q_PROPERTY(QColor ambientSecondary READ ambientSecondary NOTIFY ambientColorsChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY ambientColorsChanged)

public:
    explicit ThemeManager(AppSettings* settings, QQmlEngine* qmlEngine = nullptr, QObject* parent = nullptr);

    void setQmlEngine(QQmlEngine* engine) { qmlEngine_ = engine; }

    bool isDark() const { return isDark_; }
    QString themeMode() const;
    void setThemeMode(const QString& mode);

    QString backdropMode() const;
    void setBackdropMode(const QString& mode);

    bool nativeBackdropSupported() const { return nativeBackdropSupported_; }
    bool nativeBackdropActive() const;

    QString language() const;
    void setLanguage(const QString& lang);

    QColor ambientPrimary() const { return ambientPrimary_; }
    QColor ambientSecondary() const { return ambientSecondary_; }
    QColor accentColor() const { return accentColor_; }

    // Extracts refined Apple Music-style ambient dual-tone + accessible accent colors from a thumbnail
    void updateFromCoverImage(const QImage& cover);
    void resetAmbientColors();

    Q_INVOKABLE void toggleDarkLight();

signals:
    void themeChanged();
    void backdropChanged();
    void languageChanged();
    void ambientColorsChanged();

private:
    void resolveTheme();
    void applyLanguage(const QString& lang);
    static bool detectWindows11BackdropSupport();

    AppSettings* settings_ = nullptr;
    QQmlEngine* qmlEngine_ = nullptr;
    QTranslator translator_;
    bool translatorInstalled_ = false;

    bool isDark_ = true;
    bool nativeBackdropSupported_ = false;
    QColor ambientPrimary_;
    QColor ambientSecondary_;
    QColor accentColor_;
};

} // namespace sonora
