#pragma once

#include <QColor>
#include <QQuickItem>

namespace sonora {

class PlaybackEngine;

// Real-time GPU frequency response curve renderer for the 10-band parametric EQ.
class EqCurveItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY styleChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY styleChanged)
    Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)

public:
    explicit EqCurveItem(QQuickItem* parent = nullptr);

    static void setSharedEngine(PlaybackEngine* engine);

    QColor lineColor() const { return lineColor_; }
    void setLineColor(const QColor& c);

    QColor gridColor() const { return gridColor_; }
    void setGridColor(const QColor& c);

    int revision() const { return revision_; }
    void setRevision(int r);

signals:
    void styleChanged();
    void revisionChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    static PlaybackEngine* sharedEngine_;
    QColor lineColor_{250, 72, 102};
    QColor gridColor_{255, 255, 255, 40};
    int revision_ = 0;
};

} // namespace sonora
