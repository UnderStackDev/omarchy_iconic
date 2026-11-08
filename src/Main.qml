import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1080
    height: 740
    minimumWidth: 680
    minimumHeight: 480
    visible: true
    title: "Iconic"
    color: theme.background
    font.family: theme.font
    font.pixelSize: 13

    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property int tab: 0
    readonly property var tabNames: [qsTr("Themes"), qsTr("Install"), qsTr("Apps")]
    // Single-key shortcuts stay quiet while a text field has focus.
    // Popups count too: keys typed while one opens must not trigger shortcuts.
    readonly property bool typing: (activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition"))
        || themesPage.popupOpen || installPage.popupOpen || appsPage.popupOpen || quitDialog.visible || aboutDialog.visible
    property bool quitAsked: false

    onClosing: (close) => {
        if (appsPage.dirty && !win.quitAsked) {
            close.accepted = false
            quitDialog.open()
        }
    }

    function quit() {
        if (appsPage.dirty) quitDialog.open()
        else Qt.quit()
    }

    // Another tool (or an Omarchy theme switch) may change the icon theme
    // while Iconic is open; re-read when the window comes back to front.
    onActiveChanged: if (active) backend.refresh()
    // Arrow keys work right away on the tab you switch to.
    onTabChanged: Qt.callLater(() => { if (tab === 0) themesPage.focusList(); else if (tab === 2) appsPage.focusList() })

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: aboutDialog.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.quit() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 1 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 2 }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab !== 1; onActivated: (win.tab === 0 ? themesPage : appsPage).searchField.forceActiveFocus() }
    Shortcut { sequence: "I"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab === 2; onActivated: appsPage.pickIcon() }
    Shortcut { sequences: [StandardKey.Save]; context: Qt.ApplicationShortcut; enabled: win.tab === 2; onActivated: appsPage.save() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.canUndo; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.canRedo; onActivated: backend.redo() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab === 0; onActivated: themesPage.applySelected() }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.setLock(!backend.lock) }
    Shortcut { sequence: "Del"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab === 0; onActivated: themesPage.askRemove() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab === 1 && !backend.busy; onActivated: installPage.choose() }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header: app name, tabs, and what the desktop uses right now.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                color: theme.darkBackground

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 18

                    Text {
                        text: "ICONIC"
                        font.family: theme.font
                        font.pixelSize: 13
                        font.bold: true
                        font.letterSpacing: 2
                        color: theme.accent
                    }

                    // Tabs, each its own button.
                    Row {
                        spacing: 8
                        Repeater {
                            model: win.tabNames
                            Rectangle {
                                id: tabButton
                                required property string modelData
                                required property int index
                                readonly property bool current: win.tab === index
                                width: tabLabel.implicitWidth + 28
                                height: 28
                                radius: Math.min(4, theme.radius)
                                color: current ? theme.selection : (tabArea.containsMouse ? theme.lighterBackground : "transparent")
                                border.width: 1
                                border.color: current ? theme.accent : theme.muted
                                Text {
                                    id: tabLabel
                                    anchors.centerIn: parent
                                    text: (tabButton.index + 1) + " " + tabButton.modelData
                                    font.family: theme.font
                                    font.pixelSize: 12
                                    color: tabButton.current ? theme.accent : theme.foreground
                                }
                                MouseArea {
                                    id: tabArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.tab = tabButton.index
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: backend.currentTheme
                        font.family: theme.font
                        font.pixelSize: 12
                        color: theme.foreground
                    }
                    Badge { text: backend.lock ? qsTr("Locked") : qsTr("Follows Omarchy"); strong: backend.lock }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.muted }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.tab

                ThemesPage { id: themesPage }
                InstallPage { id: installPage }
                AppsPage { id: appsPage }
            }

            // Footer: every shortcut in sight, like Omarchy's own tools.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 46
                color: theme.darkBackground
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.muted }
                // One row; on a narrow window the last hints are cut, never wrapped.
                Item {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    clip: true
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 20
                    KeyHint { key: "1-3"; action: qsTr("tabs") }
                    KeyHint { visible: win.tab === 0; key: "←↑→↓"; action: qsTr("pick") }
                    KeyHint { visible: win.tab === 0; key: "space"; action: qsTr("apply") }
                    KeyHint { visible: win.tab === 0; key: "/"; action: qsTr("search") }
                    KeyHint { visible: win.tab === 0; key: "del"; action: qsTr("remove") }
                    KeyHint { visible: win.tab === 1; key: "o"; action: qsTr("choose files") }
                    KeyHint { visible: win.tab === 2; key: "↑↓"; action: qsTr("pick") }
                    KeyHint { visible: win.tab === 2; key: "/"; action: qsTr("search") }
                    KeyHint { visible: win.tab === 2; key: "enter"; action: qsTr("edit name") }
                    KeyHint { visible: win.tab === 2; key: "i"; action: qsTr("icon from theme") }
                    KeyHint { visible: win.tab === 2; key: "ctrl+s"; action: qsTr("save") }
                    KeyHint { visible: win.tab === 2; key: "ctrl+z"; action: qsTr("undo") }
                    KeyHint { key: "l"; action: qsTr("keep my choice") }
                    KeyHint { key: "a"; action: qsTr("about") }
                    KeyHint { key: "?"; action: qsTr("all keys") }
                    KeyHint { key: "q"; action: qsTr("quit") }
                }
                }
            }
        }

        AboutDialog { id: aboutDialog }

        ConfirmDialog {
            id: quitDialog
            title: qsTr("Quit without saving?")
            message: qsTr("Your changes to %1 aren't saved.").arg(appsPage.app ? appsPage.app.name : "")
            confirmText: qsTr("Quit")
            onConfirmed: { win.quitAsked = true; Qt.quit() }
        }

        // Errors stay on screen until dismissed; nothing fails silently.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 108
            visible: backend.error.length > 0
            width: Math.min(parent.width - 40, errorRow.implicitWidth + 32)
            height: errorRow.implicitHeight + 20
            radius: Math.min(4, theme.radius)
            color: theme.background
            border.color: theme.danger
            border.width: 1
            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.margins: 10
                anchors.leftMargin: 16
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 640
                    text: backend.error
                    wrapMode: Text.Wrap
                    font.family: theme.font
                    font.pixelSize: 12
                    color: theme.foreground
                }
                OmButton { text: qsTr("Dismiss"); onClicked: backend.clearError() }
            }
        }

        // Keyboard help overlay.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#99000000"
            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }
            Rectangle {
                anchors.centerIn: parent
                width: helpColumn.implicitWidth + 48
                height: helpColumn.implicitHeight + 40
                radius: Math.min(4, theme.radius)
                color: theme.background
                border.width: 1
                border.color: theme.accent
                ColumnLayout {
                    id: helpColumn
                    anchors.centerIn: parent
                    spacing: 14
                    SectionLabel { text: qsTr("Keyboard"); color: theme.accent }
                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 8
                        KeyHint { key: "1 2 3" }          Text { text: qsTr("Switch tab"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "←↑→↓" }           Text { text: qsTr("Pick a theme"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "space / enter" }  Text { text: qsTr("Apply the picked theme"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "/" }              Text { text: qsTr("Search themes"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "l" }              Text { text: qsTr("Keep my choice on or off"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "del" }            Text { text: qsTr("Remove the picked user theme"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "i" }              Text { text: qsTr("Pick an icon from the theme (Apps tab)"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "ctrl+s" }         Text { text: qsTr("Save the launcher you're editing (Apps tab)"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "ctrl+z / ctrl+shift+z" } Text { text: qsTr("Undo / redo launcher changes"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "o" }              Text { text: qsTr("Choose theme files to install (Install tab)"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "a" }              Text { text: qsTr("About Iconic"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "?" }              Text { text: qsTr("Show or hide this"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "q" }              Text { text: qsTr("Quit"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
