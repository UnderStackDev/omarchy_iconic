import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A modal yes/no box in the Omarchy look. Enter confirms, Esc cancels.
Popup {
    id: dialog
    property string title
    property string message
    property string confirmText: qsTr("OK")
    signal confirmed()

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 20
    width: Math.min(460, parent ? parent.width - 40 : 460)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: theme.background
        border.width: 1
        border.color: theme.accent
    }
    Overlay.modal: Rectangle { color: "#99000000" }

    contentItem: ColumnLayout {
        spacing: 14
        Keys.onReturnPressed: { dialog.confirmed(); dialog.close() }
        SectionLabel { text: dialog.title; color: theme.accent }
        Text {
            Layout.fillWidth: true
            text: dialog.message
            wrapMode: Text.Wrap
            font.family: theme.font
            font.pixelSize: 12
            color: theme.foreground
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            OmButton { text: qsTr("Cancel"); onClicked: dialog.close() }
            OmButton { text: dialog.confirmText; danger: true; onClicked: { dialog.confirmed(); dialog.close() } }
        }
    }
    onOpened: contentItem.forceActiveFocus()
}
