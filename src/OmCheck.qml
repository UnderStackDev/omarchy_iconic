import QtQuick

// A square checkbox with a label, like Omarchy's own tools use.
Item {
    id: check
    property alias text: label.text
    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: row.implicitWidth
    implicitHeight: 22

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Rectangle {
            width: 14; height: 14
            anchors.verticalCenter: parent.verticalCenter
            radius: Math.min(2, theme.radius)
            color: check.checked ? theme.accent : "transparent"
            border.width: 1
            border.color: check.checked ? theme.accent : theme.dimForeground
            Text {
                anchors.centerIn: parent
                visible: check.checked
                text: "✓"
                font.pixelSize: 11
                font.bold: true
                color: theme.accentForeground
            }
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            font.family: theme.font
            font.pixelSize: 12
            color: theme.foreground
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: check.toggled(!check.checked)
    }
}
