#include "lyrics/LyricsController.h"

#include <QVariantList>
#include <QVariantMap>
#include <algorithm>

namespace virela {

namespace {

int64_t resolveWordEndMs(const LyricLine& line, int wordIdx, int64_t nextLineTs) {
    const int64_t startMs = line.words[wordIdx].timestampMs;
    if (line.words[wordIdx].endTimestampMs > startMs) {
        return line.words[wordIdx].endTimestampMs;
    }
    if (wordIdx + 1 < line.words.size() && line.words[wordIdx + 1].timestampMs > startMs) {
        return line.words[wordIdx + 1].timestampMs;
    }
    if (line.durationMs > 0 && line.timestampMs >= 0 && line.timestampMs + line.durationMs > startMs) {
        return line.timestampMs + line.durationMs;
    }
    // Cap final word duration when followed by a long instrumental interlude.
    const int64_t chars = std::max<int64_t>(1, line.words[wordIdx].text.trimmed().size());
    const int64_t maxTailMs = std::clamp<int64_t>(chars * 350 + 500, 700, 2600);
    if (nextLineTs > startMs) {
        return std::max<int64_t>(startMs + 60, std::min<int64_t>(nextLineTs, startMs + maxTailMs));
    }
    return startMs + maxTailMs;
}

} // namespace

LyricsController::LyricsController(QObject* parent) : QAbstractListModel(parent) {}

int LyricsController::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(parsed_.lines.size());
}

qint64 LyricsController::lineDurationMs(int row) const {
    if (row < 0 || row >= parsed_.lines.size()) {
        return 3500;
    }
    if (parsed_.lines[row].durationMs > 0) {
        return std::clamp<qint64>(parsed_.lines[row].durationMs, 300, 15000);
    }
    const int64_t ts = parsed_.lines[row].timestampMs;
    if (ts >= 0 && row + 1 < parsed_.lines.size()) {
        const int64_t nextTs = parsed_.lines[row + 1].timestampMs;
        if (nextTs > ts) {
            return std::clamp<qint64>(nextTs - ts, 600, 12000);
        }
    }
    return 3500;
}

QVariantList LyricsController::wordTokensForRow(int row) const {
    QVariantList list;
    if (row < 0 || row >= parsed_.lines.size()) {
        return list;
    }
    const LyricLine& line = parsed_.lines[row];
    if (line.words.isEmpty()) {
        return list;
    }

    const int64_t nextLineTs = (row + 1 < parsed_.lines.size())
        ? parsed_.lines[row + 1].timestampMs
        : -1;

    list.reserve(line.words.size());
    for (int i = 0; i < line.words.size(); ++i) {
        const int64_t startMs = line.words[i].timestampMs;
        const int64_t endMs = std::max<int64_t>(startMs + 60, resolveWordEndMs(line, i, nextLineTs));

        QVariantMap tok;
        tok.insert(QStringLiteral("startMs"), QVariant::fromValue(static_cast<qint64>(startMs)));
        tok.insert(QStringLiteral("endMs"), QVariant::fromValue(static_cast<qint64>(endMs)));
        tok.insert(QStringLiteral("text"), line.words[i].text);
        list.push_back(tok);
    }
    return list;
}

QVariant LyricsController::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= parsed_.lines.size()) {
        return {};
    }
    const int row = index.row();
    const LyricLine& line = parsed_.lines[row];
    switch (role) {
    case TimestampMsRole:    return QVariant::fromValue(line.timestampMs);
    case TextRole:           return line.text;
    case TranslationRole:    return line.translation;
    case IsActiveRole:       return row == activeLineIndex_;
    case HasWordsRole:       return !line.words.isEmpty();
    case WordTokensRole:     return wordTokensForRow(row);
    case LineDurationMsRole: return QVariant::fromValue(lineDurationMs(row));
    default:                 return {};
    }
}

QHash<int, QByteArray> LyricsController::roleNames() const {
    return {
        {TimestampMsRole, "timestampMs"},
        {TextRole, "text"},
        {TranslationRole, "translation"},
        {IsActiveRole, "isActive"},
        {HasWordsRole, "hasWords"},
        {WordTokensRole, "wordTokens"},
        {LineDurationMsRole, "lineDurationMs"}
    };
}

QString LyricsController::activeLineText() const {
    if (activeLineIndex_ >= 0 && activeLineIndex_ < parsed_.lines.size()) {
        return parsed_.lines[activeLineIndex_].text;
    }
    return {};
}

QString LyricsController::activeLineTranslation() const {
    if (activeLineIndex_ >= 0 && activeLineIndex_ < parsed_.lines.size()) {
        return parsed_.lines[activeLineIndex_].translation;
    }
    return {};
}

void LyricsController::loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics) {
    beginResetModel();
    parsed_ = LyricsParser::loadForTrack(audioFilePath, embeddedLyrics);
    hasEnhancedWords_ = std::any_of(parsed_.lines.begin(), parsed_.lines.end(), [](const LyricLine& l) {
        return !l.words.isEmpty();
    });
    activeLineIndex_ = -1;
    userOffsetMs_ = 0;
    lastPositionMs_ = 0;
    effectivePositionMs_ = parsed_.offsetMs;
    endResetModel();

    emit lyricsChanged();
    emit activeLineIndexChanged();
    emit userOffsetMsChanged();
    emit positionProgressChanged();
}

void LyricsController::loadRawLyrics(const QString& primaryLyrics,
                                     const QString& translationLyrics,
                                     const QString& sourceLabel) {
    beginResetModel();
    parsed_ = LyricsParser::parseWithTranslation(primaryLyrics, translationLyrics, sourceLabel);
    hasEnhancedWords_ = std::any_of(parsed_.lines.begin(), parsed_.lines.end(), [](const LyricLine& l) {
        return !l.words.isEmpty();
    });
    activeLineIndex_ = -1;
    userOffsetMs_ = 0;
    effectivePositionMs_ = lastPositionMs_ + parsed_.offsetMs;
    endResetModel();

    emit lyricsChanged();
    emit activeLineIndexChanged();
    emit userOffsetMsChanged();
    emit positionProgressChanged();

    if (lastPositionMs_ > 0) {
        updatePositionMs(lastPositionMs_);
    }
}

void LyricsController::clear() {
    beginResetModel();
    parsed_ = ParsedLyrics{};
    hasEnhancedWords_ = false;
    activeLineIndex_ = -1;
    userOffsetMs_ = 0;
    lastPositionMs_ = 0;
    effectivePositionMs_ = 0;
    endResetModel();

    emit lyricsChanged();
    emit activeLineIndexChanged();
    emit userOffsetMsChanged();
    emit positionProgressChanged();
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

    effectivePositionMs_ = positionMs + parsed_.offsetMs + userOffsetMs_;

    const int nextIdx = LyricsParser::findActiveLineIndex(parsed_, positionMs, userOffsetMs_);
    if (nextIdx != activeLineIndex_) {
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

    emit positionProgressChanged();
}

qreal LyricsController::activeLineProgress() const {
    if (activeLineIndex_ < 0 || activeLineIndex_ >= parsed_.lines.size()) return 0.0;
    const auto& line = parsed_.lines[activeLineIndex_];
    if (line.words.isEmpty()) {
        return std::clamp<qreal>(
            static_cast<qreal>(effectivePositionMs_ - line.timestampMs) / lineDurationMs(activeLineIndex_),
            0.0, 1.0);
    }

    const int64_t nextLineTs = activeLineIndex_ + 1 < parsed_.lines.size()
        ? parsed_.lines[activeLineIndex_ + 1].timestampMs : -1;
    qreal weightedSum = 0.0;
    qreal totalWeight = 0.0;
    for (int i = 0; i < line.words.size(); ++i) {
        const auto& word = line.words[i];
        const int64_t endMs = std::max<int64_t>(word.timestampMs + 60, resolveWordEndMs(line, i, nextLineTs));
        const qreal progress = static_cast<qreal>(effectivePositionMs_ - word.timestampMs) / (endMs - word.timestampMs);
        const qreal weight = std::max<qreal>(1.0, word.text.size());
        weightedSum += weight * std::clamp<qreal>(progress, 0.0, 1.0);
        totalWeight += weight;
    }
    return weightedSum / totalWeight;
}

qint64 LyricsController::timestampForLine(int lineIndex) const {
    if (lineIndex < 0 || lineIndex >= parsed_.lines.size()) return -1;
    const int64_t ts = parsed_.lines[lineIndex].timestampMs;
    return ts >= 0 ? std::max<int64_t>(0, ts - parsed_.offsetMs - userOffsetMs_) : -1;
}

} // namespace virela
