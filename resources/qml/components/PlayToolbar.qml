import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Section play toolbar used by Songs, Albums and Artists: starts the whole
// visible content from the beginning (in order) or in a random order. The
// parent decides what "all" means — the visible song list, or the songs of
// every visible album/artist.
RowLayout {
    id: playToolbar

    signal playFromStart()
    signal playShuffled()

    property bool canPlay: true

    spacing: 6

    Button {
        font.family: Theme.fontFamily
        id: playAllButton
        focusPolicy: Qt.NoFocus
        flat: true
        font.pixelSize: 12
        enabled: playToolbar.canPlay
        text: qsTr("Play all")

        contentItem: Text {
            text: playAllButton.text
            font: playAllButton.font
            color: playAllButton.enabled
                   ? (playAllButton.hovered ? Theme.accent : Theme.textSecondary)
                   : Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { color: "transparent" }
        onClicked: playToolbar.playFromStart()
    }

    Button {
        font.family: Theme.fontFamily
        id: shuffleButton
        focusPolicy: Qt.NoFocus
        flat: true
        font.pixelSize: 12
        enabled: playToolbar.canPlay
        text: qsTr("Shuffle")

        contentItem: Text {
            text: shuffleButton.text
            font: shuffleButton.font
            color: shuffleButton.enabled
                   ? (shuffleButton.hovered ? Theme.accent : Theme.textSecondary)
                   : Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle { color: "transparent" }
        onClicked: playToolbar.playShuffled()
    }

    Item {
        Layout.fillWidth: true
    }
}