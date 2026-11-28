#include "ui/IconImageProvider.h"

#include <QHash>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace virela {

namespace {

constexpr int kDefaultIconPx = 32;
constexpr int kMinIconPx = 12;
constexpr int kMaxIconPx = 128;
constexpr int kMaxCacheBytes = 2 * 1024 * 1024;

const QHash<QString, QString>& iconPathTable() {
    static const QHash<QString, QString> kTable = {
        {QStringLiteral("play"),             QStringLiteral("M 7 4.8 L 19.2 12 L 7 19.2 Z")},
        {QStringLiteral("pause"),            QStringLiteral("M 6.5 5 H 10.2 V 19 H 6.5 Z M 13.8 5 H 17.5 V 19 H 13.8 Z")},
        {QStringLiteral("next"),             QStringLiteral("M 5 5.5 L 14.5 12 L 5 18.5 Z M 18 5.5 V 18.5")},
        {QStringLiteral("prev"),             QStringLiteral("M 19 5.5 L 9.5 12 L 19 18.5 Z M 6 5.5 V 18.5")},
        {QStringLiteral("shuffle"),          QStringLiteral("M 16 3 H 21 V 8 M 4 20 L 21 3 M 21 16 V 21 H 16 M 15 15 L 21 21 M 4 4 L 9 9")},
        {QStringLiteral("repeat"),           QStringLiteral("M 17 2 L 21 6 L 17 10 M 3 11 V 9 A 4 4 0 0 1 7 5 H 21 M 7 22 L 3 18 L 7 14 M 21 13 V 15 A 4 4 0 0 1 17 19 H 3")},
        {QStringLiteral("repeat_one"),       QStringLiteral("M 17 2 L 21 6 L 17 10 M 3 11 V 9 A 4 4 0 0 1 7 5 H 21 M 7 22 L 3 18 L 7 14 M 21 13 V 15 A 4 4 0 0 1 17 19 H 3 M 12 9 V 15")},
        {QStringLiteral("volume"),           QStringLiteral("M 11 5 L 6 9 H 2 V 15 H 6 L 11 19 Z M 15.5 8.5 A 5 5 0 0 1 15.5 15.5 M 18.5 5.5 A 9 9 0 0 1 18.5 18.5")},
        {QStringLiteral("volume_mute"),      QStringLiteral("M 11 5 L 6 9 H 2 V 15 H 6 L 11 19 Z M 22 9 L 16 15 M 16 9 L 22 15")},
        {QStringLiteral("queue"),            QStringLiteral("M 3 6 H 21 M 3 12 H 15 M 3 18 H 12 M 17 14 L 21 17 L 17 20 Z")},
        {QStringLiteral("lyrics"),           QStringLiteral("M 4 6 H 20 M 4 11 H 16 M 4 16 H 12 M 17 14 V 19 A 2 2 0 1 1 15 17 H 17 V 13 L 21 12")},
        {QStringLiteral("eq"),               QStringLiteral("M 4 20 V 14 M 4 10 V 4 M 12 20 V 12 M 12 8 V 4 M 20 20 V 16 M 20 12 V 4 M 2 14 H 6 M 10 8 H 14 M 18 16 H 22")},
        {QStringLiteral("music"),            QStringLiteral("M 9 18 V 5 L 21 3 V 16 M 6 18 A 3 3 0 1 0 9 18 A 3 3 0 1 0 6 18 M 18 16 A 3 3 0 1 0 21 16 A 3 3 0 1 0 18 16")},
        {QStringLiteral("album"),            QStringLiteral("M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 9 A 3 3 0 1 0 12 15 A 3 3 0 1 0 12 9")},
        {QStringLiteral("artist"),           QStringLiteral("M 12 11 A 4 4 0 1 0 12 3 A 4 4 0 1 0 12 11 M 4 21 A 8 8 0 0 1 20 21")},
        {QStringLiteral("folder"),           QStringLiteral("M 3 6 A 2 2 0 0 1 5 4 H 9.5 L 11.5 6.5 H 19 A 2 2 0 0 1 21 8.5 V 18 A 2 2 0 0 1 19 20 H 5 A 2 2 0 0 1 3 18 Z")},
        {QStringLiteral("heart"),            QStringLiteral("M 12 20.5 C 12 20.5 4 15.2 4 9.2 A 4.5 4.5 0 0 1 12 6.4 A 4.5 4.5 0 0 1 20 9.2 C 20 15.2 12 20.5 12 20.5 Z")},
        {QStringLiteral("heart_filled"),     QStringLiteral("M 12 20.5 C 12 20.5 4 15.2 4 9.2 A 4.5 4.5 0 0 1 12 6.4 A 4.5 4.5 0 0 1 20 9.2 C 20 15.2 12 20.5 12 20.5 Z")},
        {QStringLiteral("clock"),            QStringLiteral("M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 7 V 12 L 15.5 14")},
        {QStringLiteral("playlist"),         QStringLiteral("M 3 6 H 17 M 3 11 H 17 M 3 16 H 11 M 17 13 V 21 M 13 17 H 21")},
        {QStringLiteral("search"),           QStringLiteral("M 11 4 A 7 7 0 1 0 11 18 A 7 7 0 1 0 11 4 M 20 20 L 16.2 16.2")},
        {QStringLiteral("settings"),         QStringLiteral("M 9.8 5.6 L 10.4 2.7 L 13.6 2.7 L 14.2 5.6 A 6.8 6.8 0 0 1 16.5 6.9 L 19.2 6.0 L 20.8 8.8 L 18.7 10.7 A 6.8 6.8 0 0 1 18.7 13.3 L 20.8 15.2 L 19.2 18.0 L 16.5 17.1 A 6.8 6.8 0 0 1 14.2 18.4 L 13.6 21.3 L 10.4 21.3 L 9.8 18.4 A 6.8 6.8 0 0 1 7.5 17.1 L 4.8 18.0 L 3.2 15.2 L 5.3 13.3 A 6.8 6.8 0 0 1 5.3 10.7 L 3.2 8.8 L 4.8 6.0 L 7.5 6.9 A 6.8 6.8 0 0 1 9.8 5.6 Z M 12 9 A 3 3 0 1 0 12 15 A 3 3 0 1 0 12 9")},
        {QStringLiteral("sun"),              QStringLiteral("M 12 8 A 4 4 0 1 0 12 16 A 4 4 0 1 0 12 8 M 12 2 V 4 M 12 20 V 22 M 4.9 4.9 L 6.3 6.3 M 17.7 17.7 L 19.1 19.1 M 2 12 H 4 M 20 12 H 22 M 4.9 19.1 L 6.3 17.7 M 17.7 6.3 L 19.1 4.9")},
        {QStringLiteral("moon"),             QStringLiteral("M 20 14.5 A 8.5 8.5 0 1 1 9.5 4 A 6.8 6.8 0 0 0 20 14.5 Z")},
        {QStringLiteral("mini"),             QStringLiteral("M 3 5 H 21 V 19 H 3 Z M 11 11 H 18 V 16 H 11 Z")},
        {QStringLiteral("minimize"),         QStringLiteral("M 5 12 H 19")},
        {QStringLiteral("maximize"),         QStringLiteral("M 5 5 H 19 V 19 H 5 Z")},
        {QStringLiteral("restore"),          QStringLiteral("M 5 9 H 15 V 19 H 5 Z M 9 9 V 5 H 19 V 15 H 15")},
        {QStringLiteral("close"),            QStringLiteral("M 6 6 L 18 18 M 18 6 L 6 18")},
        {QStringLiteral("plus"),             QStringLiteral("M 12 5 V 19 M 5 12 H 19")},
        {QStringLiteral("trash"),            QStringLiteral("M 4 7 H 20 M 10 11 V 17 M 14 11 V 17 M 6 7 L 7 20 H 17 L 18 7 M 9 7 V 4 H 15 V 7")},
        {QStringLiteral("info"),             QStringLiteral("M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 11 V 16 M 12 8 V 8.2")},
        {QStringLiteral("chevron_down"),     QStringLiteral("M 6 9 L 12 15 L 18 9")},
        {QStringLiteral("chevron_up"),       QStringLiteral("M 6 15 L 12 9 L 18 15")},
        {QStringLiteral("chevron_left"),     QStringLiteral("M 15 6 L 9 12 L 15 18")},
        {QStringLiteral("chevron_right"),    QStringLiteral("M 9 6 L 15 12 L 9 18")},
        {QStringLiteral("check"),            QStringLiteral("M 5 12.5 L 9.8 17.2 L 19 7.5")},
        {QStringLiteral("wave"),             QStringLiteral("M 3 12 H 6 L 9 5 L 13 19 L 16 10 L 18 12 H 21")},
        {QStringLiteral("import"),           QStringLiteral("M 12 3 V 15 M 7 10 L 12 15 L 17 10 M 4 19 H 20")},
        {QStringLiteral("export"),           QStringLiteral("M 12 15 V 3 M 7 8 L 12 3 L 17 8 M 4 19 H 20")},
        {QStringLiteral("sidebar_expand"),   QStringLiteral("M 4 5 H 20 V 19 H 4 Z M 9 5 V 19 M 13 9.5 L 15.8 12 L 13 14.5")},
        {QStringLiteral("sidebar_collapse"), QStringLiteral("M 4 5 H 20 V 19 H 4 Z M 9 5 V 19 M 15.8 9.5 L 13 12 L 15.8 14.5")},
        {QStringLiteral("stage_panel"),      QStringLiteral("M 3 5 H 21 V 19 H 3 Z M 14.5 5 V 19")},
        {QStringLiteral("expand"),           QStringLiteral("M 14 4 H 20 V 10 M 20 4 L 13.5 10.5 M 10 20 H 4 V 14 M 4 20 L 10.5 13.5")},
        {QStringLiteral("collapse"),         QStringLiteral("M 4 14 H 10 V 20 M 10 14 L 3.5 20.5 M 20 10 H 14 V 4 M 14 10 L 20.5 3.5")},
        {QStringLiteral("cloud"),            QStringLiteral("M 7 18 H 17.5 A 4.2 4.2 0 0 0 18.2 9.7 A 5.5 5.5 0 0 0 7.5 8.5 A 4.5 4.5 0 0 0 7 18 Z")},
        {QStringLiteral("download"),         QStringLiteral("M 12 4 V 14.5 M 7.5 10 L 12 14.5 L 16.5 10 M 4 19 H 20")},
        {QStringLiteral("link"),             QStringLiteral("M 10 13.5 A 4 4 0 0 0 15.6 13.5 L 18.5 10.6 A 4 4 0 0 0 12.8 4.9 L 11.2 6.5 M 14 10.5 A 4 4 0 0 0 8.4 10.5 L 5.5 13.4 A 4 4 0 0 0 11.2 19.1 L 12.8 17.5")},
        {QStringLiteral("play_next"),        QStringLiteral("M 4 6 L 12 11 L 4 16 Z M 14 8 H 21 M 14 13 H 21 M 14 18 H 19")},
        {QStringLiteral("refresh"),          QStringLiteral("M 20 5 V 10 H 15 M 4 19 V 14 H 9 M 5.6 9 A 7 7 0 0 1 18.5 7.5 M 18.4 15 A 7 7 0 0 1 5.5 16.5")},
        {QStringLiteral("eye"),              QStringLiteral("M 2 12 C 4.8 7.5 8.2 5.5 12 5.5 C 15.8 5.5 19.2 7.5 22 12 C 19.2 16.5 15.8 18.5 12 18.5 C 8.2 18.5 4.8 16.5 2 12 Z M 12 9 A 3 3 0 1 0 12 15 A 3 3 0 1 0 12 9")},
        {QStringLiteral("eye_off"),          QStringLiteral("M 3 3 L 21 21 M 10.5 5.7 A 8.5 8.5 0 0 1 22 12 C 20.8 14 19.2 15.5 17.4 16.6 M 14.2 17.9 A 9 9 0 0 1 2 12 C 3.5 9.5 5.4 7.7 7.6 6.7")}
    };
    return kTable;
}

QString pathForIconName(const QString& name) {
    const auto& table = iconPathTable();
    const auto it = table.constFind(name);
    if (it != table.constEnd()) {
        return it.value();
    }
    return QStringLiteral("M 9 18 V 5 L 21 3 V 16");
}

// Convert SVG elliptical arc endpoint parameterization to Qt arcTo (W3C SVG 1.1 Appendix F.6.5).
void appendSvgArc(QPainterPath& path,
                  double x1, double y1,
                  double rx, double ry,
                  double xAxisRotDeg,
                  bool largeArc,
                  bool sweep,
                  double x2, double y2) {
    if (std::abs(x1 - x2) < 1e-6 && std::abs(y1 - y2) < 1e-6) {
        return;
    }
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx < 1e-6 || ry < 1e-6) {
        path.lineTo(x2, y2);
        return;
    }

    constexpr double kPi = std::numbers::pi_v<double>;
    const double phi = xAxisRotDeg * (kPi / 180.0);
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);

    const double dx2 = (x1 - x2) * 0.5;
    const double dy2 = (y1 - y2) * 0.5;
    const double x1p = cosPhi * dx2 + sinPhi * dy2;
    const double y1p = -sinPhi * dx2 + cosPhi * dy2;

    double rxSq = rx * rx;
    double rySq = ry * ry;
    const double x1pSq = x1p * x1p;
    const double y1pSq = y1p * y1p;

    const double lambda = (x1pSq / rxSq) + (y1pSq / rySq);
    if (lambda > 1.0) {
        const double s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
        rxSq = rx * rx;
        rySq = ry * ry;
    }

    const double num = rxSq * rySq - rxSq * y1pSq - rySq * x1pSq;
    const double den = rxSq * y1pSq + rySq * x1pSq;
    const double factor = (den > 1e-12) ? std::sqrt(std::max(0.0, num / den)) : 0.0;
    const double sign = (largeArc == sweep) ? -1.0 : 1.0;
    const double cxp = sign * factor * ((rx * y1p) / ry);
    const double cyp = sign * factor * (-(ry * x1p) / rx);

    const double cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) * 0.5;
    const double cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) * 0.5;

    auto vectorAngle = [](double ux, double uy, double vx, double vy) -> double {
        const double dot = ux * vx + uy * vy;
        const double len = std::hypot(ux, uy) * std::hypot(vx, vy);
        if (len < 1e-12) return 0.0;
        const double ratio = std::clamp(dot / len, -1.0, 1.0);
        const double ang = std::acos(ratio);
        return ((ux * vy - uy * vx) < 0.0) ? -ang : ang;
    };

    const double ux = (x1p - cxp) / rx;
    const double uy = (y1p - cyp) / ry;
    const double vx = (-x1p - cxp) / rx;
    const double vy = (-y1p - cyp) / ry;

    const double theta1 = vectorAngle(1.0, 0.0, ux, uy);
    double dtheta = vectorAngle(ux, uy, vx, vy);
    if (!sweep && dtheta > 0.0) {
        dtheta -= 2.0 * kPi;
    } else if (sweep && dtheta < 0.0) {
        dtheta += 2.0 * kPi;
    }

    const QRectF rect(cx - rx, cy - ry, 2.0 * rx, 2.0 * ry);
    // Both SVG and Qt use y-down coordinates, but SVG angles increase clockwise whereas
    // QPainterPath::arcTo expects counter-clockwise angles, so negate both angles.
    const double qtStartDeg = -theta1 * (180.0 / kPi);
    const double qtSweepDeg = -dtheta * (180.0 / kPi);

    if (std::abs(xAxisRotDeg) < 1e-4) {
        path.arcTo(rect, qtStartDeg, qtSweepDeg);
        return;
    }

    QPainterPath localArc;
    localArc.moveTo(x1, y1);
    QTransform xf;
    xf.translate(cx, cy);
    xf.rotate(xAxisRotDeg);
    xf.translate(-cx, -cy);
    localArc = xf.inverted().map(localArc);
    localArc.arcMoveTo(rect, qtStartDeg);
    localArc.arcTo(rect, qtStartDeg, qtSweepDeg);
    localArc = xf.map(localArc);
    path.connectPath(localArc);
}

QPainterPath parseSvgPath(const QString& d) {
    QPainterPath path;
    const qsizetype len = d.size();
    qsizetype i = 0;

    auto skipWsComma = [&]() {
        while (i < len) {
            const QChar ch = d.at(i);
            if (ch.isSpace() || ch == QLatin1Char(',')) {
                ++i;
            } else {
                break;
            }
        }
    };

    auto hasNumber = [&]() -> bool {
        skipWsComma();
        if (i >= len) return false;
        const QChar ch = d.at(i);
        return ch.isDigit() || ch == QLatin1Char('-') || ch == QLatin1Char('+') || ch == QLatin1Char('.');
    };

    auto readNumber = [&](bool& ok) -> double {
        skipWsComma();
        const qsizetype start = i;
        if (i < len && (d.at(i) == QLatin1Char('+') || d.at(i) == QLatin1Char('-'))) {
            ++i;
        }
        bool seenDot = false;
        while (i < len) {
            const QChar ch = d.at(i);
            if (ch.isDigit()) {
                ++i;
            } else if (ch == QLatin1Char('.') && !seenDot) {
                seenDot = true;
                ++i;
            } else {
                break;
            }
        }
        if (i == start) {
            ok = false;
            return 0.0;
        }
        return d.mid(start, i - start).toDouble(&ok);
    };

    auto readFlag = [&](bool& ok) -> int {
        skipWsComma();
        if (i >= len) {
            ok = false;
            return 0;
        }
        const QChar ch = d.at(i);
        if (ch == QLatin1Char('0') || ch == QLatin1Char('1')) {
            ++i;
            ok = true;
            return ch == QLatin1Char('1') ? 1 : 0;
        }
        ok = false;
        return 0;
    };

    QChar cmd = QLatin1Char('\0');
    double curX = 0.0;
    double curY = 0.0;
    double subStartX = 0.0;
    double subStartY = 0.0;

    while (i < len) {
        skipWsComma();
        if (i >= len) break;
        const QChar ch = d.at(i);
        if (ch.isLetter()) {
            cmd = ch;
            ++i;
            if (cmd == QLatin1Char('Z') || cmd == QLatin1Char('z')) {
                path.closeSubpath();
                curX = subStartX;
                curY = subStartY;
                continue;
            }
        } else if (cmd == QLatin1Char('\0')) {
            break;
        }

        const bool rel = cmd.isLower();
        const QChar upper = cmd.toUpper();
        bool ok = true;

        if (upper == QLatin1Char('M')) {
            const double nx = readNumber(ok);
            const double ny = ok ? readNumber(ok) : 0.0;
            if (!ok) break;
            curX = rel ? (curX + nx) : nx;
            curY = rel ? (curY + ny) : ny;
            subStartX = curX;
            subStartY = curY;
            path.moveTo(curX, curY);
            // Implicit subsequent coordinate pairs after M are treated as L
            cmd = rel ? QLatin1Char('l') : QLatin1Char('L');
        } else if (upper == QLatin1Char('L')) {
            while (hasNumber()) {
                const double nx = readNumber(ok);
                const double ny = ok ? readNumber(ok) : 0.0;
                if (!ok) break;
                curX = rel ? (curX + nx) : nx;
                curY = rel ? (curY + ny) : ny;
                path.lineTo(curX, curY);
            }
        } else if (upper == QLatin1Char('H')) {
            while (hasNumber()) {
                const double nx = readNumber(ok);
                if (!ok) break;
                curX = rel ? (curX + nx) : nx;
                path.lineTo(curX, curY);
            }
        } else if (upper == QLatin1Char('V')) {
            while (hasNumber()) {
                const double ny = readNumber(ok);
                if (!ok) break;
                curY = rel ? (curY + ny) : ny;
                path.lineTo(curX, curY);
            }
        } else if (upper == QLatin1Char('C')) {
            while (hasNumber()) {
                const double c1x = readNumber(ok);
                const double c1y = ok ? readNumber(ok) : 0.0;
                const double c2x = ok ? readNumber(ok) : 0.0;
                const double c2y = ok ? readNumber(ok) : 0.0;
                const double nx = ok ? readNumber(ok) : 0.0;
                const double ny = ok ? readNumber(ok) : 0.0;
                if (!ok) break;
                const double ax1 = rel ? (curX + c1x) : c1x;
                const double ay1 = rel ? (curY + c1y) : c1y;
                const double ax2 = rel ? (curX + c2x) : c2x;
                const double ay2 = rel ? (curY + c2y) : c2y;
                const double ax = rel ? (curX + nx) : nx;
                const double ay = rel ? (curY + ny) : ny;
                path.cubicTo(ax1, ay1, ax2, ay2, ax, ay);
                curX = ax;
                curY = ay;
            }
        } else if (upper == QLatin1Char('A')) {
            while (hasNumber()) {
                const double rx = readNumber(ok);
                const double ry = ok ? readNumber(ok) : 0.0;
                const double rot = ok ? readNumber(ok) : 0.0;
                const int largeArc = ok ? readFlag(ok) : 0;
                const int sweep = ok ? readFlag(ok) : 0;
                const double nx = ok ? readNumber(ok) : 0.0;
                const double ny = ok ? readNumber(ok) : 0.0;
                if (!ok) break;
                const double ax = rel ? (curX + nx) : nx;
                const double ay = rel ? (curY + ny) : ny;
                appendSvgArc(path, curX, curY, rx, ry, rot, largeArc != 0, sweep != 0, ax, ay);
                curX = ax;
                curY = ay;
            }
        } else {
            break;
        }
    }

    return path;
}

} // namespace

IconImageProvider::IconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
    cache_.setMaxCost(kMaxCacheBytes);
}

QStringList IconImageProvider::allIconNames() {
    QStringList names = iconPathTable().keys();
    names.sort();
    return names;
}

QImage IconImageProvider::renderIcon(const QString& name,
                                     int pixelSize,
                                     const QColor& color,
                                     qreal strokeWidth,
                                     bool filled) {
    const int px = std::clamp(pixelSize, kMinIconPx, kMaxIconPx);
    const QColor ink = color.isValid() ? color : QColor(Qt::white);
    const qreal clampedStroke = std::clamp(strokeWidth, 0.5, 6.0);

    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const QPainterPath path = parseSvgPath(pathForIconName(name));
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(static_cast<qreal>(px) / 24.0, static_cast<qreal>(px) / 24.0);

    if (filled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawPath(path);
    } else {
        QPen pen(ink, clampedStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    return img;
}

QImage IconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const int qIdx = id.indexOf(QLatin1Char('?'));
    const QString name = (qIdx >= 0 ? id.left(qIdx) : id).trimmed();
    const QUrlQuery query(qIdx >= 0 ? id.mid(qIdx + 1) : QString());

    int px = query.hasQueryItem(QStringLiteral("s"))
        ? query.queryItemValue(QStringLiteral("s")).toInt()
        : kDefaultIconPx;
    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        px = std::max(requestedSize.width(), requestedSize.height());
    }
    px = std::clamp(px, kMinIconPx, kMaxIconPx);

    QColor color(Qt::white);
    if (query.hasQueryItem(QStringLiteral("c"))) {
        QString hex = query.queryItemValue(QStringLiteral("c")).trimmed();
        if (!hex.startsWith(QLatin1Char('#'))) {
            hex.prepend(QLatin1Char('#'));
        }
        const QColor parsed = QColor::fromString(hex);
        if (parsed.isValid()) {
            color = parsed;
        }
    }

    qreal strokeWidth = 1.8;
    if (query.hasQueryItem(QStringLiteral("sw"))) {
        bool ok = false;
        const int tenths = query.queryItemValue(QStringLiteral("sw")).toInt(&ok);
        if (ok && tenths > 0) {
            strokeWidth = static_cast<qreal>(tenths) / 10.0;
        }
    }

    const bool filled = query.queryItemValue(QStringLiteral("f")) == QStringLiteral("1");
    const QString key = QStringLiteral("%1|%2|%3|%4|%5")
                            .arg(name)
                            .arg(px)
                            .arg(color.name(QColor::HexArgb))
                            .arg(qRound(strokeWidth * 10.0))
                            .arg(filled ? 1 : 0);

    {
        QMutexLocker locker(&cacheMutex_);
        if (const QImage* hit = cache_.object(key)) {
            if (size) *size = hit->size();
            return *hit;
        }
    }

    QImage rendered = renderIcon(name, px, color, strokeWidth, filled);
    if (size) *size = rendered.size();

    {
        QMutexLocker locker(&cacheMutex_);
        const int cost = std::max<int>(1, static_cast<int>(rendered.sizeInBytes()));
        cache_.insert(key, new QImage(rendered), cost);
    }

    return rendered;
}

} // namespace virela
