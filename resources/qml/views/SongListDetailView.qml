pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Drill-down view opened when clicking an album (in Albums) or an artist
// (in Artists). It shows that collection's songs as a playlist: the song
// rows work exactly like the Songs section (play, multi-select, hide, add
// to playlist) and the playback queue is the visible list.
//
// The song set is resolved at open time via Library.songsByAlbum /
// Library.songsByArtist and re-resolved whenever the library changes, so
// hiding a song while inside the view drops it immediately.
Rectangle {
    id: detail
    color: Theme.viewBg

    // What to show.
    property string filterType: "album" // "album" | "artist"
    property string primaryKey: ""      // album title or artist name
    property string secondaryKey: ""    // album artist (albums only)
    property var modelSongs: []
    // Album rollup rule (DatabaseManager::refreshAlbumArtwork): the header
    // shows the FIRST artwork-bearing song in playback order, not merely the
    // first song — otherwise a track-1-without-art album shows a cover on
    // its card but not in the drill-down header.
    readonly property string headerArtwork: {
        const songs = detail.modelSongs
        for (let i = 0; i < songs.length; ++i) {
            if (songs[i].artwork)
                return songs[i].artwork
        }
        return ""
    }
    // The StackView attached property is unreliable inside descendants of a
    // pushed component; main.qml owns the stack and handles the pop.
    signal backRequested()

    // The embedded song list, exposed so main.qml's global shortcuts
    // (Ctrl+D / Ctrl+H / Ctrl+Shift+H) can act on it while this drill-down
    // view is on top of the stack.
    property alias songsList: songsListView

    function reload() {
        if (detail.filterType === "artist")
            detail.modelSongs = Library.songsByArtist(detail.primaryKey)
        else
            detail.modelSongs = Library.songsByAlbum(detail.primaryKey,
                                                     detail.secondaryKey)
    }

    Component.onCompleted: detail.reload()

    Connections {
        target: Library
        function onLibraryChanged() { detail.reload() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        // ---- header: back button + title + song count ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            spacing: 12

            ToolButton {
                font.family: Theme.fontFamily
                id: backButton
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                padding: 8

                ToolTip.visible: backButton.hovered
                ToolTip.text: qsTr("Back")
                ToolTip.delay: 400

                contentItem: Text {
                    text: "\u2190"
                    color: backButton.down ? Theme.accentPressed
                           : backButton.hovered ? Theme.accentHover : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 18 * Theme.fontScale
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: backButton.hovered ? Theme.rowHover : "transparent"
                }
                onClicked: detail.backRequested()
            }

            // Embedded cover art for this collection, taken from the first
            // artwork-bearing song (the album-card rollup rule). Same
            // fallback pattern as the other views: it collapses to nothing
            // while empty, so the header stays as-is.
            Image {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                visible: detail.headerArtwork !== "" && status === Image.Ready
                source: detail.headerArtwork !== ""
                        ? "file://" + detail.headerArtwork : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                sourceSize.width: 40
                sourceSize.height: 40
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: detail.primaryKey.length > 0
                          ? detail.primaryKey : qsTr("Unknown")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 16 * Theme.fontScale
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: detail.filterType === "album"
                             && detail.secondaryKey.length > 0
                    text: detail.secondaryKey
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 12 * Theme.fontScale
                    elide: Text.ElideRight
                }
            }

            Text {
                text: detail.modelSongs.length
                      + (detail.modelSongs.length === 1
                         ? qsTr(" song") : qsTr(" songs"))
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // ---- the song list, same interactions as the Songs section ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            SongsView {
                id: songsListView
                anchors.fill: parent
                songs: detail.modelSongs
                emptyHint: qsTr("No songs for this %1")
                           .arg(detail.filterType === "artist"
                                ? qsTr("artist") : qsTr("album"))
            }
        }
    }
}