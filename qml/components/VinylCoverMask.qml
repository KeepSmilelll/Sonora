import QtQuick
import QtQuick.Effects

MultiEffect {
    required property Item sourceItem
    required property Item maskItem
    source: sourceItem
    maskEnabled: true
    maskSource: maskItem
    maskThresholdMin: 0.5
    maskSpreadAtMin: 1.0
}
