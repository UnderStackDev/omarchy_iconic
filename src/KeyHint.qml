import QtQuick

// "[key] action" in the footer, so every shortcut is in sight.
Row {
    id: hint
    property string key
    property string action
    spacing: 7
    Rectangle {
        width: keyText.implicitWidth + 12
        height: 22
        anchors.verticalCenter: parent.verticalCenter
        radius: Math.min(2, theme.radius)
        color: "transparent"
        border.width: 1
        border.color: theme.muted
        Text {
            id: keyText
            anchors.centerIn: parent
            text: hint.key
            font.family: theme.font
            font.pixelSize: 12
            color: theme.foreground
        }
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: hint.action
        font.family: theme.font
        font.pixelSize: 12
        color: theme.dimForeground
    }
}
