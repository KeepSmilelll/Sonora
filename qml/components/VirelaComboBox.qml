import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Virela.UI

Item {
    id: root

    property var model: []
    property string textRole: ""
    property int currentIndex: 0
    property bool autoSelect: true
    signal activated(int index)

    implicitWidth: 168
    implicitHeight: 32
    opacity: enabled ? 1.0 : 0.45

    readonly property int itemCount: {
        if (!model) return 0
        if (model.count !== undefined) return model.count
        if (model.length !== undefined) return model.length
        return 0
    }

    function labelAt(idx) {
        if (!model || idx < 0 || idx >= itemCount) return ""
        const item = model.get !== undefined ? model.get(idx) : model[idx]
        if (item === undefined || item === null) return ""
        if (typeof item === "string" || typeof item === "number") return String(item)
        if (textRole.length > 0 && item[textRole] !== undefined) return String(item[textRole])
        if (item.name !== undefined) return String(item.name)
        return String(item)
    }

    Rectangle {
        id: trigger
        anchors.fill: parent
        radius: height / 2
        color: (popup.visible || triggerMa.containsMouse)
            ? (DesignTokens.isDark ? DesignTokens.surfaceCardHover : Qt.rgba(0.935, 0.948, 0.980, 0.96))
            : DesignTokens.surfaceInput
        border.width: 1
        border.color: popup.visible ? DesignTokens.accent
            : (triggerMa.containsMouse ? DesignTokens.borderHover : DesignTokens.borderOuter)

        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 11
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: root.labelAt(root.currentIndex)
                elide: Text.ElideRight
                color: DesignTokens.textPrimary
                font.family: DesignTokens.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
            }

            IconGlyph {
                name: "chevron_down"
                size: 13
                color: popup.visible ? DesignTokens.accent : DesignTokens.textSecondary
                rotation: popup.visible ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: 200; easing.type: Easing.OutQuint }
                }
            }
        }
    }

    MouseArea {
        id: triggerMa
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (popup.visible) popup.close()
            else if (root.itemCount > 0) popup.open()
        }
    }

    Popup {
        id: popup
        y: root.height + 5
        width: Math.max(root.width, 180)
        height: Math.min(240, listCol.implicitHeight + 12)
        padding: 6
        transformOrigin: Item.Top

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 160; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale";   from: 0.95; to: 1.0; duration: 190; easing.type: Easing.OutBack; easing.overshoot: 1.05 }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 120; easing.type: Easing.InQuad }
                NumberAnimation { property: "scale";   from: 1.0; to: 0.96; duration: 120; easing.type: Easing.InQuad }
            }
        }

        background: GlassPanel {
            radius: DesignTokens.radiusMd
            elevated: true
            shadowEnabled: true
        }

        contentItem: Flickable {
            contentWidth: width
            contentHeight: listCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: listCol
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.itemCount
                    delegate: Rectangle {
                        id: optRow
                        required property int index
                        readonly property bool isCurrent: index === root.currentIndex
                        width: listCol.width
                        height: 30
                        radius: DesignTokens.radiusSm
                        color: isCurrent
                            ? DesignTokens.surfaceSelected
                            : (optMa.containsMouse ? DesignTokens.surfaceHover : "transparent")
                        Behavior on color { ColorAnimation { duration: 100 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 9
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: root.labelAt(optRow.index)
                                elide: Text.ElideRight
                                color: optRow.isCurrent ? DesignTokens.accent : DesignTokens.textPrimary
                                font.family: DesignTokens.fontFamily
                                font.pixelSize: 12
                                font.weight: optRow.isCurrent ? Font.DemiBold : Font.Normal
                            }

                            IconGlyph {
                                visible: optRow.isCurrent
                                name: "check"
                                size: 13
                                color: DesignTokens.accent
                            }
                        }

                        MouseArea {
                            id: optMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.autoSelect) root.currentIndex = optRow.index
                                root.activated(optRow.index)
                                popup.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
