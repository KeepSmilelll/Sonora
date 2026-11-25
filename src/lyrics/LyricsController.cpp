#include "lyrics/LyricsController.h"

#include <algorithm>

namespace sonora {

LyricsController::LyricsController(QObject* parent) : QAbstractListModel(parent) {}

int LyricsController::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(parsed_.lines.size());
}

QVariant LyricsController::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= parsed_.lines.size()) {
        return {};
    }
    const LyricLine& line = parsed_.lines[index.row()];
    switch (role) {
    case TimestampMsRole: return QVariant::fromValue(line.timestampMs);
    case TextRole:        return line.text;
    case TranslationRole: return line.translation;
    case IsActiveRole:    return index.row() == activeLineIndex_;
    default:              return {};
    }
}

QHash<int, QByteArray> LyricsController::roleNames() const {
    return {
        {TimestampMsRole, "timestampMs"},
        {TextRole, "text"},
        {TranslationRole, "translation"},
        {IsActiveRole, "isActive"}
    };
}

void LyricsController::loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics) {
    beginResetModel();
    parsed_ = LyricsParser::loadForTrack(audioFilePath, embeddedLyrics);
    activeLineIndex_ = -1;
    userOffsetMs_ = 0;
    lastPositionMs_ = 0;
    endResetModel();

    emit lyricsChanged();
    emit activeLineIndexChanged();
    emit userOffsetMsChanged();
}

void LyricsController::clear() {
    beginResetModel();
    parsed_ = ParsedLyrics{};
    activeLineIndex_ = -1;
    userOffsetMs_ = 0;
    lastPositionMs_ = 0;
    endResetModel();

    emit lyricsChanged();
    emit activeLineIndexChanged();
    emit userOffsetMsChanged();
}

void LyricsController::setUserOffsetMs(qint64 offsetMs) {
    const qint64 clamped = std::clamp<qint64>(offsetMs, -10000, 10000);
    if (userOffsetMs_ == clamped) return;
    userOffsetMs_ = clamped;
    emit userOffsetMsChanged();
    updatePositionMs(lastPositionMs_);
}

void LyricsController::adjustOffsetMs(qint64 deltaMs) {
    setUserOffsetMs(userOffsetMs_ + deltaMs);
}

void LyricsController::updatePositionMs(qint64 positionMs) {
    lastPositionMs_ = positionMs;
    if (!parsed_.synced || parsed_.lines.isEmpty()) return;

    const int nextIdx = LyricsParser::findActiveLineIndex(parsed_, positionMs, userOffsetMs_);
    if (nextIdx == activeLineIndex_) return;

    const int oldIdx = activeLineIndex_;
    activeLineIndex_ = nextIdx;
    if (oldIdx >= 0 && oldIdx < parsed_.lines.size()) {
        emit dataChanged(index(oldIdx), index(oldIdx), {IsActiveRole});
    }
    if (activeLineIndex_ >= 0 && activeLineIndex_ < parsed_.lines.size()) {
        emit dataChanged(index(activeLineIndex_), index(activeLineIndex_), {IsActiveRole});
    }
    emit activeLineIndexChanged();
}

qint64 LyricsController::timestampForLine(int lineIndex) const {
    if (lineIndex < 0 || lineIndex >= parsed_.lines.size()) return -1;
    const int64_t ts = parsed_.lines[lineIndex].timestampMs;
    return ts >= 0 ? std::max<int64_t>(0, ts - parsed_.offsetMs - userOffsetMs_) : -1;
}

} // namespace sonora
