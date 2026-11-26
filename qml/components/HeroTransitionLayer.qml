import QtQuick
import QtQuick.Effects
import Sonora.UI

// Global shared-element Hero transition layer for seamless cover & spinning vinyl morphing.
// Reuses existing cover image URLs with zero extra RAM and supports safe mid-flight interruption.
Item {
    id: root

    property bool active: false
    property string activeTag: ""

    property string imageUrl: ""
    property bool vinylMode: false
    property real startRotation: 0.0
    property real spinSpeed: 0.0

    property real fromX: 0
    property real fromY: 0
    property real fromW: 0
    property real fromH: 0
    property real fromRadius: DesignTokens.radiusMd

    property real toX: 0
    property real toY: 0
    property real toW: 0
    property real toH: 0
    property real toRadius: DesignTokens.radiusXl

    property real progress: 0.0
    property int animDurationMs: DesignTokens.animHero

    readonly property real currentRotation: (root.startRotation + root.spinSpeed * (root.progress * root.animDurationMs / 1000.0)) % 360.0

    signal completed(string tag)

    visible: active
    enabled: false // Never blocks user mouse/wheel input

    function isConcealing(tag) {
        return active && activeTag === tag
    }

    function cancel() {
        heroAnim.stop()
        active = false
        activeTag = ""
        progress = 0.0
    }

    function startHero(params) {
        if (!params || !params.toRect) {
            cancel()
            return
        }

        const tag = params.tag || "hero"
        let sx = 0
        let sy = 0
        let sw = 0
        let sh = 0
        let sr = params.fromRadius !== undefined ? params.fromRadius : DesignTokens.radiusMd
        let sRot = params.startRotation !== undefined ? params.startRotation : 0.0

        // If interrupting an in-flight hero of the same tag, pick up smoothly from current proxy coordinates & angle
        if (active && activeTag === tag && proxyCard.width > 4 && proxyCard.height > 4) {
            sx = proxyCard.x
            sy = proxyCard.y
            sw = proxyCard.width
            sh = proxyCard.height
            sr = proxyCard.radius
            if (vinylMode) {
                sRot = root.currentRotation
            }
        } else if (params.sourceItem) {
            const mapped = params.sourceItem.mapToItem(root, 0, 0)
            sx = mapped.x
            sy = mapped.y
            sw = params.sourceItem.width
            sh = params.sourceItem.height
        } else if (params.fromRect) {
            sx = params.fromRect.x
            sy = params.fromRect.y
            sw = params.fromRect.width
            sh = params.fromRect.height
        }

        const tr = params.toRect
        if (sw <= 4 || sh <= 4 || tr.width <= 4 || tr.height <= 4) {
            cancel()
            return
        }

        heroAnim.stop()
        vinylMode = Boolean(params.vinylMode)
        startRotation = sRot
        spinSpeed = params.spinSpeed !== undefined ? params.spinSpeed : 0.0

        fromX = sx
        fromY = sy
        fromW = sw
        fromH = sh
        fromRadius = sr

        toX = tr.x
        toY = tr.y
        toW = tr.width
        toH = tr.height
        toRadius = params.toRadius !== undefined ? params.toRadius : DesignTokens.radiusXl

        imageUrl = params.imageUrl || ""
        activeTag = tag
        animDurationMs = params.duration !== undefined ? params.duration : DesignTokens.animHero
        progress = 0.0
        active = true

        heroAnim.duration = animDurationMs
        heroAnim.restart()
    }

    NumberAnimation {
        id: heroAnim
        target: root
        property: "progress"
        from: 0.0
        to: 1.0
        duration: DesignTokens.animHero
        easing.type: Easing.OutExpo
        onFinished: {
            const finishedTag = root.activeTag
            root.active = false
            root.activeTag = ""
            root.completed(finishedTag)
        }
    }

    // Soft mid-flight elevation shadow ring
    Rectangle {
        readonly property real lift: Math.sin(Math.max(0.0, Math.min(1.0, root.progress)) * Math.PI)
        x: proxyCard.x - 6 * lift
        y: proxyCard.y - 2 * lift
        width: proxyCard.width + 12 * lift
        height: proxyCard.height + 14 * lift
        radius: root.vinylMode ? width * 0.5 : (proxyCard.radius + 6 * lift)
        color: "transparent"
        border.width: Math.max(1, 6 * lift)
        border.color: Qt.rgba(0, 0, 0, (DesignTokens.isDark ? 0.34 : 0.14) * lift)
        opacity: lift
    }

    Item {
        id: proxyCard
        readonly property real t: root.progress
        readonly property real radius: root.fromRadius + (root.toRadius - root.fromRadius) * t
        x: root.fromX + (root.toX - root.fromX) * t
        y: root.fromY + (root.toY - root.fromY) * t
        width: root.fromW + (root.toW - root.fromW) * t
        height: root.fromH + (root.toH - root.fromH) * t

        // Spinning Vinyl LP proxy
        VinylDiscItem {
            anchors.fill: parent
            visible: root.vinylMode
            coverUrl: root.imageUrl
            playing: root.spinSpeed > 0.1
            discRotation: root.currentRotation
            ownSpin: false
            compact: width < 84
            hovered: true
        }

        // Standard Rounded Card proxy (non-vinyl)
        Rectangle {
            anchors.fill: parent
            visible: !root.vinylMode
            radius: proxyCard.radius
            color: DesignTokens.surfaceCard
            clip: true

            Rectangle {
                id: heroMask
                anchors.fill: parent
                radius: proxyCard.radius
                visible: false
                layer.enabled: true
                layer.smooth: true
            }

            Image {
                id: heroImg
                anchors.fill: parent
                source: root.imageUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                visible: false
            }

            MultiEffect {
                anchors.fill: heroImg
                visible: !root.vinylMode && heroImg.status === Image.Ready
                source: heroImg
                maskEnabled: true
                maskSource: heroMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }

            IconGlyph {
                anchors.centerIn: parent
                name: "music"
                size: Math.min(72, Math.max(18, Math.min(proxyCard.width, proxyCard.height) * 0.34))
                color: DesignTokens.textTertiary
                visible: heroImg.status !== Image.Ready
            }

            Rectangle {
                anchors.fill: parent
                radius: proxyCard.radius
                color: "transparent"
                border.width: 1
                border.color: DesignTokens.borderHighlight
            }
        }
    }
}
