#pragma once

#include <QCache>
#include <QColor>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>
#include <QStringList>

namespace virela {

// Rasterizes Virela's crisp vector UI icons into HiDPI-cached ARGB32 QImages so Qt Quick
// automatically packs them into QSGAtlasTexture and renders all icons in a single draw batch
// instead of dozens of unbatched Shape.CurveRenderer nodes every frame.
class IconImageProvider final : public QQuickImageProvider {
public:
    IconImageProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    static QStringList allIconNames();
    static QImage renderIcon(const QString& name,
                             int pixelSize,
                             const QColor& color,
                             qreal strokeWidth = 1.8,
                             bool filled = false);

private:
    QMutex cacheMutex_;
    QCache<QString, QImage> cache_;
};

} // namespace virela
