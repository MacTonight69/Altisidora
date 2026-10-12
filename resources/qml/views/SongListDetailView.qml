pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "../js/Utils.js" as Utils

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

            // Edit this collection after a confirmation dialog: an artist
            // rename, or an album edit (title, artist and cover).
            ToolButton {
                font.family: Theme.fontFamily
                id: renameButton
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                padding: 8

                ToolTip.visible: renameButton.hovered
                ToolTip.text: detail.filterType === "artist"
                              ? qsTr("Rename") : qsTr("Edit")
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

    // ---- inline edit (rewrites the collection's songs) ----
    // No standardButtons: a refused edit (collision, failure) must keep the
    // dialog open, which the stock Ok button's auto-close breaks.
    //
    // Artist collection: rename the artist globally (Library.renameArtist).
    // Album collection: edit the album's title, artist and cover. The artist
    // field moves ONLY this album (Library.setAlbumIdentity, one atomic call
    // with the title so a combined edit cannot be refused by an intermediate
    // state) — the global rename stays in the Artists section — and the
    // cover applies to every song of the album (Library.setAlbumArtwork),
    // the explicit "this album's artwork is X" intent.
    Dialog {
        id: renameDialog
        modal: true
        title: detail.filterType === "artist" ? qsTr("Rename artist")
                                               : qsTr("Edit album")

        // Transient album-edit state, reset on every open (onOpened below).
        property string pickedArtwork: ""
        property bool removeArtwork: false

        // What the cover preview shows: a fresh pick wins over the stored
        // cover and "Remove cover" suppresses both, so the preview follows
        // the pending edit. detail.headerArtwork is the album rollup rule
        // (the first artwork-bearing song in playback order).
        readonly property string effectiveArtwork:
            renameDialog.removeArtwork ? ""
            : renameDialog.pickedArtwork !== "" ? renameDialog.pickedArtwork
            : detail.headerArtwork

        onOpened: {
            renameError.text = ""
            renameField.text = detail.primaryKey
            renameDialog.pickedArtwork = ""
            renameDialog.removeArtwork = false
            if (detail.filterType === "album")
                albumArtistField.text = detail.secondaryKey
            renameField.forceActiveFocus()
        }

        // A collision pre-check runs before the backend calls because the
        // backend only returns false (no reason). The album target compares
        // BOTH names, so an edit that changes title and artist together
        // cannot land on an identity that already exists, and the album
        // being edited never counts as its own collision (that is what keeps
        // a case-only rename working).
        function hasArtistCollision(target) {
            var artists = Library.artists
            for (var i = 0; i < artists.length; ++i) {
                if (artists[i].name === target)
                    return true
            }
            return false
        }

        function hasAlbumCollision(targetTitle, targetArtist) {
            if (targetTitle === detail.primaryKey
                    && targetArtist === detail.secondaryKey)
                return false
            var albums = Library.albums
            for (var j = 0; j < albums.length; ++j) {
                if (albums[j].title === targetTitle
                        && albums[j].artist === targetArtist)
                    return true
            }
            return false
        }

        function apply() {
            var from = detail.primaryKey
            var target = renameField.text.trim()
            if (target.length === 0)
                return
            renameError.text = ""

            if (detail.filterType === "artist") {
                // Unchanged name is a no-op, not a failure: the backend
                // refuses from == to, which would otherwise surface a
                // misleading "Could not rename" error.
                if (target === from) {
                    renameDialog.close()
                    return
                }
                if (renameDialog.hasArtistCollision(target)) {
                    renameError.text = qsTr("An artist with this name already exists.")
                    return
                }
                // Move the stored key BEFORE the write: renameArtist emits
                // libraryChanged synchronously (refresh() inside the call),
                // and the onLibraryChanged reload() above resolves songs by
                // this key while the call is still running. Resolving by the
                // old name at that instant would empty the view. A refused
                // rename writes nothing and emits nothing, so restoring the
                // old key on failure is safe.
                detail.primaryKey = target
                if (!Library.renameArtist(from, target)) {
                    detail.primaryKey = from
                    renameError.text = qsTr("Could not rename. Try again.")
                    return
                }
                renameDialog.close()
                return
            }

            // ---- album edit: title, artist and cover ----
            var fromArtist = detail.secondaryKey
            var newArtist = albumArtistField.text.trim()
            if (newArtist !== fromArtist && newArtist.length === 0) {
                renameError.text = qsTr("The artist cannot be empty.")
                return
            }
            var titleChanged = target !== from
            var artistChanged = newArtist !== fromArtist
            var artworkChanged = renameDialog.removeArtwork
                                 || renameDialog.pickedArtwork !== ""
            if (!titleChanged && !artistChanged && !artworkChanged) {
                renameDialog.close()
                return
            }
            if (renameDialog.hasAlbumCollision(target, newArtist)) {
                renameError.text = qsTr("An album with this title already exists for this artist.")
                return
            }
            // Title and artist are ONE backend call: an edit that changes
            // both must not be refused by an intermediate state that the
            // final identity never has (see setAlbumIdentity). Move the
            // stored keys BEFORE the write, so the synchronous refresh()
            // inside the call (libraryChanged -> reload()) already resolves
            // the album by the name the database has at that instant and
            // the view never blanks out mid-edit. A refused call writes
            // nothing, so restoring the keys on failure is safe.
            if (titleChanged || artistChanged) {
                detail.primaryKey = target
                detail.secondaryKey = newArtist
                if (!Library.setAlbumIdentity(from, fromArtist,
                                              target, newArtist)) {
                    detail.primaryKey = from
                    detail.secondaryKey = fromArtist
                    renameError.text = qsTr("Could not save the changes. Try again.")
                    return
                }
            }
            if (artworkChanged) {
                var artPath = renameDialog.removeArtwork
                        ? "" : renameDialog.pickedArtwork
                if (!Library.setAlbumArtwork(detail.primaryKey,
                                             detail.secondaryKey, artPath)) {
                    renameError.text = qsTr("Could not save the artwork. Try again.")
                    return
                }
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

            // The album's artist is editable and moves THIS album only; the
            // global rename stays in the Artists section.
            Text {
                Layout.fillWidth: true
                visible: detail.filterType === "album"
                text: qsTr("Artist:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            TextField {
                id: albumArtistField
                visible: detail.filterType === "album"
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: qsTr("Artist")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: renameDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: albumArtistField.activeFocus ? Theme.accent : Theme.border
                }
            }

            // ---- album cover (applies to every song of the album) ----
            RowLayout {
                visible: detail.filterType === "album"
                Layout.fillWidth: true
                spacing: 8

                Image {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    visible: status === Image.Ready
                    source: renameDialog.effectiveArtwork !== ""
                            ? "file://" + renameDialog.effectiveArtwork : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    sourceSize.width: 56
                    sourceSize.height: 56
                }

                Button {
                    font.family: Theme.fontFamily
                    id: chooseAlbumArtworkButton
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Choose cover…")
                    onClicked: albumArtworkFileDialog.open()
                }

                Button {
                    font.family: Theme.fontFamily
                    id: removeAlbumArtworkButton
                    focusPolicy: Qt.NoFocus
                    visible: detail.headerArtwork !== ""
                             || renameDialog.pickedArtwork !== ""
                    text: qsTr("Remove cover")
                    onClicked: {
                        renameDialog.removeArtwork = true
                        renameDialog.pickedArtwork = ""
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: detail.filterType === "artist"
                       ? qsTr("This renames %n song(s)", "", detail.modelSongs.length)
                       : qsTr("Applies to %n song(s)", "", detail.modelSongs.length)
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

    // ---- album cover picker (built-in QML dialog; native dialogs are
    //      disabled app-wide in main.cpp) ----
    FileDialog {
        id: albumArtworkFileDialog
        title: qsTr("Choose cover")
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            renameDialog.pickedArtwork = Utils.urlToPath(selectedFile)
            renameDialog.removeArtwork = false
        }
    }
}