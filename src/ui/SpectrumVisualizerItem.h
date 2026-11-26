#pragma once

#include "audio/dsp/FftAnalyzer.h"

#include <QColor>
#include <QQuickItem>
#include <QTimer>
#include <array>

namespace sonora {

class PlaybackEngine;

// Single-draw-call GPU SceneGraph spectrum visualizer (QSGGeometryNode).
// Zero QML delegate objects, zero JS GC pressure, auto-suspends when hidden/paused.
class SpectrumVisualizerItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY styleChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY styleChanged)
    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY activeChanged)
    Q_PROPERTY(bool visualizerEnabled READ visualizerEnabled WRITE setVisualizerEnabled NOTIFY activeChanged)
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY styleChanged)
    Q_PROPERTY(QString styleMode READ styleMode WRITE setStyleMode NOTIFY styleChanged)
    Q_PROPERTY(qreal bassEnergy READ bassEnergy NOTIFY bassEnergyChanged)

public:
    explicit SpectrumVisualizerItem(QQuickItem* parent = nullptr);

    static void setSharedEngine(PlaybackEngine* engine);

    QColor accentColor() const { return accentColor_; }
    void setAccentColor(const QColor& c);

    QColor secondaryColor() const { return secondaryColor_; }
    void setSecondaryColor(const QColor& c);

    bool isPlaying() const { return playing_; }
    void setPlaying(bool p);

    bool visualizerEnabled() const { return visualizerEnabled_; }
    void setVisualizerEnabled(bool e);

    bool mirrored() const { return mirrored_; }
    void setMirrored(bool m);

    QString styleMode() const { return styleMode_; }
    void setStyleMode(const QString& mode);

    qreal bassEnergy() const { return static_cast<qreal>(bassEnergy_); }

signals:
    void styleChanged();
    void activeChanged();
    void bassEnergyChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void updateTimerState();
    void onFrameTick();

    static PlaybackEngine* sharedEngine_;

    QColor accentColor_{250, 72, 102};
    QColor secondaryColor_{120, 140, 255};
    bool playing_ = false;
    bool visualizerEnabled_ = true;
    bool mirrored_ = false;
    QString styleMode_{QStringLiteral("bars")};
    float bassEnergy_ = 0.0f;
    float lastEmittedBass_ = 0.0f;
    int idleDecayFrames_ = 0;

    QTimer frameTimer_;
    std::array<float, FftAnalyzer::kBandCount> bars_{};
    std::array<float, FftAnalyzer::kBandCount> peaks_{};
};

} // namespace sonora
