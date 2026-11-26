import QtQuick
import Sonora.UI

// Spatial Fluid Glass container with dual-rim specular top highlight & zero-blur ambient shadow
Item {
    id: root

    default property alias contentData: body.data
    property real radius: DesignTokens.radiusLg
    property color panelColor: elevated ? DesignTokens.surfaceElevated : DesignTokens.surfaceCard
    property color borderColor: elevated ? DesignTokens.borderHighlight : DesignTokens.borderOuter
    property bool elevated: false
    property int elevationLevel: elevated ? 2 : 1
    property bool shadowEnabled: elevated
    property bool specularEnabled: true
    property bool borderless: false

    // Outer ambient shadow ring (zero GPU shader blur cost)
    Rectangle {
        visible: root.shadowEnabled
        anchors.fill: parent
        anchors.margins: -5
        anchors.topMargin: -2
        anchors.bottomMargin: -8
        radius: root.radius + 6
        color: "transparent"
        border.width: 5
        border.color: Qt.rgba(0, 0, 0, DesignTokens.isDark ? 0.20 : 0.055)
    }

    // Inner tighter contact shadow ring
    Rectangle {
        visible: root.shadowEnabled
        anchors.fill: parent
        anchors.margins: -1
        anchors.topMargin: 0
        anchors.bottomMargin: -3
        radius: root.radius + 2
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(0, 0, 0, DesignTokens.isDark ? 0.28 : 0.08)
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: root.radius
        color: root.panelColor
        border.width: root.borderless ? 0 : 1
        border.color: root.borderColor
        clip: true

        Behavior on color { ColorAnimation { duration: DesignTokens.animFast } }
        Behavior on border.color { ColorAnimation { duration: DesignTokens.animFast } }

        // Top 1px visionOS specular light reflection rim
        Rectangle {
            visible: root.specularEnabled
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Math.max(8, root.radius * 0.65)
            anchors.rightMargin: Math.max(8, root.radius * 0.65)
            height: 1
            z: 99
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0;  color: "transparent" }
                GradientStop { position: 0.25; color: DesignTokens.borderSpecularTop }
                GradientStop { position: 0.75; color: DesignTokens.borderSpecularTop }
                GradientStop { position: 1.0;  color: "transparent" }
            }
        }
    }
}
