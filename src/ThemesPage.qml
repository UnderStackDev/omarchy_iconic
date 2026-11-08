import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Pick the desktop's icon theme, and whether it follows the Omarchy theme.
Item {
    id: page

    property string selectedId: backend.currentTheme
    property alias searchField: search

    readonly property var filtered: {
        const q = search.text.trim().toLowerCase()
        return backend.themes.filter(t => q.length === 0
            || t.id.toLowerCase().includes(q) || t.name.toLowerCase().includes(q))
    }
    readonly property var selectedTheme: {
        for (const t of backend.themes)
            if (t.id === selectedId) return t
        return null
    }
    // Reads the current and locked theme so it re-checks when they change.
    readonly property bool selectedRemovable: selectedTheme !== null && selectedTheme.user
        && backend.currentTheme.length >= 0 && backend.lockedTheme.length >= 0
        && backend.canRemove(selectedTheme.id)

    function applySelected() {
        if (selectedTheme && selectedTheme.id !== backend.currentTheme)
            backend.applyTheme(selectedTheme.id)
    }
    function askRemove() { if (selectedRemovable) removeDialog.open() }

    function moveSelection(step) {
        const list = filtered
        if (list.length === 0) return
        let i = list.findIndex(t => t.id === selectedId)
        i = i < 0 ? 0 : Math.max(0, Math.min(list.length - 1, i + step))
        selectedId = list[i].id
        grid.positionViewAtIndex(i, GridView.Contain)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        // Follow / Lock
        SectionLabel { text: qsTr("Omarchy theme switches") }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: modeRow.implicitHeight + 24
            radius: Math.min(4, theme.radius)
            color: theme.darkBackground
            border.width: 1
            border.color: backend.lock ? theme.accent : theme.muted

            RowLayout {
                id: modeRow
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: 14
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: theme.font
                        font.pixelSize: 13
                        color: theme.foreground
                        text: backend.lock ? qsTr("Keeping %1 when the Omarchy theme changes").arg(backend.lockedTheme)
                                           : qsTr("Each Omarchy theme picks its own icon theme")
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.family: theme.font
                        font.pixelSize: 11
                        color: theme.dimForeground
                        text: backend.lock ? qsTr("Untick to let Omarchy themes choose again.")
                                           : qsTr("Tick to keep the icon theme you apply here.")
                    }
                }
                OmCheck {
                    text: qsTr("Keep my choice")
                    checked: backend.lock
                    onToggled: (on) => backend.setLock(on)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            SectionLabel { text: qsTr("Installed themes") }
            SectionLabel { text: page.filtered.length; color: theme.accent }
            Item { Layout.fillWidth: true }
        }

        // Search box
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
                Text {
                    text: "/"
                    font.family: theme.font
                    font.pixelSize: 13
                    color: search.activeFocus ? theme.accent : theme.dimForeground
                }
                TextInput {
                    id: search
                    Layout.fillWidth: true
                    font.family: theme.font
                    font.pixelSize: 13
                    color: theme.foreground
                    selectionColor: theme.selection
                    selectedTextColor: theme.brightForeground
                    clip: true
                    Keys.onEscapePressed: { text = ""; grid.forceActiveFocus() }
                    Keys.onDownPressed: grid.forceActiveFocus()
                    Keys.onReturnPressed: { if (page.filtered.length > 0) page.selectedId = page.filtered[0].id; grid.forceActiveFocus() }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: search.text.length === 0
                        text: qsTr("Search themes")
                        font: search.font
                        color: theme.dimForeground
                    }
                }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.IBeamCursor; onClicked: search.forceActiveFocus() }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            readonly property int columns: Math.max(1, Math.floor(width / 290))
            cellWidth: Math.floor(width / columns)
            cellHeight: 92
            model: page.filtered
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Keys.onLeftPressed: page.moveSelection(-1)
            Keys.onRightPressed: page.moveSelection(1)
            Keys.onUpPressed: page.moveSelection(-columns)
            Keys.onDownPressed: page.moveSelection(columns)
            Keys.onReturnPressed: page.applySelected()

            delegate: Item {
                id: cell
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight
                ThemeCard {
                    anchors.fill: parent
                    anchors.margins: 5
                    modelData: cell.modelData
                    selected: cell.modelData.id === page.selectedId
                    onPicked: { page.selectedId = cell.modelData.id; grid.forceActiveFocus() }
                    onChosen: { page.selectedId = cell.modelData.id; page.applySelected() }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: page.filtered.length === 0
                text: qsTr("No icon themes match \"%1\"").arg(search.text)
                font.family: theme.font
                color: theme.dimForeground
            }
        }

        // The selected theme and what can be done with it.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: page.selectedTheme ? page.selectedTheme.id : ""
                    font.family: theme.font
                    font.pixelSize: 13
                    color: theme.accent
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: page.selectedTheme
                          ? (page.selectedTheme.comment.length > 0 ? page.selectedTheme.comment : page.selectedTheme.path)
                          : ""
                    font.family: theme.font
                    font.pixelSize: 11
                    color: theme.dimForeground
                }
            }
            OmButton {
                text: qsTr("Remove")
                danger: true
                visible: page.selectedTheme !== null && page.selectedTheme.user
                enabled: page.selectedRemovable
                onClicked: page.askRemove()
            }
            OmButton {
                text: page.selectedTheme && page.selectedTheme.id === backend.currentTheme ? qsTr("Applied") : qsTr("Apply")
                primary: true
                enabled: page.selectedTheme !== null && page.selectedTheme.id !== backend.currentTheme
                onClicked: page.applySelected()
            }
        }
    }

    ConfirmDialog {
        id: removeDialog
        title: qsTr("Remove icon theme?")
        readonly property var users: page.selectedTheme ? backend.dependents(page.selectedTheme.id) : []
        message: !page.selectedTheme ? ""
                 : users.length > 0
                   ? qsTr("%1 uses files from %2. Remove %1 first.").arg(users.join(", ")).arg(page.selectedTheme.id)
                   : qsTr("%1 will be deleted from %2.").arg(page.selectedTheme.id).arg(page.selectedTheme.path)
        confirmText: qsTr("Remove")
        onConfirmed: if (page.selectedTheme) backend.removeTheme(page.selectedTheme.id)
    }
}
