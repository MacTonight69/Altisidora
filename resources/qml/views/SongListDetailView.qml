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

            // Rename this artist / album: rewrites every song in the
            // collection after a confirmation dialog.
            ToolButton {
                font.family: Theme.fontFamily
                id: renameButton
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                padding: 8

                ToolTip.visible: renameButton.hovered
                ToolTip.text: qsTr("Rename")
                ToolTip.delay: 400

                contentItem: Text {
                    text: "\u270e"
                    color: renameButton.down ? Theme.accentPressed
                           : renameButton.hovered ? Theme.accentHover : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 18 * Theme.fontScale
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: renameButton.hovered ? Theme.rowHover : "transparent"
                }
                onClicked: renameDialog.open()
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

    // ---- inline rename (rewrites every song in this collection) ----
    // No standardButtons: a refused rename (collision, failure) must keep
    // the dialog open, which the stock Ok button's auto-close breaks.
    Dialog {
        id: renameDialog
        modal: true
        title: detail.filterType === "artist" ? qsTr("Rename artist")
                                               : qsTr("Rename album")

        onOpened: {
            renameError.text = ""
            renameField.text = detail.primaryKey
            renameField.forceActiveFocus()
        }

        // A collision pre-check runs before the backend call because the
        // backend only returns false (no reason). It is skipped when the
        // typed name equals the current one, so a case-only rename is
        // never blocked by finding the very collection being renamed.
        function hasCollision(target) {
            if (detail.filterType === "artist") {
                var artists = Library.artists
                for (var i = 0; i < artists.length; ++i) {
                    if (artists[i].name === target)
                        return true
                }
            } else {
                var albums = Library.albums
                for (var j = 0; j < albums.length; ++j) {
                    if (albums[j].title === target
                            && albums[j].artist === detail.secondaryKey)
                        return true
                }
            }
            return false
        }

        function apply() {
            var from = detail.primaryKey
            var target = renameField.text.trim()
            if (target.length === 0)
                return
            // Unchanged name is a no-op, not a failure: the backend
            // refuses from == to, which would otherwise surface a
            // misleading "Could not rename" error.
            if (target === from) {
                renameDialog.close()
                return
            }
            renameError.text = ""
            if (renameDialog.hasCollision(target)) {
                renameError.text = detail.filterType === "artist"
                        ? qsTr("An artist with this name already exists.")
                        : qsTr("An album with this title already exists for this artist.")
                return
            }
            // Move the stored key BEFORE the write: renameArtist and
            // renameAlbum emit libraryChanged synchronously (refresh()
            // inside the call), and the onLibraryChanged reload() above
            // resolves songs by this key while the call is still
            // running. Resolving by the old name at that instant would
            // empty the view. A refused rename writes nothing and emits
            // nothing, so restoring the old key on failure is safe.
            detail.primaryKey = target
            var ok = detail.filterType === "artist"
                    ? Library.renameArtist(from, target)
                    : Library.renameAlbum(from, detail.secondaryKey, target)
            if (!ok) {
                detail.primaryKey = from
                renameError.text = qsTr("Could not rename. Try again.")
                return
            }
            renameDialog.close()
        }

        contentItem: ColumnLayout {
            spacing: 10
            // Constant width like every other dialog: a contentItem that
            // references its own implicitWidth is the binding-loop trap.
            implicitWidth: 380

            Text {
                Layout.fillWidth: true
                text: detail.filterType === "artist"
                       ? qsTr("New artist name:") : qsTr("New album title:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            TextField {
                id: renameField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: renameDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: renameField.activeFocus ? Theme.accent : Theme.border
                }
            }

            // The artist is fixed when renaming an album: renaming never
            // moves the album to another artist.
            Text {
                Layout.fillWidth: true
                visible: detail.filterType === "album"
                         && detail.secondaryKey.length > 0
                text: qsTr("Artist: %1").arg(detail.secondaryKey)
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("This renames %n song(s)", "", detail.modelSongs.length)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }

            Text {
                id: renameError
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.Wrap
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                Button {
                    font.family: Theme.fontFamily
                    id: renameCancelButton
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Cancel")
                    onClicked: renameDialog.close()
                }

                Button {
                    font.family: Theme.fontFamily
                    id: renameSaveButton
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Save")
                    enabled: renameField.text.trim().length > 0
                    onClicked: renameDialog.apply()
                }
            }
        }
    }
}