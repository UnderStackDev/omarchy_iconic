import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Pick an icon by name from the current theme (and hicolor's app icons).
Popup {
    id: picker
    signal chosen(string name)
    property string selected: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760, parent ? parent.width - 60 : 760)
    height: Math.min(560, parent ? parent.height - 60 : 560)
    modal: true
    focus: true
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var names: visible ? backend.themeIconNames(search.text, 600) : []

    // Focus before the open animation, so the first keys typed land here.
    onAboutToShow: { search.text = ""; search.forceActiveFocus() }

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: theme.background
        border.width: 1
        border.color: theme.accent
    }
    Overlay.modal: Rectangle { color: "#99000000" }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            SectionLabel { text: qsTr("Icons in %1").arg(backend.currentTheme); color: theme.accent }
            Item { Layout.fillWidth: true }
            SectionLabel { text: picker.names.length >= 600 ? qsTr("first 600 — type to narrow") : picker.names.length }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            radius: Math.min(4, theme.radius)
            color: theme.darkBackground
            border.width: 1
            border.color: search.activeFocus ? theme.accent : theme.muted
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10
                Text { text: "/"; font.family: theme.font; font.pixelSize: 13; color: theme.accent }
                TextInput {
                    id: search
                    Layout.fillWidth: true
                    font.family: theme.font
                    font.pixelSize: 13
                    color: theme.foreground
                    clip: true
                    Keys.onDownPressed: grid.forceActiveFocus()
                    Keys.onReturnPressed: if (picker.names.length > 0) { picker.chosen(picker.names[0]); picker.close() }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: search.text.length === 0
                        text: qsTr("Search icon names, e.g. browser, music, terminal")
                        font: search.font
                        color: theme.dimForeground
                    }
                }
            }
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(width / Math.max(1, Math.floor(width / 92)))
            cellHeight: 92
            model: picker.names
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            keyNavigationEnabled: true
            highlightFollowsCurrentItem: true
            Keys.onReturnPressed: if (currentIndex >= 0) { picker.chosen(picker.names[currentIndex]); picker.close() }

            delegate: Rectangle {
                id: cell
                required property string modelData
                required property int index
                width: grid.cellWidth - 6
                height: grid.cellHeight - 6
                radius: Math.min(4, theme.radius)
                color: GridView.isCurrentItem && grid.activeFocus ? theme.selection : (hover.hovered ? theme.lighterBackground : "transparent")
                border.width: GridView.isCurrentItem && grid.activeFocus ? 1 : 0
                border.color: theme.accent
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { picker.chosen(cell.modelData); picker.close() } }
                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 40; height: 40
                        sourceSize: Qt.size(80, 80)
                        asynchronous: true
                        source: "image://preview/" + encodeURIComponent(backend.currentTheme) + "/" + encodeURIComponent(cell.modelData)
                    }
                    Text {
                        width: cell.width - 8
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        text: cell.modelData
                        font.family: theme.font
                        font.pixelSize: 9
                        color: theme.dimForeground
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: picker.names.length === 0
                text: qsTr("No icon names match")
                font.family: theme.font
                color: theme.dimForeground
            }
        }
    }
}
