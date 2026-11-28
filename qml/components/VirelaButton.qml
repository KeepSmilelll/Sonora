import QtQuick
import QtQuick.Layouts
import Virela.UI

Item {
    id: root

    property string text: ""
    property string iconName: ""
    property bool primary: false
    property bool accentTinted: false
    property bool compact: false
    property bool flat: false
    property real iconSize: compact ? 13 : 14
    signal clicked()

    implicitHeight: compact ? 30 : 34
    implicitWidth: contentRow.implicitWidth + (text.length > 0 ? (compact ? 22 : 28) : (compact ? 14 : 18))
    opacity: enabled ? 1.0 : 0.45

    scale: !enabled ? 1.0 : (ma.pressed ? 0.94 : (ma.containsMouse ? 1.025 : 1.0))
    Behavior on scale {
        NumberAnimation { duration: DesignTokens.animFast; easing.type: Easing.OutBack; easing.overshoot: 1.1 }
    }

    // Soft ambient glow halo under primary accent button
    Rectangle {
        visible: root.primary && root.enabled
        anchors.fill: parent
        anchors.margins: -3
        anchors.topMargin: -1
        anchors.bottomMargin: -5
        radius: height / 2
        color: DesignTokens.accentGlow
        opacity: ma.containsMouse ? 0.85 : 0.45
        Behavior on opacity { NumberAnimation { duration: DesignTokens.animFast } }
    }

    Rectangle {
        id: pill
        anchors.fill: parent
        radius: height / 2
        border.width: root.primary || root.flat ? 0 : 1
        border.color: root.accentTinted
            ? Qt.rgba(DesignTokens.accent.r, DesignTokens.accent.g, DesignTokens.accent.b, 0.40)
            : (ma.containsMouse ? DesignTokens.borderHover : DesignTokens.borderOuter)

        color: {
            if (root.primary) return "transparent"
            if (root.flat) return ma.containsMouse ? DesignTokens.surfaceHover : "transparent"
            if (root.accentTinted) return ma.containsMouse ? DesignTokens.surfaceSelected : DesignTokens.accentSoft
            if (ma.pressed) return DesignTokens.surfacePressed
            if (ma.containsMouse) return DesignTokens.isDark ? DesignTokens.surfaceCardHover : Qt.rgba(0.935, 0.948, 0.980, 0.96)
            return DesignTokens.surfaceCard
        }
        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }

        // Primary coral-rose gradient fill
        Rectangle {
            visible: root.primary
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: ma.pressed ? Qt.darker(DesignTokens.accent, 1.12) : DesignTokens.accentLight }
                GradientStop { position: 1.0; color: ma.pressed ? Qt.darker(DesignTokens.accent, 1.22) : DesignTokens.accent }
            }
        }

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: 6

            IconGlyph {
                visible: root.iconName.length > 0
                name: root.iconName
                size: root.iconSize
                color: root.primary ? DesignTokens.playbackInk
                     : (root.accentTinted ? DesignTokens.accent : DesignTokens.textPrimary)
            }

            Text {
                visible: root.text.length > 0
                text: root.text
                color: root.primary ? DesignTokens.playbackInk
                     : (root.accentTinted ? DesignTokens.accent : DesignTokens.textPrimary)
                font.family: DesignTokens.fontFamily
                font.pixelSize: root.compact ? 12 : 13
                font.weight: Font.DemiBold
            }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
