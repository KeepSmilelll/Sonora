#include "ui/SpectrumVisualizerItem.h"
#include "audio/PlaybackEngine.h"

#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <algorithm>
#include <cmath>

namespace sonora {

PlaybackEngine* SpectrumVisualizerItem::sharedEngine_ = nullptr;

void SpectrumVisualizerItem::setSharedEngine(PlaybackEngine* engine) {
    sharedEngine_ = engine;
}

SpectrumVisualizerItem::SpectrumVisualizerItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    frameTimer_.setInterval(16); // ~60 fps only while actively visible & playing
    connect(&frameTimer_, &QTimer::timeout, this, &SpectrumVisualizerItem::onFrameTick);
}

void SpectrumVisualizerItem::setAccentColor(const QColor& c) {
    if (accentColor_ == c) return;
    accentColor_ = c;
    emit styleChanged();
    update();
}

void SpectrumVisualizerItem::setSecondaryColor(const QColor& c) {
    if (secondaryColor_ == c) return;
    secondaryColor_ = c;
    emit styleChanged();
    update();
}

void SpectrumVisualizerItem::setPlaying(bool p) {
    if (playing_ == p) return;
    playing_ = p;
    if (!playing_) {
        idleDecayFrames_ = 28; // Smooth fade-out to flat baseline then stop timer
    }
    emit activeChanged();
    updateTimerState();
}

void SpectrumVisualizerItem::setVisualizerEnabled(bool e) {
    if (visualizerEnabled_ == e) return;
    visualizerEnabled_ = e;
    emit activeChanged();
    updateTimerState();
    update();
}

void SpectrumVisualizerItem::setMirrored(bool m) {
    if (mirrored_ == m) return;
    mirrored_ = m;
    emit styleChanged();
    update();
}

void SpectrumVisualizerItem::itemChange(ItemChange change, const ItemChangeData& value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange) {
        updateTimerState();
    }
}

void SpectrumVisualizerItem::updateTimerState() {
    const bool visibleNow = isVisible() && visualizerEnabled_ && window() != nullptr;
    const bool shouldTick = visibleNow && (playing_ || idleDecayFrames_ > 0);

    if (sharedEngine_) {
        sharedEngine_->fftAnalyzer().setEnabled(visibleNow && playing_);
    }

    if (shouldTick) {
        if (!frameTimer_.isActive()) frameTimer_.start();
    } else {
        if (frameTimer_.isActive()) frameTimer_.stop();
    }
}

void SpectrumVisualizerItem::onFrameTick() {
    if (!sharedEngine_) {
        frameTimer_.stop();
        return;
    }
    sharedEngine_->fftAnalyzer().computeBands(bars_, peaks_, playing_);
    if (!playing_) {
        if (idleDecayFrames_ > 0) --idleDecayFrames_;
        if (idleDecayFrames_ <= 0) {
            updateTimerState();
        }
    }
    update();
}

QSGNode* SpectrumVisualizerItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    constexpr int kBands = static_cast<int>(FftAnalyzer::kBandCount);
    // Each band: 1 body quad (6 verts) + 1 peak cap quad (6 verts) = 12 vertices
    constexpr int kVertCount = kBands * 12;

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

    auto* geom = node->geometry();
    auto* v = geom->vertexDataAsColoredPoint2D();

    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    if (w <= 2.0f || h <= 2.0f || !visualizerEnabled_) {
        for (int i = 0; i < kVertCount; ++i) {
            v[i].set(0, 0, 0, 0, 0, 0);
        }
        node->markDirty(QSGNode::DirtyGeometry);
        return node;
    }

    const float step = w / static_cast<float>(kBands);
    const float gap = std::max(2.0f, step * 0.28f);
    const float barW = std::max(1.5f, step - gap);

    auto writeQuad = [&](int offset, float x0, float y0, float x1, float y1,
                         const QColor& topCol, const QColor& botCol) {
        // Premultiplied RGBA for Qt SceneGraph QSGVertexColorMaterial
        const auto premul = [](const QColor& c) {
            const float a = static_cast<float>(c.alphaF());
            return std::array<unsigned char, 4>{
                static_cast<unsigned char>(c.red() * a),
                static_cast<unsigned char>(c.green() * a),
                static_cast<unsigned char>(c.blue() * a),
                static_cast<unsigned char>(c.alpha())
            };
        };
        const auto tc = premul(topCol);
        const auto bc = premul(botCol);

        v[offset + 0].set(x0, y0, tc[0], tc[1], tc[2], tc[3]);
        v[offset + 1].set(x1, y0, tc[0], tc[1], tc[2], tc[3]);
        v[offset + 2].set(x0, y1, bc[0], bc[1], bc[2], bc[3]);

        v[offset + 3].set(x1, y0, tc[0], tc[1], tc[2], tc[3]);
        v[offset + 4].set(x1, y1, bc[0], bc[1], bc[2], bc[3]);
        v[offset + 5].set(x0, y1, bc[0], bc[1], bc[2], bc[3]);
    };

    for (int i = 0; i < kBands; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kBands - 1);
        QColor topCol(
            static_cast<int>(accentColor_.red() * (1.0f - t) + secondaryColor_.red() * t),
            static_cast<int>(accentColor_.green() * (1.0f - t) + secondaryColor_.green() * t),
            static_cast<int>(accentColor_.blue() * (1.0f - t) + secondaryColor_.blue() * t),
            225);
        QColor botCol = topCol;
        botCol.setAlpha(65);

        const float val = std::clamp(bars_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float peak = std::clamp(peaks_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float x0 = i * step + gap * 0.5f;
        const float x1 = x0 + barW;

        if (mirrored_) {
            const float mid = h * 0.5f;
            const float halfH = std::max(1.5f, val * mid * 0.92f);
            writeQuad(i * 12, x0, mid - halfH, x1, mid + halfH, topCol, topCol);
            // Zero out peak cap in mirrored mode
            writeQuad(i * 12 + 6, 0, 0, 0, 0, Qt::transparent, Qt::transparent);
        } else {
            const float barH = std::max(2.0f, val * (h - 6.0f));
            const float y0 = h - barH;
            const float y1 = h;
            writeQuad(i * 12, x0, y0, x1, y1, topCol, botCol);

            // Subtle peak hold cap
            const float peakY = h - std::max(2.0f, peak * (h - 6.0f)) - 3.0f;
            QColor capCol = topCol;
            capCol.setAlpha(peak > 0.02f ? 210 : 0);
            writeQuad(i * 12 + 6, x0, std::max(0.0f, peakY), x1, std::max(2.0f, peakY + 2.0f), capCol, capCol);
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace sonora
