#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>

class QQuickWindow;

namespace virela {

// Single-draw-call GPU SceneGraph procedural fluid aurora mesh (QSGGeometryNode).
// Zero offscreen FBOs, zero textures, < 16 KB RAM, auto-suspends when idle/hidden/solid.
class FluidAuroraItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(QColor primaryColor READ primaryColor WRITE setPrimaryColor NOTIFY styleChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY styleChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY styleChanged)
    Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor NOTIFY styleChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY styleChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY activeChanged)
    Q_PROPERTY(qreal intensity READ intensity WRITE setIntensity NOTIFY styleChanged)
    Q_PROPERTY(qreal immersiveFactor READ immersiveFactor WRITE setImmersiveFactor NOTIFY styleChanged)
    Q_PROPERTY(qreal audioEnergy READ audioEnergy WRITE setAudioEnergy NOTIFY styleChanged)

public:
    explicit FluidAuroraItem(QQuickItem* parent = nullptr);

    QColor primaryColor() const { return primaryColor_; }
    void setPrimaryColor(const QColor& c);

    QColor secondaryColor() const { return secondaryColor_; }
    void setSecondaryColor(const QColor& c);

    QColor accentColor() const { return accentColor_; }
    void setAccentColor(const QColor& c);

    QColor baseColor() const { return baseColor_; }
    void setBaseColor(const QColor& c);

    bool darkMode() const { return darkMode_; }
    void setDarkMode(bool dark);

    bool active() const { return active_; }
    void setActive(bool a);

    bool isPlaying() const { return playing_; }
    void setPlaying(bool p);

    qreal intensity() const { return intensity_; }
    void setIntensity(qreal v);

    qreal immersiveFactor() const { return immersiveFactor_; }
    void setImmersiveFactor(qreal v);

    qreal audioEnergy() const { return audioEnergy_; }
    void setAudioEnergy(qreal v);

signals:
    void styleChanged();
    void activeChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void updateTimerState();
    void onFrameTick();
    void triggerSettle(int frames = 24);

    QPointer<QQuickWindow> observedWindow_;

    QColor primaryColor_{42, 48, 72};
    QColor secondaryColor_{24, 28, 42};
    QColor accentColor_{250, 72, 102};
    QColor baseColor_{11, 13, 20};
    bool darkMode_ = true;
    bool active_ = true;
    bool playing_ = false;
    qreal intensity_ = 0.75;
    qreal immersiveFactor_ = 0.0;
    qreal audioEnergy_ = 0.0;

    // Smoothed current render state (RGB + A in [0, 1])
    float curP_[4]{0.16f, 0.19f, 0.28f, 1.0f};
    float curS_[4]{0.09f, 0.11f, 0.16f, 1.0f};
    float curA_[4]{0.98f, 0.28f, 0.40f, 1.0f};
    float curB_[4]{0.043f, 0.051f, 0.078f, 1.0f};
    float curIntensity_ = 0.75f;
    float curImmersive_ = 0.0f;
    float curEnergy_ = 0.0f;
    bool stateInitialized_ = false;

    float phase_ = 0.0f;
    int settleFrames_ = 0;
    QTimer frameTimer_;
};

} // namespace virela
