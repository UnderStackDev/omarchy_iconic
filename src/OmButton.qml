import QtQuick

// A boxy Omarchy button: 1px border, monospace label, accent when primary.
Rectangle {
    id: button
    property alias text: label.text
    property bool primary: false
    property bool danger: false
    signal clicked()

    implicitWidth: label.implicitWidth + 28
    implicitHeight: 30
    radius: Math.min(4, theme.radius)
    opacity: enabled ? 1 : 0.4
    color: area.containsMouse && enabled ? theme.selection : (primary ? theme.lighterBackground : "transparent")
    border.width: 1
    border.color: danger ? theme.danger : (primary ? theme.accent : theme.muted)

    Text {
        id: label
        anchors.centerIn: parent
        font.family: theme.font
        font.pixelSize: 12
        color: button.danger ? theme.danger : (button.primary ? theme.accent : theme.foreground)
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (button.enabled) button.clicked()
    }
}
