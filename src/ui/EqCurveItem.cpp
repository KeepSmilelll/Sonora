#include "ui/EqCurveItem.h"
#include "audio/PlaybackEngine.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <algorithm>
#include <cmath>

namespace sonora {

PlaybackEngine* EqCurveItem::sharedEngine_ = nullptr;

void EqCurveItem::setSharedEngine(PlaybackEngine* engine) {
    sharedEngine_ = engine;
}

EqCurveItem::EqCurveItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void EqCurveItem::setLineColor(const QColor& c) {
    if (lineColor_ == c) return;
    lineColor_ = c;
    emit styleChanged();
    update();
}

void EqCurveItem::setGridColor(const QColor& c) {
    if (gridColor_ == c) return;
    gridColor_ = c;
    emit styleChanged();
    update();
}

void EqCurveItem::setRevision(int r) {
    if (revision_ == r) return;
    revision_ = r;
    emit revisionChanged();
    update();
}

QSGNode* EqCurveItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    constexpr int kPoints = 96;
    auto* rootNode = oldNode;
    QSGGeometryNode* fillNode = nullptr;
    QSGGeometryNode* lineNode = nullptr;

    if (!rootNode) {
        rootNode = new QSGNode();

        fillNode = new QSGGeometryNode();
        auto* fillGeom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), kPoints * 2);
        fillGeom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        fillNode->setGeometry(fillGeom);
        fillNode->setFlag(QSGNode::OwnsGeometry);
        fillNode->setMaterial(new QSGVertexColorMaterial());
        fillNode->setFlag(QSGNode::OwnsMaterial);
        rootNode->appendChildNode(fillNode);

        lineNode = new QSGGeometryNode();
        auto* lineGeom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), kPoints * 2);
        lineGeom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        lineNode->setGeometry(lineGeom);
        lineNode->setFlag(QSGNode::OwnsGeometry);
        auto* mat = new QSGFlatColorMaterial();
        mat->setColor(lineColor_);
        lineNode->setMaterial(mat);
        lineNode->setFlag(QSGNode::OwnsMaterial);
        rootNode->appendChildNode(lineNode);
    } else {
        fillNode = static_cast<QSGGeometryNode*>(rootNode->firstChild());
        lineNode = static_cast<QSGGeometryNode*>(fillNode->nextSibling());
        static_cast<QSGFlatColorMaterial*>(lineNode->material())->setColor(lineColor_);
        lineNode->markDirty(QSGNode::DirtyMaterial);
    }

    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const float midY = h * 0.5f;

    auto* fv = fillNode->geometry()->vertexDataAsColoredPoint2D();
    auto* lv = lineNode->geometry()->vertexDataAsPoint2D();

    constexpr double kMinHz = 20.0;
    constexpr double kMaxHz = 20000.0;
    const double logRatio = std::log(kMaxHz / kMinHz);

    const float fillAlpha = 0.24f;
    const auto fr = static_cast<unsigned char>(lineColor_.red() * fillAlpha);
    const auto fg = static_cast<unsigned char>(lineColor_.green() * fillAlpha);
    const auto fb = static_cast<unsigned char>(lineColor_.blue() * fillAlpha);
    const auto fa = static_cast<unsigned char>(255 * fillAlpha);

    float xs[kPoints];
    float ys[kPoints];
    for (int i = 0; i < kPoints; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kPoints - 1);
        xs[i] = static_cast<float>(t) * w;
        double db = 0.0;
        if (sharedEngine_) {
            const double fHz = kMinHz * std::exp(logRatio * t);
            db = std::clamp(sharedEngine_->equalizer().magnitudeDbAt(fHz), -15.0, 15.0);
        }
        ys[i] = midY - static_cast<float>(db / 15.0) * (midY - 8.0f);
        fv[i * 2 + 0].set(xs[i], ys[i], fr, fg, fb, fa);
        fv[i * 2 + 1].set(xs[i], midY, 0, 0, 0, 0);
    }

    constexpr float kHalfStroke = 1.2f;
    for (int i = 0; i < kPoints; ++i) {
        const int i0 = std::max(0, i - 1);
        const int i1 = std::min(kPoints - 1, i + 1);
        float dx = xs[i1] - xs[i0];
        float dy = ys[i1] - ys[i0];
        const float len = std::max(1e-4f, std::hypot(dx, dy));
        const float nx = -dy / len * kHalfStroke;
        const float ny =  dx / len * kHalfStroke;
        lv[i * 2 + 0].set(xs[i] + nx, ys[i] + ny);
        lv[i * 2 + 1].set(xs[i] - nx, ys[i] - ny);
    }

    fillNode->markDirty(QSGNode::DirtyGeometry);
    lineNode->markDirty(QSGNode::DirtyGeometry);
    return rootNode;
}

} // namespace sonora
