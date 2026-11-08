import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Install icon themes by dropping archives or folders, and manage the ones
// installed this way. Everything lands in ~/.local/share/icons, where Flatpak
// apps can see it too.
Item {
    id: page

    function choose() { filePicker.openFiles(qsTr("Choose icon theme archives")) }

    readonly property bool popupOpen: removeDialog.visible
    readonly property var userThemes: backend.themes.filter(t => t.user)

    Connections {
        target: filePicker
        function onPicked(paths) { backend.installPaths(paths) }
        function onFailed(message) { backend.reportError(message) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        SectionLabel { text: qsTr("Add icon themes") }

        // Drop zone
        Rectangle {
            id: zone
            Layout.fillWidth: true
            Layout.preferredHeight: 190
            radius: Math.min(4, theme.radius)
            color: drop.containsDrag ? theme.selection : theme.darkBackground

            // A dashed outline, brighter while something is dragged over.
            Canvas {
                anchors.fill: parent
                property color stroke: drop.containsDrag ? theme.accent : theme.muted
                onStrokeChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = stroke
                    ctx.lineWidth = 1
                    ctx.setLineDash([6, 4])
                    ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                }
            }

            DropArea {
                id: drop
                anchors.fill: parent
                enabled: !backend.busy
                onDropped: (event) => {
                    if (event.hasUrls) {
                        backend.installPaths(event.urls.map(u => u.toString()))
                        event.acceptProposedAction()
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: !backend.busy
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "⇣"
                    font.pixelSize: 30
                    color: drop.containsDrag ? theme.accent : theme.dimForeground
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: drop.containsDrag ? qsTr("Drop to install") : qsTr("Drop icon theme archives or folders here")
                    font.family: theme.font
                    font.pixelSize: 14
                    color: theme.foreground
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: ".tar.gz  .tar.xz  .tar.zst  .zip  .7z"
                    font.family: theme.font
                    font.pixelSize: 11
                    color: theme.dimForeground
                }
                OmButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    text: qsTr("Choose files…")
                    onClicked: page.choose()
                }
            }

            // Working
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: backend.busy
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: backend.busy
                    implicitWidth: 36; implicitHeight: 36
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: backend.busyText
                    font.family: theme.font
                    font.pixelSize: 13
                    color: theme.foreground
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Installed to ~/.local/share/icons, so Flatpak apps see them too.")
            font.family: theme.font
            font.pixelSize: 11
            color: theme.dimForeground
        }

        // Already installed: ask what to do.
        Rectangle {
            Layout.fillWidth: true
            visible: backend.conflicts.length > 0
            implicitHeight: conflictColumn.implicitHeight + 24
            radius: Math.min(4, theme.radius)
            color: theme.darkBackground
            border.width: 1
            border.color: theme.accent
            ColumnLayout {
                id: conflictColumn
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: 14
                spacing: 10
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Already installed: %1").arg(backend.conflicts.join(", "))
                    font.family: theme.font
                    font.pixelSize: 13
                    color: theme.foreground
                }
                RowLayout {
                    spacing: 8
                    OmButton { text: qsTr("Replace"); primary: true; onClicked: backend.resolveConflicts("replace") }
                    OmButton { text: qsTr("Keep both"); onClicked: backend.resolveConflicts("keepBoth") }
                    OmButton { text: qsTr("Skip those"); onClicked: backend.resolveConflicts("skip") }
                    OmButton { text: qsTr("Cancel"); onClicked: backend.resolveConflicts("cancel") }
                }
            }
        }

        // Result of the last install.
        Rectangle {
            Layout.fillWidth: true
            visible: !backend.busy && (backend.lastInstalled.length > 0 || backend.lastNotes.length > 0)
            implicitHeight: resultColumn.implicitHeight + 24
            radius: Math.min(4, theme.radius)
            color: theme.darkBackground
            border.width: 1
            border.color: theme.muted
            ColumnLayout {
                id: resultColumn
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: 14
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("Last install"); color: theme.accent }
                    Item { Layout.fillWidth: true }
                    OmButton { text: qsTr("Clear"); onClicked: backend.clearInstallResult() }
                }
                Repeater {
                    model: backend.lastInstalled
                    RowLayout {
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "✓"; color: theme.accent; font.family: theme.font }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Installed %1").arg(parent.modelData)
                            font.family: theme.font
                            font.pixelSize: 13
                            color: theme.foreground
                        }
                        OmButton {
                            text: parent.modelData === backend.currentTheme ? qsTr("Applied") : qsTr("Apply now")
                            primary: true
                            enabled: parent.modelData !== backend.currentTheme
                            onClicked: backend.applyTheme(parent.modelData)
                        }
                    }
                }
                Repeater {
                    model: backend.lastNotes
                    Text {
                        required property string modelData
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: "· " + modelData
                        font.family: theme.font
                        font.pixelSize: 11
                        color: theme.dimForeground
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            SectionLabel { text: qsTr("Installed by you") }
            SectionLabel { text: page.userThemes.length; color: theme.accent }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: page.userThemes
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: OmScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width - 12  // room for the scrollbar
                height: 48
                radius: Math.min(4, theme.radius)
                color: theme.darkBackground
                border.width: 1
                border.color: theme.muted
                readonly property bool active: modelData.id === backend.currentTheme

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 10
                    spacing: 12
                    Image {
                        width: 24; height: 24
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        sourceSize: Qt.size(48, 48)
                        asynchronous: true
                        source: "image://preview/" + encodeURIComponent(row.modelData.id) + "/folder|inode-directory"
                    }
                    Text {
                        text: row.modelData.id
                        font.family: theme.font
                        font.pixelSize: 13
                        color: row.active ? theme.accent : theme.foreground
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: row.modelData.path
                        font.family: theme.font
                        font.pixelSize: 11
                        color: theme.dimForeground
                    }
                    Badge { visible: row.active; text: qsTr("Active"); strong: true }
                    OmButton {
                        visible: !row.active
                        text: qsTr("Apply")
                        primary: true
                        onClicked: backend.applyTheme(row.modelData.id)
                    }
                    OmButton {
                        text: qsTr("Remove")
                        danger: true
                        enabled: !row.active && backend.lockedTheme.length >= 0 && backend.canRemove(row.modelData.id)
                        onClicked: { removeDialog.themeId = row.modelData.id; removeDialog.themePath = row.modelData.path; removeDialog.open() }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: page.userThemes.length === 0
                text: qsTr("Nothing yet — themes you add show up here.")
                font.family: theme.font
                font.pixelSize: 12
                color: theme.dimForeground
            }
        }
    }

    ConfirmDialog {
        id: removeDialog
        property string themeId
        property string themePath
        title: qsTr("Remove icon theme?")
        readonly property var users: themeId.length > 0 ? backend.dependents(themeId) : []
        message: users.length > 0
                 ? qsTr("%1 uses files from %2. Remove %1 first.").arg(users.join(", ")).arg(themeId)
                 : qsTr("%1 will be deleted from %2.").arg(themeId).arg(themePath)
        confirmText: qsTr("Remove")
        onConfirmed: backend.removeTheme(themeId)
    }
}
