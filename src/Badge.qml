import QtQuick

// A small uppercase chip beside a theme's name (ACTIVE, LOCKED, USER).
Rectangle {
    property alias text: label.text
    property bool strong: false

    implicitWidth: label.implicitWidth + 10
    implicitHeight: 16
    radius: Math.min(2, theme.radius)
    color: strong ? theme.accent : "transparent"
    border.width: 1
    border.color: strong ? theme.accent : theme.dimForeground

    Text {
        id: label
        anchors.centerIn: parent
        font.family: theme.font
        font.pixelSize: 9
        font.letterSpacing: 0.8
        font.capitalization: Font.AllUppercase
        color: parent.strong ? theme.accentForeground : theme.dimForeground
    }
}
