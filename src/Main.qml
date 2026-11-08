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
    readonly property bool typing: themesPage.searchField.activeFocus

    // Another tool (or an Omarchy theme switch) may change the icon theme
    // while Iconic is open; re-read when the window comes back to front.
    onActiveChanged: if (active) backend.refresh()

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 1 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.tab = 2 }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.tab === 0; onActivated: themesPage.searchField.forceActiveFocus() }
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
                implicitHeight: 48
                color: theme.darkBackground

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 18

                    Row {
                        spacing: 10
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18; height: 18
                            sourceSize: Qt.size(18, 18)
                            source: "image://preview/" + encodeURIComponent(backend.currentTheme) + "/folder|inode-directory"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "ICONIC"
                            font.family: theme.font
                            font.pixelSize: 13
                            font.bold: true
                            font.letterSpacing: 2
                            color: theme.accent
                        }
                    }

                    // Tabs as a segmented strip.
                    Rectangle {
                        implicitWidth: tabRow.implicitWidth + 2
                        implicitHeight: 28
                        radius: Math.min(4, theme.radius)
                        color: "transparent"
                        border.width: 1
                        border.color: theme.muted
                        Row {
                            id: tabRow
                            anchors.centerIn: parent
                            Repeater {
                                model: win.tabNames
                                Rectangle {
                                    required property string modelData
                                    required property int index
                                    width: tabLabel.implicitWidth + 28
                                    height: 26
                                    color: win.tab === index ? theme.selection : (tabArea.containsMouse ? theme.lighterBackground : "transparent")
                                    Text {
                                        id: tabLabel
                                        anchors.centerIn: parent
                                        text: (parent.index + 1) + " " + parent.modelData
                                        font.family: theme.font
                                        font.pixelSize: 12
                                        color: win.tab === parent.index ? theme.accent : theme.foreground
                                    }
                                    MouseArea {
                                        id: tabArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.tab = parent.index
                                    }
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
                Placeholder { text: qsTr("App names and icons — coming in phase 3") }
            }

            // Footer: every shortcut in sight, like Omarchy's own tools.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 34
                color: theme.darkBackground
                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.muted }
                Flow {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.topMargin: 8
                    spacing: 16
                    clip: true
                    KeyHint { key: "1-3"; action: qsTr("tabs") }
                    KeyHint { visible: win.tab === 0; key: "←↑→↓"; action: qsTr("pick") }
                    KeyHint { visible: win.tab === 0; key: "space"; action: qsTr("apply") }
                    KeyHint { visible: win.tab === 0; key: "/"; action: qsTr("search") }
                    KeyHint { visible: win.tab === 0; key: "del"; action: qsTr("remove") }
                    KeyHint { visible: win.tab === 1; key: "o"; action: qsTr("choose files") }
                    KeyHint { key: "l"; action: qsTr("keep my choice") }
                    KeyHint { key: "?"; action: qsTr("all keys") }
                    KeyHint { key: "q"; action: qsTr("quit") }
                }
            }
        }

        // Errors stay on screen until dismissed; nothing fails silently.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 96
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
                        KeyHint { key: "o" }              Text { text: qsTr("Choose theme files to install (Install tab)"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "?" }              Text { text: qsTr("Show or hide this"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                        KeyHint { key: "q" }              Text { text: qsTr("Quit"); color: theme.foreground; font.family: theme.font; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
