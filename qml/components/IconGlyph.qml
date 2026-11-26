import QtQuick
import QtQuick.Shapes

// Resolution-independent GPU CurveRenderer vector icon glyph (zero Qt6::Svg, zero bitmap atlas bloat)
Item {
    id: root

    property string name: "music"
    property color color: "#ffffff"
    property real size: 18
    property real strokeWidth: 1.8
    property bool filled: name === "play" || name === "pause" || name === "heart_filled"

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Behavior on color {
        ColorAnimation { duration: 140; easing.type: Easing.OutQuad }
    }

    readonly property string pathData: {
        switch (name) {
        case "play":          return "M 7 4.8 L 19.2 12 L 7 19.2 Z"
        case "pause":         return "M 6.5 5 H 10.2 V 19 H 6.5 Z M 13.8 5 H 17.5 V 19 H 13.8 Z"
        case "next":          return "M 5 5.5 L 14.5 12 L 5 18.5 Z M 18 5.5 V 18.5"
        case "prev":          return "M 19 5.5 L 9.5 12 L 19 18.5 Z M 6 5.5 V 18.5"
        case "shuffle":       return "M 16 3 H 21 V 8 M 4 20 L 21 3 M 21 16 V 21 H 16 M 15 15 L 21 21 M 4 4 L 9 9"
        case "repeat":        return "M 17 2 L 21 6 L 17 10 M 3 11 V 9 A 4 4 0 0 1 7 5 H 21 M 7 22 L 3 18 L 7 14 M 21 13 V 15 A 4 4 0 0 1 17 19 H 3"
        case "repeat_one":    return "M 17 2 L 21 6 L 17 10 M 3 11 V 9 A 4 4 0 0 1 7 5 H 21 M 7 22 L 3 18 L 7 14 M 21 13 V 15 A 4 4 0 0 1 17 19 H 3 M 12 9 V 15"
        case "volume":        return "M 11 5 L 6 9 H 2 V 15 H 6 L 11 19 Z M 15.5 8.5 A 5 5 0 0 1 15.5 15.5 M 18.5 5.5 A 9 9 0 0 1 18.5 18.5"
        case "volume_mute":   return "M 11 5 L 6 9 H 2 V 15 H 6 L 11 19 Z M 22 9 L 16 15 M 16 9 L 22 15"
        case "queue":         return "M 3 6 H 21 M 3 12 H 15 M 3 18 H 12 M 17 14 L 21 17 L 17 20 Z"
        case "lyrics":        return "M 4 6 H 20 M 4 11 H 16 M 4 16 H 12 M 17 14 V 19 A 2 2 0 1 1 15 17 H 17 V 13 L 21 12"
        case "eq":            return "M 4 20 V 14 M 4 10 V 4 M 12 20 V 12 M 12 8 V 4 M 20 20 V 16 M 20 12 V 4 M 2 14 H 6 M 10 8 H 14 M 18 16 H 22"
        case "music":         return "M 9 18 V 5 L 21 3 V 16 M 6 18 A 3 3 0 1 0 9 18 A 3 3 0 1 0 6 18 M 18 16 A 3 3 0 1 0 21 16 A 3 3 0 1 0 18 16"
        case "album":         return "M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 9 A 3 3 0 1 0 12 15 A 3 3 0 1 0 12 9"
        case "artist":        return "M 12 11 A 4 4 0 1 0 12 3 A 4 4 0 1 0 12 11 M 4 21 A 8 8 0 0 1 20 21"
        case "folder":        return "M 3 6 A 2 2 0 0 1 5 4 H 9.5 L 11.5 6.5 H 19 A 2 2 0 0 1 21 8.5 V 18 A 2 2 0 0 1 19 20 H 5 A 2 2 0 0 1 3 18 Z"
        case "heart":         return "M 12 20.5 C 12 20.5 4 15.2 4 9.2 A 4.5 4.5 0 0 1 12 6.4 A 4.5 4.5 0 0 1 20 9.2 C 20 15.2 12 20.5 12 20.5 Z"
        case "heart_filled":  return "M 12 20.5 C 12 20.5 4 15.2 4 9.2 A 4.5 4.5 0 0 1 12 6.4 A 4.5 4.5 0 0 1 20 9.2 C 20 15.2 12 20.5 12 20.5 Z"
        case "clock":         return "M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 7 V 12 L 15.5 14"
        case "playlist":      return "M 3 6 H 17 M 3 11 H 17 M 3 16 H 11 M 17 13 V 21 M 13 17 H 21"
        case "search":        return "M 11 4 A 7 7 0 1 0 11 18 A 7 7 0 1 0 11 4 M 20 20 L 16.2 16.2"
        case "settings":      return "M 12 8.5 A 3.5 3.5 0 1 0 12 15.5 A 3.5 3.5 0 1 0 12 8.5 M 12 2 V 4.5 M 12 19.5 V 22 M 4.9 4.9 L 6.7 6.7 M 17.3 17.3 L 19.1 19.1 M 2 12 H 4.5 M 19.5 12 H 22 M 4.9 19.1 L 6.7 17.3 M 17.3 6.7 L 19.1 4.9"
        case "sun":           return "M 12 8 A 4 4 0 1 0 12 16 A 4 4 0 1 0 12 8 M 12 2 V 4 M 12 20 V 22 M 4.9 4.9 L 6.3 6.3 M 17.7 17.7 L 19.1 19.1 M 2 12 H 4 M 20 12 H 22 M 4.9 19.1 L 6.3 17.7 M 17.7 6.3 L 19.1 4.9"
        case "moon":          return "M 20 14.5 A 8.5 8.5 0 1 1 9.5 4 A 6.8 6.8 0 0 0 20 14.5 Z"
        case "mini":          return "M 3 5 H 21 V 19 H 3 Z M 11 11 H 18 V 16 H 11 Z"
        case "minimize":      return "M 5 12 H 19"
        case "maximize":      return "M 5 5 H 19 V 19 H 5 Z"
        case "restore":       return "M 5 9 H 15 V 19 H 5 Z M 9 9 V 5 H 19 V 15 H 15"
        case "close":         return "M 6 6 L 18 18 M 18 6 L 6 18"
        case "plus":          return "M 12 5 V 19 M 5 12 H 19"
        case "trash":         return "M 4 7 H 20 M 10 11 V 17 M 14 11 V 17 M 6 7 L 7 20 H 17 L 18 7 M 9 7 V 4 H 15 V 7"
        case "info":          return "M 12 3 A 9 9 0 1 0 12 21 A 9 9 0 1 0 12 3 M 12 11 V 16 M 12 8 V 8.2"
        case "chevron_down":  return "M 6 9 L 12 15 L 18 9"
        case "chevron_up":    return "M 6 15 L 12 9 L 18 15"
        case "chevron_left":  return "M 15 6 L 9 12 L 15 18"
        case "chevron_right": return "M 9 6 L 15 12 L 9 18"
        case "check":         return "M 5 12.5 L 9.8 17.2 L 19 7.5"
        case "wave":          return "M 3 12 H 6 L 9 5 L 13 19 L 16 10 L 18 12 H 21"
        case "import":        return "M 12 3 V 15 M 7 10 L 12 15 L 17 10 M 4 19 H 20"
        case "export":        return "M 12 15 V 3 M 7 8 L 12 3 L 17 8 M 4 19 H 20"
        default:              return "M 9 18 V 5 L 21 3 V 16"
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        transform: Scale {
            xScale: root.width / 24.0
            yScale: root.height / 24.0
        }

        ShapePath {
            strokeColor: root.filled ? "transparent" : root.color
            strokeWidth: root.filled ? 0 : root.strokeWidth
            fillColor: root.filled ? root.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.pathData
            }
        }
    }
}
