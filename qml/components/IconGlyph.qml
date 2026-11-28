import QtQuick

// Cached HiDPI rasterized vector icon glyph rendered via QQuickImageProvider so Qt Quick
// batches all UI icons into QSGAtlasTexture with minimal draw calls.
Item {
    id: root

    property string name: "music"
    property color color: "#ffffff"
    property real size: 18
    property real strokeWidth: 1.8
    property bool filled: name === "play" || name === "pause" || name === "heart_filled"

    readonly property real _dpr: root.Screen.devicePixelRatio ? Math.max(1.0, root.Screen.devicePixelRatio) : 2.0
    readonly property int _px: Math.max(12, Math.min(128, Math.round(root.size * _dpr)))

    function _toHexByte(v) {
        const n = Math.max(0, Math.min(255, Math.round(v * 255)))
        return n.toString(16).padStart(2, "0")
    }

    readonly property string _hexColor: _toHexByte(root.color.a)
        + _toHexByte(root.color.r)
        + _toHexByte(root.color.g)
        + _toHexByte(root.color.b)

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Image {
        anchors.fill: parent
        source: root.name.length > 0
            ? ("image://icon/" + root.name
               + "?s=" + root._px
               + "&c=" + root._hexColor
               + "&sw=" + Math.round(root.strokeWidth * 10)
               + "&f=" + (root.filled ? 1 : 0))
            : ""
        sourceSize: Qt.size(root._px, root._px)
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false
        cache: true
    }
}
