import QtQuick
import QtQuick.Controls

// A thin scrollbar in the theme's colours; it widens a little under the pointer.
ScrollBar {
    id: bar
    padding: 2
    implicitWidth: hovered || pressed ? 8 : 5
    Behavior on implicitWidth { NumberAnimation { duration: 120 } }

    background: Item {}
    contentItem: Rectangle {
        implicitWidth: bar.implicitWidth - bar.padding * 2
        radius: width / 2
        color: bar.pressed ? theme.accent : (bar.hovered ? theme.dimForeground : theme.muted)
        opacity: bar.size < 1 ? 1 : 0
    }
}
