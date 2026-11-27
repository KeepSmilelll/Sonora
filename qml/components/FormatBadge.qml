import QtQuick
import Virela.UI

Rectangle {
    id: root

    property string text: ""
    property bool compact: false

    visible: text.length > 0
    implicitHeight: compact ? 18 : 21
    implicitWidth: label.implicitWidth + (compact ? 11 : 15)
    radius: height / 2

    readonly property color badgeColor: {
        if (text.indexOf("DSD") === 0) return DesignTokens.badgePurple
        if (text === "Hi-Res" || text.indexOf("Exclusive") >= 0 || text.indexOf("Bit-Perfect") >= 0
            || text === "源码直出" || text === "独占模式") {
            return DesignTokens.badgeGold
        }
        if (text === "Lossless" || text === "FLAC" || text === "ALAC" || text === "WAV" || text === "APE") {
            return DesignTokens.badgeTeal
        }
        return DesignTokens.textSecondary
    }

    color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, DesignTokens.isDark ? 0.16 : 0.11)
    border.width: 1
    border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, DesignTokens.isDark ? 0.38 : 0.30)

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.badgeColor
        font.family: DesignTokens.fontFamily
        font.pixelSize: root.compact ? 10 : 11
        font.weight: Font.DemiBold
        font.letterSpacing: 0.25
    }
}
