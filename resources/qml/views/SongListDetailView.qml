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
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: backButton.hovered ? Theme.rowHover : "transparent"
                }
                onClicked: {
                    var stack = StackView.view
                    if (stack)
                        stack.pop()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: detail.primaryKey.length > 0
                          ? detail.primaryKey : qsTr("Unknown")
                    color: Theme.textPrimary
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: detail.filterType === "album"
                             && detail.secondaryKey.length > 0
                    text: detail.secondaryKey
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Text {
                text: detail.modelSongs.length
                      + (detail.modelSongs.length === 1
                         ? qsTr(" song") : qsTr(" songs"))
                color: Theme.textMuted
                font.pixelSize: 12
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
                anchors.fill: parent
                songs: detail.modelSongs
                emptyHint: qsTr("No songs for this %1")
                           .arg(detail.filterType === "artist"
                                ? qsTr("artist") : qsTr("album"))
            }
        }
    }
}