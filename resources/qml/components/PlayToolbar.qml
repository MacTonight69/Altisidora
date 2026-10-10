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

    ThemedButton {
        enabled: playToolbar.canPlay
        text: qsTr("Play all")
        hoverColor: Theme.accent
        onClicked: playToolbar.playFromStart()
    }

    ThemedButton {
        enabled: playToolbar.canPlay
        text: qsTr("Shuffle")
        hoverColor: Theme.accent
        onClicked: playToolbar.playShuffled()
    }

    Item {
        Layout.fillWidth: true
    }
}