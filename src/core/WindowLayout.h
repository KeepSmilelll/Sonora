#pragma once

#include <QRect>
#include <QSize>

#include <algorithm>
#include <cmath>

namespace virela {

inline constexpr int kDefaultMinWindowWidth = 840;
inline constexpr int kDefaultMinWindowHeight = 560;

struct InitialWindowGeometry {
    QRect geometry;
    QSize minimumSize;
};

inline InitialWindowGeometry computeInitialWindowGeometry(const QRect& availableGeometry) {
    const QRect work = availableGeometry.isValid()
        && availableGeometry.width() > 0 && availableGeometry.height() > 0
        ? availableGeometry : QRect(0, 0, 1280, 720);
    const int availW = work.width();
    const int availH = work.height();
    const int marginX = availW <= 1320 ? 32 : 48;
    const int marginY = availH <= 720 ? 36 : 48;
    const int maxW = std::max(320, availW - marginX);
    const int maxH = std::max(240, availH - marginY);
    const QSize minimumSize(std::min(kDefaultMinWindowWidth, maxW),
                            std::min(kDefaultMinWindowHeight, maxH));
    const int idealW = std::clamp(static_cast<int>(std::lround(availW * 0.75)), 1180, 1560);
    const int idealH = std::clamp(static_cast<int>(std::lround(availH * 0.84)), 628, 940);
    const int finalW = std::max(minimumSize.width(), std::min(maxW, idealW));
    const int finalH = std::max(minimumSize.height(), std::min(maxH, idealH));
    const int x = work.x() + (availW - finalW) / 2;
    const int y = work.y() + (availH - finalH) / 2;
    return {QRect(x, y, finalW, finalH), minimumSize};
}

} // namespace virela
