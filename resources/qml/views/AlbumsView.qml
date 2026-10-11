pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../js/Utils.js" as Utils

Rectangle {
    id: albumsView
    color: "transparent"

    // The albums to show; defaults to the whole library so the view also
    // works standalone. main.qml passes the search-filtered list.
    property var albums: Library.albums
    signal openAlbum(var album)

    // All songs of the visible albums, respecting each album's disc/track
    // order. Resolved in ONE pass over the already-loaded library
    // (Library.songs excludes hidden songs) instead of one SQL query per
    // album, which the old per-album songsByAlbum() loop did.
    function collectSongs() {
        var buckets = {}
        var all = Library.songs
        for (var i = 0; i < all.length; ++i) {
            var s = all[i]
            var key = String(s.album ? s.album : "") + "\u0000"
                    + String(s.artist ? s.artist : "")
            if (!buckets[key])
                buckets[key] = []
            buckets[key].push(s)
        }
        var out = []
        var list = albumsView.albums
        for (var j = 0; j < list.length; ++j) {
            var album = list[j]
            var songKey = String(album.title ? album.title : "") + "\u0000"
                        + String(album.artist ? album.artist : "")
            var songs = buckets[songKey]
            if (!songs)
                continue
            // Restore the songsByAlbum() ordering: disc, track, title.
            songs.sort(function (a, b) {
                return (a.discNumber - b.discNumber)
                       || (a.trackNumber - b.trackNumber)
                       || String(a.title).localeCompare(String(b.title))
            })
            for (var m = 0; m < songs.length; ++m)
                out.push(songs[m])
        }
        return out
    }

    function playAllFromStart() {
        var out = albumsView.collectSongs()
        if (out.length === 0)
            return
        Player.setQueue(out)
        Player.loadSong(out[0])
        Player.play()
    }

    function playAllShuffled() {
        var out = albumsView.collectSongs()
        if (out.length === 0)
            return
        out = Utils.shuffle(out)
        Player.setQueue(out)
        Player.loadSong(out[0])
        Player.play()
    }

    // Stable placeholder colour derived from a hash of the album title.
    function albumColor(title) {
        var s = (title === undefined || title === null) ? "" : String(title)
        var hash = 0
        for (var i = 0; i < s.length; ++i)
            hash = (hash * 31 + s.charCodeAt(i)) >>> 0

        var hue = (hash % 360) / 60
        var sat = 0.45
        var lig = 0.55
        var c = (1 - Math.abs(2 * lig - 1)) * sat
        var x = c * (1 - Math.abs((hue % 2) - 1))
        var m = lig - c / 2
        var r, g, b

        if (hue < 1)      { r = c; g = x; b = 0 }
        else if (hue < 2) { r = x; g = c; b = 0 }
        else if (hue < 3) { r = 0; g = c; b = x }
        else if (hue < 4) { r = 0; g = x; b = c }
        else if (hue < 5) { r = x; g = 0; b = c }
        else              { r = c; g = 0; b = x }

        return Qt.rgba(r + m, g + m, b + m, 1.0)
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
            canPlay: albumsView.albums.length > 0
            onPlayFromStart: albumsView.playAllFromStart()
            onPlayShuffled: albumsView.playAllShuffled()
        }

        GridView {
            id: albumGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: albumsView.albums
            cellWidth: 176
            cellHeight: 240
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                required property var modelData
                width: albumGrid.cellWidth
                height: albumGrid.cellHeight
                color: "transparent"

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    radius: 6
                    color: albumMouse.containsMouse ? Theme.rowHover : Theme.bgAlt
                    border.width: 1
                    border.color: albumMouse.containsMouse ? Theme.accent : Theme.border

                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: 120 }
                    }

                    MouseArea {
                        id: albumMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        // Drill down: show this album's songs as a playlist.
                        onClicked: albumsView.openAlbum(modelData)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            radius: 6
                            color: albumsView.albumColor(modelData.title)

                            Text {
                                anchors.centerIn: parent
                                text: "\u266A"
                                color: "#ffffff"
                                opacity: 0.85
                                font.family: Theme.fontFamily
                                font.pixelSize: 36 * Theme.fontScale
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.title ? modelData.title : qsTr("Unknown album")
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 13 * Theme.fontScale
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.artist ? modelData.artist : ""
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 11 * Theme.fontScale
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.year ? String(modelData.year) : qsTr("Unknown year")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11 * Theme.fontScale
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // ---- empty state ----
    Text {
        anchors.centerIn: parent
        visible: albumsView.albums.length === 0
        text: Library.albums.length === 0
              ? qsTr("No albums yet — press Scan library")
              : qsTr("No albums match your search")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 15 * Theme.fontScale
    }
}