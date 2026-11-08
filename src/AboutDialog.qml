import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Name, version, copyright and where the code lives.
Popup {
    id: about
    readonly property string repo: "https://github.com/UnderStackDev/iconic"

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 28
    width: Math.min(440, parent ? parent.width - 40 : 440)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: theme.background
        border.width: 1
        border.color: theme.accent
    }
    Overlay.modal: Rectangle { color: "#99000000" }
    onAboutToShow: contentItem.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 16
        Keys.onReturnPressed: about.close()

        // The current theme's own icons, as a small signature.
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10
            Repeater {
                model: ["folder|inode-directory", "utilities-terminal|org.gnome.Terminal", "web-browser|firefox",
                        "preferences-system|preferences-desktop", "audio-x-generic|multimedia-audio-player"]
                Image {
                    required property string modelData
                    width: 32; height: 32
                    sourceSize: Qt.size(64, 64)
                    source: "image://preview/" + encodeURIComponent(backend.currentTheme) + "/" + modelData
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "ICONIC"
                font.family: theme.font
                font.pixelSize: 20
                font.bold: true
                font.letterSpacing: 4
                color: theme.accent
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Version %1").arg(Qt.application.version)
                font.family: theme.font
                font.pixelSize: 12
                color: theme.foreground
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Icon themes and app launchers for Omarchy.")
            font.family: theme.font
            font.pixelSize: 12
            color: theme.dimForeground
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: theme.muted }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "© 2026 UnderStackDev"
                font.family: theme.font
                font.pixelSize: 12
                color: theme.foreground
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("MIT License")
                font.family: theme.font
                font.pixelSize: 11
                color: theme.dimForeground
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                text: qsTr("Theme code follows Omacut (MIT, David Heinemeier Hansson)")
                font.family: theme.font
                font.pixelSize: 10
                color: theme.dimForeground
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            OmButton {
                text: "github.com/UnderStackDev/iconic"
                primary: true
                onClicked: Qt.openUrlExternally(about.repo)
            }
            OmButton { text: qsTr("Close"); onClicked: about.close() }
        }
    }
}
