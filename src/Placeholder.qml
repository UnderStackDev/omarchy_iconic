import QtQuick

// A tab that isn't built yet says so instead of showing an empty page.
Item {
    property alias text: label.text
    Text {
        id: label
        anchors.centerIn: parent
        font.family: theme.font
        font.pixelSize: 13
        color: theme.dimForeground
    }
}
