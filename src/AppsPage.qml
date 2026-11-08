import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Rename, re-icon or hide single app launchers, like a menu editor.
Item {
    id: page

    property alias searchField: search
    property string selectedId: ""
    property string filter: "all"   // all | modified | hidden
    readonly property bool popupOpen: themePicker.visible

    // The editor's working copy of the selected launcher.
    property string editName: ""
    property string editIcon: ""
    property bool editHidden: false

    readonly property var filtered: {
        const q = search.text.trim().toLowerCase()
        return backend.apps.filter(a => {
            if (page.filter === "modified" && !a.modified) return false
            if (page.filter === "hidden" && !(a.hidden || a.hiddenByDesktop)) return false
            if (page.filter === "all" && (a.hidden || a.hiddenByDesktop) && !a.modified) return false
            return q.length === 0 || a.name.toLowerCase().includes(q) || a.id.toLowerCase().includes(q)
        })
    }
    readonly property var app: {
        for (const a of backend.apps)
            if (a.id === selectedId) return a
        return null
    }
    readonly property bool dirty: app !== null
        && (editName.trim() !== app.name || (editIcon !== app.icon && editIcon.length > 0) || editHidden !== app.hidden)

    function load() {
        if (!app) return
        editName = app.name
        editIcon = app.icon
        editHidden = app.hidden
    }
    onAppChanged: load()

    function focusList() { appList.forceActiveFocus(); if (selectedId === "" && filtered.length > 0) selectedId = filtered[0].id }
    function pickIcon() { if (app) themePicker.open() }
    function save() { if (dirty) backend.saveApp(selectedId, editName, editIcon, editHidden) }
    function iconSource(icon) {
        return "image://appicon/" + encodeURIComponent(backend.currentTheme) + "/" + encodeURIComponent(icon)
    }
    function moveSelection(step) {
        const list = filtered
        if (list.length === 0) return
        let i = list.findIndex(a => a.id === selectedId)
        i = i < 0 ? 0 : Math.max(0, Math.min(list.length - 1, i + step))
        selectedId = list[i].id
        appList.positionViewAtIndex(i, ListView.Contain)
    }

    Connections {
        target: iconPicker
        function onPicked(paths) {
            const imported = backend.importIcon(paths[0], page.selectedId)
            if (imported.length > 0) page.editIcon = imported
        }
        function onFailed(message) { backend.reportError(message) }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // List of launchers
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                SectionLabel { text: qsTr("Launchers") }
                SectionLabel { text: page.filtered.length; color: theme.accent }
                Item { Layout.fillWidth: true }
                // Filter strip
                Rectangle {
                    implicitWidth: filterRow.implicitWidth + 2
                    implicitHeight: 24
                    radius: Math.min(4, theme.radius)
                    color: "transparent"
                    border.width: 1
                    border.color: theme.muted
                    Row {
                        id: filterRow
                        anchors.centerIn: parent
                        Repeater {
                            model: [["all", qsTr("All")], ["modified", qsTr("Modified")], ["hidden", qsTr("Hidden")]]
                            Rectangle {
                                required property var modelData
                                width: label.implicitWidth + 20
                                height: 22
                                color: page.filter === modelData[0] ? theme.selection : "transparent"
                                Text {
                                    id: label
                                    anchors.centerIn: parent
                                    text: parent.modelData[1]
                                    font.family: theme.font
                                    font.pixelSize: 11
                                    color: page.filter === parent.modelData[0] ? theme.accent : theme.foreground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.filter = parent.modelData[0]
                                }
                            }
                        }
                    }
                }
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
                    Text { text: "/"; font.family: theme.font; font.pixelSize: 13; color: search.activeFocus ? theme.accent : theme.dimForeground }
                    TextInput {
                        id: search
                        Layout.fillWidth: true
                        font.family: theme.font
                        font.pixelSize: 13
                        color: theme.foreground
                        clip: true
                        Keys.onEscapePressed: { text = ""; appList.forceActiveFocus() }
                        Keys.onDownPressed: appList.forceActiveFocus()
                        Keys.onReturnPressed: { if (page.filtered.length > 0) page.selectedId = page.filtered[0].id; appList.forceActiveFocus() }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: search.text.length === 0
                            text: qsTr("Search apps")
                            font: search.font
                            color: theme.dimForeground
                        }
                    }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.IBeamCursor; onClicked: search.forceActiveFocus() }
            }

            ListView {
                id: appList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                spacing: 4
                model: page.filtered
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: OmScrollBar {}
                Keys.onUpPressed: page.moveSelection(-1)
                Keys.onDownPressed: page.moveSelection(1)
                Keys.onReturnPressed: nameInput.forceActiveFocus()

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool selected: modelData.id === page.selectedId
                    width: ListView.view.width - 12  // room for the scrollbar
                    height: 44
                    radius: Math.min(4, theme.radius)
                    color: selected ? theme.selection : (hover.hovered ? theme.lighterBackground : theme.darkBackground)
                    border.width: 1
                    border.color: selected ? theme.accent : theme.muted

                    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { page.selectedId = row.modelData.id; appList.forceActiveFocus() } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12
                        Image {
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            sourceSize: Qt.size(52, 52)
                            asynchronous: true
                            opacity: row.modelData.hidden || row.modelData.hiddenByDesktop ? 0.4 : 1
                            source: page.iconSource(row.modelData.icon)
                        }
                        Text {
                            text: row.modelData.name
                            font.family: theme.font
                            font.pixelSize: 13
                            color: row.selected ? theme.accent : theme.foreground
                            elide: Text.ElideRight
                            Layout.maximumWidth: 260
                        }
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.id
                            elide: Text.ElideRight
                            font.family: theme.font
                            font.pixelSize: 11
                            color: theme.dimForeground
                        }
                        Badge { visible: row.modelData.modified; text: qsTr("Modified"); strong: true }
                        Badge { visible: row.modelData.hidden || row.modelData.hiddenByDesktop; text: qsTr("Hidden") }
                        Badge { visible: row.modelData.source === "flatpak"; text: "Flatpak" }
                        Badge { visible: row.modelData.source === "webapp"; text: qsTr("Web app") }
                        Badge { visible: row.modelData.source === "user"; text: qsTr("User") }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: page.filtered.length === 0
                    text: page.filter === "modified" ? qsTr("Nothing changed yet") : qsTr("No apps match")
                    font.family: theme.font
                    color: theme.dimForeground
                }
            }
        }

        // Editor
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            radius: Math.min(4, theme.radius)
            color: theme.darkBackground
            border.width: 1
            border.color: page.dirty ? theme.accent : theme.muted

            Text {
                anchors.centerIn: parent
                visible: page.app === null
                width: parent.width - 60
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Pick an app to change its name or icon, or hide it from the launcher.")
                font.family: theme.font
                font.pixelSize: 12
                color: theme.dimForeground
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14
                visible: page.app !== null

                SectionLabel { text: qsTr("Edit launcher"); color: theme.accent }

                // Icon: big preview, also a drop target for image files.
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 132
                    implicitHeight: 132
                    radius: Math.min(4, theme.radius)
                    color: iconDrop.containsDrag ? theme.selection : theme.background
                    border.width: 1
                    border.color: iconDrop.containsDrag ? theme.accent : theme.muted
                    Image {
                        anchors.centerIn: parent
                        width: 96; height: 96
                        sourceSize: Qt.size(192, 192)
                        asynchronous: true
                        source: page.editIcon.length > 0 ? page.iconSource(page.editIcon) : ""
                    }
                    DropArea {
                        id: iconDrop
                        anchors.fill: parent
                        onDropped: (event) => {
                            if (!event.hasUrls) return
                            const imported = backend.importIcon(event.urls[0].toString(), page.selectedId)
                            if (imported.length > 0) page.editIcon = imported
                            event.acceptProposedAction()
                        }
                    }
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Drop a PNG or SVG on the icon")
                    font.family: theme.font
                    font.pixelSize: 10
                    color: theme.dimForeground
                }

                // Name
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SectionLabel { text: qsTr("Name") }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: Math.min(4, theme.radius)
                        color: theme.background
                        border.width: 1
                        border.color: nameInput.activeFocus ? theme.accent : theme.muted
                        TextInput {
                            id: nameInput
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: TextInput.AlignVCenter
                            text: page.editName
                            onTextEdited: page.editName = text
                            font.family: theme.font
                            font.pixelSize: 13
                            color: theme.foreground
                            clip: true
                            Keys.onReturnPressed: page.save()
                            Keys.onEscapePressed: { page.load(); appList.forceActiveFocus() }
                        }
                    }
                    Text {
                        visible: page.app !== null && page.app.originalName !== page.editName.trim()
                        text: qsTr("Originally: %1").arg(page.app ? page.app.originalName : "")
                        font.family: theme.font
                        font.pixelSize: 10
                        color: theme.dimForeground
                    }
                }

                // Icon name
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SectionLabel { text: qsTr("Icon") }
                    Text {
                        Layout.fillWidth: true
                        text: page.editIcon.length > 0 ? page.editIcon : qsTr("(none)")
                        elide: Text.ElideMiddle
                        font.family: theme.font
                        font.pixelSize: 12
                        color: theme.foreground
                    }
                    RowLayout {
                        spacing: 8
                        OmButton { text: qsTr("From theme…"); onClicked: themePicker.open() }
                        OmButton { text: qsTr("From file…"); onClicked: iconPicker.openFiles(qsTr("Choose an icon")) }
                        OmButton {
                            visible: page.app !== null && page.editIcon !== page.app.originalIcon
                            text: qsTr("Original")
                            onClicked: page.editIcon = page.app.originalIcon
                        }
                    }
                }

                OmCheck {
                    text: qsTr("Hide from the launcher")
                    checked: page.editHidden
                    onToggled: (on) => page.editHidden = on
                }
                Text {
                    Layout.fillWidth: true
                    visible: page.app !== null && page.app.hiddenByDesktop
                    wrapMode: Text.Wrap
                    text: qsTr("Omarchy or the app itself keeps this one out of the launcher.")
                    font.family: theme.font
                    font.pixelSize: 10
                    color: theme.dimForeground
                }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: page.app ? page.app.path : ""
                    font.family: theme.font
                    font.pixelSize: 10
                    color: theme.dimForeground
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    OmButton {
                        text: qsTr("Reset")
                        danger: true
                        enabled: page.app !== null && page.app.modified
                        onClicked: backend.resetApp(page.selectedId)
                    }
                    Item { Layout.fillWidth: true }
                    OmButton { text: qsTr("Revert"); enabled: page.dirty; onClicked: page.load() }
                    OmButton { text: qsTr("Save"); primary: true; enabled: page.dirty; onClicked: page.save() }
                }
            }
        }
    }

    IconPicker {
        id: themePicker
        onChosen: (name) => page.editIcon = name
    }
}
