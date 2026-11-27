#pragma once

#include "lyrics/LyricsParser.h"

#include <QAbstractListModel>

namespace virela {

class LyricsController : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool isSynced READ isSynced NOTIFY lyricsChanged)
    Q_PROPERTY(bool hasEnhancedWords READ hasEnhancedWords NOTIFY lyricsChanged)
    Q_PROPERTY(int activeLineIndex READ activeLineIndex NOTIFY activeLineIndexChanged)
    Q_PROPERTY(qint64 userOffsetMs READ userOffsetMs WRITE setUserOffsetMs NOTIFY userOffsetMsChanged)
    Q_PROPERTY(qint64 effectivePositionMs READ effectivePositionMs NOTIFY positionProgressChanged)
    Q_PROPERTY(qreal activeLineProgress READ activeLineProgress NOTIFY positionProgressChanged)
    Q_PROPERTY(QString activeLineText READ activeLineText NOTIFY activeLineIndexChanged)
    Q_PROPERTY(QString activeLineTranslation READ activeLineTranslation NOTIFY activeLineIndexChanged)
    Q_PROPERTY(QString sourceLabel READ sourceLabel NOTIFY lyricsChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lyricsChanged)

public:
    enum Roles {
        TimestampMsRole = Qt::UserRole + 1,
        TextRole,
        TranslationRole,
        IsActiveRole,
        HasWordsRole,
        WordTokensRole,
        LineDurationMsRole
    };

    explicit LyricsController(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool hasLyrics() const { return !parsed_.lines.isEmpty(); }
    bool isSynced() const { return parsed_.synced; }
    bool hasEnhancedWords() const { return hasEnhancedWords_; }
    int activeLineIndex() const { return activeLineIndex_; }
    qint64 userOffsetMs() const { return userOffsetMs_; }
    void setUserOffsetMs(qint64 offsetMs);
    qint64 effectivePositionMs() const { return effectivePositionMs_; }
    qreal activeLineProgress() const { return activeLineProgress_; }
    QString activeLineText() const;
    QString activeLineTranslation() const;
    QString sourceLabel() const { return parsed_.sourceLabel; }
    int lineCount() const { return static_cast<int>(parsed_.lines.size()); }

    Q_INVOKABLE void loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics = {});
    Q_INVOKABLE void clear();
    Q_INVOKABLE void updatePositionMs(qint64 positionMs);
    Q_INVOKABLE void adjustOffsetMs(qint64 deltaMs);
    Q_INVOKABLE qint64 timestampForLine(int lineIndex) const;

signals:
    void lyricsChanged();
    void activeLineIndexChanged();
    void userOffsetMsChanged();
    void positionProgressChanged();

private:
    qint64 lineDurationMs(int row) const;
    QVariantList wordTokensForRow(int row) const;

    ParsedLyrics parsed_;
    bool hasEnhancedWords_ = false;
    int activeLineIndex_ = -1;
    qint64 userOffsetMs_ = 0;
    qint64 lastPositionMs_ = 0;
    qint64 effectivePositionMs_ = 0;
    qreal activeLineProgress_ = 0.0;
};

} // namespace virela
