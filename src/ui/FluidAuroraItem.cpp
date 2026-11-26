#include "ui/FluidAuroraItem.h"

#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <algorithm>
#include <array>
#include <cmath>

namespace sonora {

namespace {

constexpr int kGridCols = 14;
constexpr int kGridRows = 9;
constexpr int kQuadCount = (kGridCols - 1) * (kGridRows - 1);
constexpr int kVertCount = kQuadCount * 6;

void colorToFloat4(const QColor& c, float out[4]) noexcept {
    out[0] = static_cast<float>(c.redF());
    out[1] = static_cast<float>(c.greenF());
    out[2] = static_cast<float>(c.blueF());
    out[3] = static_cast<float>(c.alphaF());
}

void lerpFloat4(float cur[4], const QColor& target, float alpha) noexcept {
    float t[4]{};
    colorToFloat4(target, t);
    for (int i = 0; i < 4; ++i) {
        cur[i] += (t[i] - cur[i]) * alpha;
    }
}

unsigned char clampByte(float v) noexcept {
    return static_cast<unsigned char>(std::clamp(v * 255.0f, 0.0f, 255.0f));
}

} // namespace

FluidAuroraItem::FluidAuroraItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    frameTimer_.setInterval(33); // ~30 fps gentle fluid drift when active & playing
    connect(&frameTimer_, &QTimer::timeout, this, &FluidAuroraItem::onFrameTick);
}

void FluidAuroraItem::setPrimaryColor(const QColor& c) {
    if (primaryColor_ == c) return;
    primaryColor_ = c;
    if (!stateInitialized_) {
        colorToFloat4(c, curP_);
    } else {
        triggerSettle();
    }
    emit styleChanged();
    update();
}

void FluidAuroraItem::setSecondaryColor(const QColor& c) {
    if (secondaryColor_ == c) return;
    secondaryColor_ = c;
    if (!stateInitialized_) {
        colorToFloat4(c, curS_);
    } else {
        triggerSettle();
    }
    emit styleChanged();
    update();
}

void FluidAuroraItem::setAccentColor(const QColor& c) {
    if (accentColor_ == c) return;
    accentColor_ = c;
    if (!stateInitialized_) {
        colorToFloat4(c, curA_);
    } else {
        triggerSettle();
    }
    emit styleChanged();
    update();
}

void FluidAuroraItem::setBaseColor(const QColor& c) {
    if (baseColor_ == c) return;
    baseColor_ = c;
    if (!stateInitialized_) {
        colorToFloat4(c, curB_);
    } else {
        triggerSettle();
    }
    emit styleChanged();
    update();
}

void FluidAuroraItem::setDarkMode(bool dark) {
    if (darkMode_ == dark) return;
    darkMode_ = dark;
    triggerSettle();
    emit styleChanged();
    update();
}

void FluidAuroraItem::setActive(bool a) {
    if (active_ == a) return;
    active_ = a;
    emit activeChanged();
    updateTimerState();
    update();
}

void FluidAuroraItem::setPlaying(bool p) {
    if (playing_ == p) return;
    playing_ = p;
    if (!playing_) {
        triggerSettle(30);
    }
    emit activeChanged();
    updateTimerState();
}

void FluidAuroraItem::setIntensity(qreal v) {
    const qreal clamped = std::clamp(v, 0.0, 1.0);
    if (std::abs(intensity_ - clamped) < 1e-4) return;
    intensity_ = clamped;
    if (!stateInitialized_) {
        curIntensity_ = static_cast<float>(clamped);
    } else {
        triggerSettle(36);
    }
    emit styleChanged();
    updateTimerState();
    update();
}

void FluidAuroraItem::setImmersiveFactor(qreal v) {
    const qreal clamped = std::clamp(v, 0.0, 1.0);
    if (std::abs(immersiveFactor_ - clamped) < 1e-4) return;
    immersiveFactor_ = clamped;
    if (!stateInitialized_) {
        curImmersive_ = static_cast<float>(clamped);
    } else {
        triggerSettle(36);
    }
    emit styleChanged();
    updateTimerState();
    update();
}

void FluidAuroraItem::setAudioEnergy(qreal v) {
    const qreal clamped = std::clamp(v, 0.0, 1.0);
    if (std::abs(audioEnergy_ - clamped) < 1e-3) return;
    audioEnergy_ = clamped;
    if (!stateInitialized_) {
        curEnergy_ = static_cast<float>(clamped);
    } else if (!playing_) {
        triggerSettle(24);
    }
    emit styleChanged();
    update();
}

void FluidAuroraItem::itemChange(ItemChange change, const ItemChangeData& value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange) {
        updateTimerState();
    }
}

void FluidAuroraItem::triggerSettle(int frames) {
    settleFrames_ = std::max(settleFrames_, frames);
    updateTimerState();
}

void FluidAuroraItem::updateTimerState() {
    const bool visibleNow = isVisible() && active_ && window() != nullptr && intensity_ > 0.01;
    const bool shouldTick = visibleNow && (playing_ || settleFrames_ > 0);
    if (shouldTick) {
        if (!frameTimer_.isActive()) frameTimer_.start();
    } else {
        if (frameTimer_.isActive()) frameTimer_.stop();
    }
}

void FluidAuroraItem::onFrameTick() {
    if (!stateInitialized_) {
        colorToFloat4(primaryColor_, curP_);
        colorToFloat4(secondaryColor_, curS_);
        colorToFloat4(accentColor_, curA_);
        colorToFloat4(baseColor_, curB_);
        curIntensity_ = static_cast<float>(intensity_);
        curImmersive_ = static_cast<float>(immersiveFactor_);
        curEnergy_ = static_cast<float>(audioEnergy_);
        stateInitialized_ = true;
    } else {
        constexpr float kLerp = 0.09f;
        lerpFloat4(curP_, primaryColor_, kLerp);
        lerpFloat4(curS_, secondaryColor_, kLerp);
        lerpFloat4(curA_, accentColor_, kLerp);
        lerpFloat4(curB_, baseColor_, kLerp);
        curIntensity_ += (static_cast<float>(intensity_) - curIntensity_) * 0.14f;
        curImmersive_ += (static_cast<float>(immersiveFactor_) - curImmersive_) * 0.16f;
        curEnergy_ += (static_cast<float>(audioEnergy_) - curEnergy_) * 0.16f;
    }

    phase_ += playing_ ? (0.015f + 0.022f * curEnergy_) : 0.004f;
    if (phase_ > 628.3185f) {
        phase_ -= 628.3185f;
    }

    if (!playing_) {
        if (settleFrames_ > 0) --settleFrames_;
        if (settleFrames_ <= 0) {
            updateTimerState();
        }
    }
    update();
}

QSGNode* FluidAuroraItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode();
        auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), kVertCount);
        geom->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(geom);
        node->setFlag(QSGNode::OwnsGeometry);

        auto* mat = new QSGVertexColorMaterial();
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    if (!stateInitialized_) {
        colorToFloat4(primaryColor_, curP_);
        colorToFloat4(secondaryColor_, curS_);
        colorToFloat4(accentColor_, curA_);
        colorToFloat4(baseColor_, curB_);
        curIntensity_ = static_cast<float>(intensity_);
        curImmersive_ = static_cast<float>(immersiveFactor_);
        curEnergy_ = static_cast<float>(audioEnergy_);
        stateInitialized_ = true;
    }

    auto* geom = node->geometry();
    auto* v = geom->vertexDataAsColoredPoint2D();

    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    if (w <= 2.0f || h <= 2.0f || !active_ || curIntensity_ <= 0.005f) {
        for (int i = 0; i < kVertCount; ++i) {
            v[i].set(0.0f, 0.0f, 0, 0, 0, 0);
        }
        node->markDirty(QSGNode::DirtyGeometry);
        return node;
    }

    // 3 drifting focal blooms on gentle Lissajous orbits
    const float cx0 = 0.24f + 0.14f * std::sin(phase_ * 0.85f);
    const float cy0 = 0.28f + 0.13f * std::cos(phase_ * 0.63f);

    const float cx1 = 0.76f + 0.13f * std::cos(phase_ * 0.71f + 1.4f);
    const float cy1 = 0.34f + 0.16f * std::sin(phase_ * 0.54f + 0.8f);

    const float cx2 = 0.52f + 0.20f * std::sin(phase_ * 0.47f + 2.7f);
    const float cy2 = 0.80f + 0.11f * std::cos(phase_ * 0.77f + 2.1f);

    const float bloomBoost = 1.0f + 0.26f * curImmersive_ + 0.22f * curEnergy_;
    const float accentWeightScale = (darkMode_ ? 0.34f : 0.22f) * (1.0f + 0.45f * curImmersive_ + 0.28f * curEnergy_);

    struct GridSample {
        float x;
        float y;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    std::array<GridSample, kGridCols * kGridRows> samples{};

    for (int row = 0; row < kGridRows; ++row) {
        const float vNorm = static_cast<float>(row) / static_cast<float>(kGridRows - 1);
        const float py = vNorm * h;

        for (int col = 0; col < kGridCols; ++col) {
            const float uNorm = static_cast<float>(col) / static_cast<float>(kGridCols - 1);
            const float px = uNorm * w;

            const float dx0 = (uNorm - cx0) * 1.15f;
            const float dy0 = (vNorm - cy0);
            const float w0 = std::exp(-(dx0 * dx0 + dy0 * dy0) * 3.1f) * bloomBoost;

            const float dx1 = (uNorm - cx1) * 1.10f;
            const float dy1 = (vNorm - cy1);
            const float w1 = std::exp(-(dx1 * dx1 + dy1 * dy1) * 2.7f) * bloomBoost;

            const float dx2 = (uNorm - cx2) * 1.05f;
            const float dy2 = (vNorm - cy2);
            const float w2 = std::exp(-(dx2 * dx2 + dy2 * dy2) * 3.6f) * accentWeightScale;

            const float energy = std::clamp((w0 * 0.75f + w1 * 0.65f + w2 * 0.90f) * curIntensity_, 0.0f, 1.0f);

            // Blend bloom colors over base substrate
            float outR = curB_[0];
            float outG = curB_[1];
            float outB = curB_[2];

            const float t0 = std::clamp(w0 * curIntensity_, 0.0f, 0.92f);
            outR = outR * (1.0f - t0) + curP_[0] * t0;
            outG = outG * (1.0f - t0) + curP_[1] * t0;
            outB = outB * (1.0f - t0) + curP_[2] * t0;

            const float t1 = std::clamp(w1 * curIntensity_ * 0.85f, 0.0f, 0.85f);
            outR = outR * (1.0f - t1) + curS_[0] * t1;
            outG = outG * (1.0f - t1) + curS_[1] * t1;
            outB = outB * (1.0f - t1) + curS_[2] * t1;

            const float t2 = std::clamp(w2 * curIntensity_, 0.0f, 0.52f);
            outR = outR * (1.0f - t2) + curA_[0] * t2;
            outG = outG * (1.0f - t2) + curA_[1] * t2;
            outB = outB * (1.0f - t2) + curA_[2] * t2;

            // Keep transparent background pass-through when base alpha < 1 (Win11 Mica/Acrylic)
            const float outA = std::clamp(curB_[3] + (1.0f - curB_[3]) * energy * 0.88f, 0.0f, 1.0f);

            // Premultiplied RGBA for Qt SceneGraph QSGVertexColorMaterial
            const auto idx = static_cast<size_t>(row * kGridCols + col);
            samples[idx] = GridSample{
                px,
                py,
                clampByte(outR * outA),
                clampByte(outG * outA),
                clampByte(outB * outA),
                clampByte(outA)
            };
        }
    }

    int writeIdx = 0;
    for (int row = 0; row < kGridRows - 1; ++row) {
        for (int col = 0; col < kGridCols - 1; ++col) {
            const auto& s00 = samples[static_cast<size_t>(row * kGridCols + col)];
            const auto& s10 = samples[static_cast<size_t>(row * kGridCols + col + 1)];
            const auto& s01 = samples[static_cast<size_t>((row + 1) * kGridCols + col)];
            const auto& s11 = samples[static_cast<size_t>((row + 1) * kGridCols + col + 1)];

            v[writeIdx + 0].set(s00.x, s00.y, s00.r, s00.g, s00.b, s00.a);
            v[writeIdx + 1].set(s10.x, s10.y, s10.r, s10.g, s10.b, s10.a);
            v[writeIdx + 2].set(s01.x, s01.y, s01.r, s01.g, s01.b, s01.a);

            v[writeIdx + 3].set(s10.x, s10.y, s10.r, s10.g, s10.b, s10.a);
            v[writeIdx + 4].set(s11.x, s11.y, s11.r, s11.g, s11.b, s11.a);
            v[writeIdx + 5].set(s01.x, s01.y, s01.r, s01.g, s01.b, s01.a);
            writeIdx += 6;
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace sonora
