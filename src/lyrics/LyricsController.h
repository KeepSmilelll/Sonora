#pragma once

#include "lyrics/LyricsParser.h"

#include <QAbstractListModel>

namespace sonora {

class LyricsController : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool isSynced READ isSynced NOTIFY lyricsChanged)
    Q_PROPERTY(int activeLineIndex READ activeLineIndex NOTIFY activeLineIndexChanged)
    Q_PROPERTY(qint64 userOffsetMs READ userOffsetMs WRITE setUserOffsetMs NOTIFY userOffsetMsChanged)
    Q_PROPERTY(QString sourceLabel READ sourceLabel NOTIFY lyricsChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY lyricsChanged)

public:
    enum Roles {
        TimestampMsRole = Qt::UserRole + 1,
        TextRole,
        TranslationRole,
        IsActiveRole
    };

    explicit LyricsController(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool hasLyrics() const { return !parsed_.lines.isEmpty(); }
    bool isSynced() const { return parsed_.synced; }
    int activeLineIndex() const { return activeLineIndex_; }
    qint64 userOffsetMs() const { return userOffsetMs_; }
    void setUserOffsetMs(qint64 offsetMs);
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

private:
    ParsedLyrics parsed_;
    int activeLineIndex_ = -1;
    qint64 userOffsetMs_ = 0;
    qint64 lastPositionMs_ = 0;
};

} // namespace sonora
