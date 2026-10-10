pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../js/Utils.js" as Utils

Rectangle {
    id: artistsView
    color: "transparent"

    // The artists to show; main.qml passes the search-filtered list.
    property var artists: Library.artists
    signal openArtist(var artist)

    // All songs of the visible artists (songsByArtist sorts them by album,
    // disc and track within each artist).
    function collectSongs() {
        var out = []
        var list = artistsView.artists
        for (var i = 0; i < list.length; ++i) {
            var songs = Library.songsByArtist(list[i].name ? list[i].name : "")
            for (var j = 0; j < songs.length; ++j)
                out.push(songs[j])
        }
        return out
    }

    function playAllFromStart() {
        var out = artistsView.collectSongs()
        if (out.length === 0)
            return
        Player.setQueue(out)
        Player.loadSong(out[0])
        Player.play()
    }

    function playAllShuffled() {
        var out = artistsView.collectSongs()
        if (out.length === 0)
            return
        out = Utils.shuffle(out)
        Player.setQueue(out)
        Player.loadSong(out[0])
        Player.play()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        // ---- play all / shuffle toolbar ----
        PlayToolbar {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.bottomMargin: 6
            canPlay: artistsView.artists.length > 0
            onPlayFromStart: artistsView.playAllFromStart()
            onPlayShuffled: artistsView.playAllShuffled()
        }

        ListView {
            id: artistList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: artistsView.artists
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                id: artistRow
                required property var modelData
                width: artistList.width
                height: 52
                radius: 6
                color: artistMouse.containsMouse ? Theme.rowHover : "transparent"
                Behavior on color {
                    ColorAnimation { duration: 100 }
                }

                MouseArea {
                    id: artistMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // Drill down: show this artist's songs as a playlist.
                    onClicked: artistsView.openArtist(modelData)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 14
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 17
                        color: Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: modelData.name && modelData.name.length > 0
                                  ? modelData.name.charAt(0).toUpperCase() : "?"
                            color: Theme.accentAlt
                            font.family: Theme.fontFamily
                            font.pixelSize: 15 * Theme.fontScale
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.name
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 14 * Theme.fontScale
                        elide: Text.ElideRight
                    }

                    Text {
                        horizontalAlignment: Text.AlignRight
                        text: modelData.songCount + (modelData.songCount === 1
                                                      ? qsTr(" song") : qsTr(" songs"))
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                    }
                }
            }
        }
    }

    // ---- empty state ----
    Text {
        anchors.centerIn: parent
        visible: artistsView.artists.length === 0
        text: Library.artists.length === 0
              ? qsTr("No artists yet — press Scan library")
              : qsTr("No artists match your search")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 15 * Theme.fontScale
    }
}