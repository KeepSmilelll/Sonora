pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects

Item {
    id: root

    signal finished()

    property bool active: false
    readonly property bool windowShown: Window.window !== null && Window.window.visible
        && Window.visibility !== Window.Hidden && Window.visibility !== Window.Minimized

    property var activeGrabResult: null
    property var pendingCommit: null
    property int captureSerial: 0
    property color substrateColor: "transparent"
    property real originX: 0
    property real originY: 0
    property real progress: 0
    readonly property real maxDist: Math.hypot(
        Math.max(originX, width - originX),
        Math.max(originY, height - originY))
    readonly property real waveRadius: maxDist * 1.15 * progress

    visible: snapshotImg.status === Image.Ready

    function clearTransition() {
        if (!active) return

        ++captureSerial
        captureTimeout.stop()
        reveal.stop()
        pendingCommit = null
        snapshotImg.source = ""
        activeGrabResult = null
        active = false
        finished()
    }

    function commitWithoutReveal() {
        const commit = pendingCommit
        clearTransition()
        if (commit) commit()
    }

    function startReveal(serial) {
        if (serial !== captureSerial || !pendingCommit || !visible) return
        const commit = pendingCommit
        captureTimeout.stop()
        reveal.start()
        pendingCommit = null
        commit()
    }

    function beginTransition(captureItem: Item, x: real, y: real, oldIsDark: bool,
                             oldAmbientPrimary: color, commitThemeCallback: var) {
        if (active) {
            clearTransition()
            commitThemeCallback()
            return
        }

        pendingCommit = commitThemeCallback
        active = true
        if (!windowShown || GraphicsInfo.api === GraphicsInfo.Software
                || !captureItem || captureItem.width <= 0 || captureItem.height <= 0) {
            commitWithoutReveal()
            return
        }

        progress = 0
        originX = Math.max(0, Math.min(width, x))
        originY = Math.max(0, Math.min(height, y))
        substrateColor = Qt.tint(oldIsDark ? "#11141d" : "#eef1f7",
            Qt.rgba(oldAmbientPrimary.r, oldAmbientPrimary.g, oldAmbientPrimary.b, oldIsDark ? 0.22 : 0.16))

        const serial = ++captureSerial
        const dpr = Math.max(1.0, Math.min(2.0, Screen.devicePixelRatio || 1.0))
        const size = Qt.size(Math.round(captureItem.width * dpr), Math.round(captureItem.height * dpr))
        captureTimeout.start()
        if (!captureItem.grabToImage(function(result) {
            if (!root || serial !== root.captureSerial) return
            if (!result || !result.url) {
                root.commitWithoutReveal()
                return
            }
            // Qt 6.9 deletes the grab QObject after the callback. Image holds its pixels
            // while its source is set, so the reveal depends on Image.Ready.
            root.activeGrabResult = result
            snapshotImg.source = result.url
        }, size)) {
            commitWithoutReveal()
        }
    }

    onWindowShownChanged: {
        if (!windowShown && active) commitWithoutReveal()
    }

    Connections {
        target: root.visible && root.pendingCommit ? root.Window.window : null

        function onFrameSwapped() {
            // The snapshot must cover the scene before changing the native DWM theme.
            Qt.callLater(root.startReveal, root.captureSerial)
        }
    }

    Timer {
        id: captureTimeout
        interval: 750
        onTriggered: root.commitWithoutReveal()
    }

    NumberAnimation {
        id: reveal
        target: root
        property: "progress"
        from: 0.0
        to: 1.0
        duration: 480
        easing.type: Easing.InOutCubic
        onFinished: root.clearTransition()
    }

    Item {
        id: sourceContainer
        anchors.fill: parent
        layer.enabled: root.visible
        visible: false

        Rectangle {
            anchors.fill: parent
            color: root.substrateColor
        }

        Image {
            id: snapshotImg
            anchors.fill: parent
            cache: false
            onStatusChanged: {
                if (status === Image.Error) root.commitWithoutReveal()
            }
        }
    }

    Item {
        id: maskContainer
        anchors.fill: parent
        layer.enabled: root.visible
        layer.textureSize: Qt.size(Math.max(128, Math.round(width * 0.35)),
                                   Math.max(128, Math.round(height * 0.35)))
        visible: false

        Repeater {
            model: [
                { radiusFactor: 1.00, alpha: 0.22 },
                { radiusFactor: 0.90, alpha: 0.34 },
                { radiusFactor: 0.78, alpha: 0.55 },
                { radiusFactor: 0.64, alpha: 1.00 }
            ]

            Rectangle {
                required property var modelData
                width: root.waveRadius * modelData.radiusFactor * 2
                height: width
                x: root.originX - radius
                y: root.originY - radius
                radius: width * 0.5
                color: Qt.rgba(1, 1, 1, modelData.alpha)
            }
        }
    }

    // Create the effect after its layer providers are enabled and release it with the snapshot.
    Loader {
        anchors.fill: parent
        active: root.visible
        sourceComponent: MultiEffect {
            source: sourceContainer
            autoPaddingEnabled: false
            maskEnabled: true
            maskSource: maskContainer
            maskInverted: true
            maskSpreadAtMin: 0.92
            // Qt expands the threshold range by the spread. Keep its lower edge at 0.04
            // and include alpha 1 so the center of the wave reveals the new scene fully.
            maskThresholdMin: (0.04 + maskSpreadAtMin) / (1.0 + maskSpreadAtMin)
            maskSpreadAtMax: 0.01
            opacity: root.progress > 0.78 ? 1.0 - (root.progress - 0.78) / 0.22 : 1.0
        }
    }

    Rectangle {
        width: root.waveRadius * 2
        height: width
        x: root.originX - radius
        y: root.originY - radius
        radius: width * 0.5
        color: "transparent"
        border.width: 1
        border.color: "#ffffff"
        opacity: 0.22 * Math.sin(Math.PI * root.progress)
    }
}
