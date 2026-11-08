import QtQuick
import QtQuick.Layouts

// One installed icon theme: its name, badges, and a strip of its own icons.
Rectangle {
    id: card

    required property var modelData
    property bool selected: false
    property bool active: modelData.id === backend.currentTheme
    property bool locked: backend.lock && modelData.id === backend.lockedTheme

    signal picked()
    signal chosen()

    // Each slot lists names themes use for the same thing, first match wins.
    readonly property var samples: [
        "folder|inode-directory",
        "user-home|folder-home",
        "utilities-terminal|org.gnome.Terminal|terminal",
        "web-browser|internet-web-browser|firefox",
        "preferences-system|preferences-desktop|org.gnome.Settings",
        "system-file-manager|org.gnome.Nautilus|nautilus",
        "accessories-text-editor|text-editor|org.gnome.TextEditor",
        "audio-x-generic|multimedia-audio-player"
    ]

    radius: Math.min(4, theme.radius)
    color: selected ? theme.selection : (hover.hovered ? theme.lighterBackground : theme.darkBackground)
    border.width: 1
    border.color: selected ? theme.accent : theme.muted

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        onTapped: card.picked()
        onDoubleTapped: card.chosen()
    }

    // The active theme gets an accent bar on its leading edge.
    Rectangle {
        visible: card.active
        width: 3
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 1 }
        color: theme.accent
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        anchors.leftMargin: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                Layout.fillWidth: true
                text: card.modelData.id
                elide: Text.ElideRight
                font.family: theme.font
                font.pixelSize: 13
                color: card.active || card.selected ? theme.accent : theme.foreground
            }
            Badge { visible: card.locked; text: qsTr("Locked") }
            Badge { visible: card.active; text: qsTr("Active"); strong: true }
            Badge { visible: card.modelData.user; text: qsTr("User") }
        }

        Row {
            spacing: 8
            Repeater {
                model: card.samples
                Image {
                    required property string modelData
                    width: 26; height: 26
                    // Asked for at twice the size: themes keep monochrome panel icons in
                    // their small folders, and HiDPI screens want the pixels anyway.
                    sourceSize: Qt.size(52, 52)
                    asynchronous: true
                    source: "image://preview/" + encodeURIComponent(card.modelData.id) + "/" + modelData
                }
            }
        }
    }
}
