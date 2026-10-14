pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../js/Utils.js" as Utils

Rectangle {
    id: songsView

    // The song list area uses the current theme's page background.
    color: Theme.viewBg

    property var songs: []

    // Optional render model (SongListModel). When set, the ListView binds to
    // it instead of the plain songs array. The delegate reads model.<prop>
    // either way: for a JS array `model` proxies the element, for the C++
    // model it resolves the roleNames() roles.
    property var songsModel: null

    // Optional custom message for the empty state (used by drill-down views
    // when an album/artist has no visible songs).
    property string emptyHint: ""

    // ---- model/array access helpers ----
    // The list is driven either by a role-based SongListModel (the full
    // library or the active search) or by a plain QVariant array (album and
    // artist drill-downs). These accessors let every function below work with
    // both paths without re-wrapping every row into a QVariantMap just to
    // read a field.
    function songCount() { return songsModel ? songsModel.count : songs.length }
    function songAt(i) { return songsModel ? songsModel.songData(i) : songs[i] }
    function songsArray() { return songsModel ? songsModel.toVariantList() : songs }
    // Selection only needs the id, so read that instead of materialising a
    // whole map (and a JS object) per row; "select all" is one call per song.
    function songIdAt(i) {
        if (songsModel)
            return songsModel.idAt(i)
        var song = songs[i]
        return song ? song.id : undefined
    }

    // ---- selection state ----
    property var selectedIds: new Set()
    property int selectedCount: 0
    property int selectionAnchor: -1
    property var targetSongIds: []
    property bool addFromSelection: false

    function clearSelection() {
        selectedIds.clear()
        selectedCount = 0
    }

    function findSongById(id) {
        // Model-backed: one index lookup plus a single map, instead of mapping
        // every row until the id matches (the hide flows call this once per
        // selected id, so the per-row map build must not run per call).
        if (songsModel) {
            var row = songsModel.rowOfId(id)
            return row >= 0 ? songsModel.songData(row) : null
        }
        for (var i = 0; i < songs.length; ++i) {
            var song = songs[i]
            if (song && song.id !== undefined && song.id === id)
                return song
        }
        return null
    }

    // Paths of the given song ids. Model-backed: one pass over the rows, no
    // QVariantMap per row (the array fallback scans the plain array).
    function pathsForIds(ids) {
        if (songsModel)
            return songsModel.pathsForIds(ids)
        var out = []
        for (var i = 0; i < songs.length; ++i) {
            var s = songs[i]
            if (s && s.id !== undefined && ids.indexOf(s.id) >= 0 && s.path)
                out.push(s.path)
        }
        return out
    }

    function folderOf(path) {
        var p = String(path)
        var i = p.lastIndexOf("/")
        return i > 0 ? p.substring(0, i) : p
    }

    function toggle(id) {
        if (selectedIds.has(id))
            selectedIds.delete(id)
        else
            selectedIds.add(id)
        selectedCount = selectedIds.size
    }

    function selectRange(fromIndex, toIndex) {
        var lo = Math.min(fromIndex, toIndex)
        var hi = Math.min(Math.max(fromIndex, toIndex), songCount() - 1)
        clearSelection()
        for (var i = lo; i <= hi; ++i) {
            var id = songIdAt(i)
            if (id !== undefined && id !== null && id >= 0)
                selectedIds.add(id)
        }
        selectedCount = selectedIds.size
    }

    function selectAll() {
        selectRange(0, songCount() - 1)
    }

    // Left click plays the song and nothing else: selection is entirely
    // right-click driven (see onRowRightClicked).
    function onRowClicked(index) {
        playSong(index)
    }

    // Selection lives on the right button: a plain right click selects this
    // song (replacing the selection), Ctrl+right click toggles it, and
    // Shift+right click extends the range from the anchor. A plain right
    // click on an already-selected song deselects it (toggle off). Returns
    // true only for a plain right click that selected the row, which is the
    // case that opens the per-row actions menu.
    function onRowRightClicked(index, id, modifiers) {
        var ctrl = (modifiers & Qt.ControlModifier) !== 0
        var shift = (modifiers & Qt.ShiftModifier) !== 0
        if (ctrl) {
            toggle(id)
            return false
        }
        if (shift && selectionAnchor >= 0) {
            selectRange(selectionAnchor, index)
            return false
        }
        if (selectedIds.has(id)) {
            selectedIds.delete(id)
            selectedCount = selectedIds.size
            if (selectedCount === 0)
                selectionAnchor = -1
            return false
        }
        clearSelection()
        selectedIds.add(id)
        selectedCount = 1
        selectionAnchor = index
        return true
    }

    function playSong(index) {
        var song = songAt(index)
        if (!song)
            return
        // The visible list is the navigation context for next/previous.
        Player.setQueue(songsArray())
        Player.loadSong(song)
        Player.play()
    }

    // Play the whole visible list from the first song (in list order).
    function playAllFromStart() {
        if (songCount() === 0)
            return
        Player.setQueue(songsArray())
        Player.loadSong(songAt(0))
        Player.play()
    }

    // Play the whole visible list in a random order. The shuffle happens on
    // a copy: the list itself (and its selection) is never reordered, and
    // next()/previous() afterwards navigate the shuffled order.
    function playAllShuffled() {
        if (songCount() === 0)
            return
        var copy = Utils.shuffle(songsArray())
        Player.setQueue(copy)
        Player.loadSong(copy[0])
        Player.play()
    }

    // Create the playlist, add the pending targets and (when the add came
    // from the selection bar) clear the selection.
    function addTargetSongsToPlaylist(name) {
        Playlists.createPlaylist(name)
        var list = Playlists.playlists
        for (var i = 0; i < list.length; ++i) {
            if (list[i].name === name) {
                Playlists.addSongs(list[i].id, targetSongIds)
                break
            }
        }
        if (addFromSelection)
            clearSelection()
        targetSongIds = []
    }

    function formatDuration(ms) {
        return Utils.formatDuration(ms)
    }

    // Delete the selected songs. Both modes confirm first: discarding the
    // file from disk is irreversible, and removing it from the library
    // hides it from future scans until it is restored in Settings. These
    // actions are reusable from the row menu, the toolbar buttons and the
    // global Ctrl+D / Ctrl+H / Ctrl+Shift+H shortcuts in main.qml.
    function applyDelete(deleteFiles) {
        if (songsView.selectedCount === 0)
            return
        Library.removeSongs(Array.from(songsView.selectedIds), deleteFiles)
        songsView.clearSelection()
    }

    function deleteSelected() {
        if (songsView.selectedCount === 0)
            return
        if (AppSettings.deleteFilesFromDisk)
            deleteFromDiskDialog.open()        // irreversibly deletes files
        else
            deleteFromLibraryDialog.open()     // reversible: files stay on disk
    }

    // Hide the selected songs: they stay on disk and in the database, they
    // just disappear from the lists (restore them in Settings). One batch
    // hide and one refresh for the whole selection, not one per song.
    function hideSelected() {
        if (songsView.selectedCount === 0)
            return
        var paths = songsView.pathsForIds(Array.from(songsView.selectedIds))
        if (paths.length > 0)
            Library.ignorePaths(paths)
        songsView.clearSelection()
    }

    // Hide the folder containing each selected song (deduplicated).
    function hideSelectedFolders() {
        if (songsView.selectedCount === 0)
            return
        var paths = songsView.pathsForIds(Array.from(songsView.selectedIds))
        var folders = []
        var seen = {}
        for (var i = 0; i < paths.length; ++i) {
            var folder = songsView.folderOf(paths[i])
            if (folder && !seen[folder]) {
                seen[folder] = true
                folders.push(folder)
            }
        }
        if (folders.length > 0)
            Library.ignorePaths(folders)
        songsView.clearSelection()
    }

    // ---- tag editing (shared EditSongDialog) ----

    // Open the shared edit dialog on one song (row menu -> "Edit tags…").
    function editSong(id) {
        var song = findSongById(id)
        if (!song)
            return
        editSongDialog.mode = "single"
        editSongDialog.songMap = song
        editSongDialog.batchIds = []
        editSongDialog.open()
    }

    // Open the shared edit dialog in a batch mode: reassigning ONE field
    // (album or artist) across the whole selection. The dialog builds
    // minimal {id, field} maps, which is what structurally prevents a
    // batch edit from equalizing any other column.
    function startBatchEdit(field) {
        if (songsView.selectedCount === 0)
            return
        editSongDialog.mode = field
        editSongDialog.songMap = ({})
        editSongDialog.batchIds = Array.from(songsView.selectedIds)
        editSongDialog.open()
    }

    // Open the per-row actions menu at the right-click position. The
    // delegate calls this only after a plain right click selected the row
    // (Ctrl/Shift right clicks adjust the selection without a menu, and a
    // plain right click on a selected row keeps the documented toggle-off
    // behaviour and deselects without any menu), so the row's model is the
    // menu's target.
    function openRowMenu(row, x, y) {
        if (!row || row.model === undefined || row.model.id === undefined)
            return
        rowMenu.songId = row.model.id
        var pos = row.mapToItem(songsView, x, y)
        rowMenu.popup(pos.x, pos.y)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        // ---- play all / shuffle toolbar ----
        PlayToolbar {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.topMargin: 2
            canPlay: songsView.songCount() > 0
            onPlayFromStart: songsView.playAllFromStart()
            onPlayShuffled: songsView.playAllShuffled()
        }

        // ---- column headers ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: qsTr("Title")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Artist")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Album")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
            Text {
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
                text: qsTr("Duration")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // ---- selection toolbar (visible when songs are selected) ----
        RowLayout {
            id: selectionBar
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            Layout.topMargin: 8
            spacing: 10
            visible: songsView.selectedCount > 0

            Text {
                Layout.fillWidth: true
                text: qsTr("%n song(s) selected", "", songsView.selectedCount)
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
                elide: Text.ElideRight
            }

            ToolButton {
                font.family: Theme.fontFamily
                id: addSelectedButton
                text: qsTr("+ Playlist")
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                padding: 8
                contentItem: Text {
                    text: addSelectedButton.text
                    font: addSelectedButton.font
                    color: Theme.accentText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    color: addSelectedButton.hovered ? Theme.accentHover : Theme.accent
                }
                onClicked: {
                    songsView.addFromSelection = true
                    songsView.targetSongIds = Array.from(songsView.selectedIds)
                    addToMenu.popup()
                }
            }

            ThemedButton {
                text: qsTr("Set album…")
                onClicked: songsView.startBatchEdit("album")
            }

            ThemedButton {
                text: qsTr("Set artist…")
                onClicked: songsView.startBatchEdit("artist")
            }

            ThemedButton {
                text: qsTr("Hide")
                onClicked: songsView.hideSelected()
            }

            ThemedButton {
                text: qsTr("Hide folder")
                onClicked: songsView.hideSelectedFolders()
            }

            ThemedButton {
                text: qsTr("Clear selection")
                onClicked: songsView.clearSelection()
            }
        }

        // ---- song list ----
        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: songsView.songsModel ? songsView.songsModel : songsView.songs
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            focus: true
            ScrollBar.vertical: ScrollBar { }

            Keys.onPressed: function (event) {
                if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                    songsView.selectAll()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    songsView.clearSelection()
                    event.accepted = true
                }
            }

            delegate: Rectangle {
                id: songRow
                width: songList.width
                height: 40
                radius: 6

                required property int index
                required property var model
                // model.<prop> resolves each element's property: for plain
                // JS arrays `model` proxies the element, for SongListModel
                // it resolves the named roles (no per-binding map wrap).
                property bool isCurrent: Player.currentSong !== undefined
                                         && Player.currentSong !== null
                                         && Player.currentSong.id !== undefined
                                         && Player.currentSong.id === model.id
                // Depends on selectedCount so it re-evaluates when the
                // selection (either as a Set or its size) changes.
                property bool isSelected: songsView.selectedCount > 0
                                          && songsView.selectedIds.has(model.id)

                color: isSelected ? Theme.selection
                       : isCurrent ? Theme.rowCurrent
                       : songMouse.containsMouse ? Theme.rowHover : "transparent"
                border.color: isSelected ? Theme.selectionBorder : "transparent"
                border.width: isSelected ? 2 : 0
                Behavior on color {
                    ColorAnimation { duration: 100 }
                }

                MouseArea {
                    id: songMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    // Left = play; right = select, with Ctrl to toggle and
                    // Shift to extend the range.
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            // A plain right click selects the row and opens
                            // the song actions menu; Ctrl/Shift+right click
                            // only adjust the selection, and a plain right
                            // click on a selected row toggles it off.
                            if (songsView.onRowRightClicked(songRow.index, model.id,
                                                            mouse.modifiers))
                                songsView.openRowMenu(songRow, mouse.x, mouse.y)
                            return
                        }
                        songsView.onRowClicked(songRow.index)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: model.title ? model.title : qsTr("Unknown title")
                        // Accent at rest, lighter on hover, selection colour when selected.
                        color: songRow.isSelected ? Theme.selectionText
                               : songMouse.containsMouse ? Theme.accentAltHover : Theme.accentAlt
                        font.family: Theme.fontFamily
                        font.pixelSize: 13 * Theme.fontScale
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: model.artist ? model.artist : ""
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: model.album ? model.album : ""
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: songsView.formatDuration(model.duration)
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                    }

                    ToolButton {
                        font.family: Theme.fontFamily
                        id: addToButton
                        visible: songMouse.containsMouse
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: true
                        padding: 4

                        ToolTip.visible: addToButton.hovered
                        ToolTip.text: qsTr("Add to playlist")
                        ToolTip.delay: 400

                        contentItem: Text {
                            text: qsTr("+")
                            color: addToButton.down ? Theme.accentAltPressed
                                   : addToButton.hovered ? Theme.accentAltHover : Theme.accentAlt
                            font.family: Theme.fontFamily
                            font.pixelSize: 16 * Theme.fontScale
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: addToButton.hovered ? Theme.border : "transparent"
                        }
                        onClicked: {
                            // Single-song add always targets just this row.
                            songsView.addFromSelection = false
                            songsView.targetSongIds = [model.id]
                            addToMenu.popup()
                        }
                    }
                }
            }
        }
    }

    // ---- empty state ----
    Text {
        anchors.centerIn: parent
        visible: songsView.songCount() === 0
        text: songsView.emptyHint.length > 0
              ? songsView.emptyHint
              : Library.songCount() === 0
                ? (Library.ignoredPaths.length > 0
                   ? qsTr("All songs are hidden — manage them in Settings")
                   : qsTr("No songs yet — press Scan library"))
                : qsTr("No songs match your search")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 15 * Theme.fontScale
    }

    // ---- add to playlist menu (acts on songsView.targetSongIds) ----
    Menu {
        id: addToMenu
        title: qsTr("Add to playlist")

        Instantiator {
            model: Playlists.playlists
            delegate: MenuItem {
                font.family: Theme.fontFamily
                required property var modelData
                text: modelData.name
                onTriggered: {
                    Playlists.addSongs(modelData.id, songsView.targetSongIds)
                    // Batch add from the selection bar: the added songs
                    // leave the selection.
                    if (songsView.addFromSelection)
                        songsView.clearSelection()
                }
            }
            onObjectAdded: (index, object) => addToMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => addToMenu.removeItem(object)
        }

        MenuItem {
            text: qsTr("New playlist…")
            onTriggered: {
                newPlaylistNameField.text = qsTr("Playlist %1")
                                            .arg(Playlists.playlists.length + 1)
                newPlaylistDialog.open()
            }
        }
    }

    // ---- per-row actions (right click that selects the row) ----
    // Sibling of addToMenu, not a submenu: items live directly in this
    // menu, never inside addToMenu's Instantiator (whose index-based
    // insert/remove is only stable for the playlist entries it manages).
    Menu {
        id: rowMenu

        property int songId: -1

        MenuItem {
            font.family: Theme.fontFamily
            text: qsTr("Edit tags…")
            onTriggered: songsView.editSong(rowMenu.songId)
        }

        MenuItem {
            font.family: Theme.fontFamily
            text: qsTr("Add to playlist…")
            onTriggered: {
                // Same flow as the row's "+" button: a single-song add
                // that targets only this row.
                songsView.addFromSelection = false
                songsView.targetSongIds = [rowMenu.songId]
                addToMenu.popup()
            }
        }

        MenuSeparator { }

        MenuItem {
            font.family: Theme.fontFamily
            text: qsTr("Delete…")
            onTriggered: {
                // The menu only opens after a plain right click selected the
                // row (openRowMenu), so the selection is exactly this song.
                // Reusing deleteSelected() keeps the Settings-driven
                // disk-vs-library choice and its confirmation dialog identical
                // to Ctrl+D.
                songsView.deleteSelected()
            }
        }
    }

    // ---- name prompt for "New playlist…" ----
    Dialog {
        id: newPlaylistDialog
        title: qsTr("New playlist")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: newPlaylistNameField.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: 10
            implicitWidth: 320

            Text {
                text: qsTr("Name for the new playlist:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            TextField {
                font.family: Theme.fontFamily
                id: newPlaylistNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                placeholderText: qsTr("Playlist name")
                placeholderTextColor: Theme.textMuted
                color: Theme.textPrimary
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                font.pixelSize: 13 * Theme.fontScale
                leftPadding: 10
                rightPadding: 10
                onAccepted: newPlaylistDialog.accept()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: newPlaylistNameField.activeFocus ? Theme.accent : Theme.border
                }
            }
        }

        onAccepted: {
            // Empty input falls back to a generated name so the batch add
            // still happens.
            var name = newPlaylistNameField.text.trim()
            if (name.length === 0)
                name = qsTr("Playlist %1").arg(Playlists.playlists.length + 1)
            songsView.addTargetSongsToPlaylist(name)
        }
    }

    // ---- confirmations before deleting the selected songs ----
    Dialog {
        id: deleteFromDiskDialog
        title: qsTr("Delete from disk")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        // Explicit width: sizing the dialog from the wrapped content re-enters
        // the Fusion style's implicitWidth binding and warns about a binding
        // loop.
        width: 340

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("%n file(s) will be permanently deleted from disk. This cannot be undone.", "",
                           songsView.selectedCount)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("The selected songs are also removed from the library and from any playlist.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
        }

        onAccepted: songsView.applyDelete(true)
    }

    // Removing from the library is reversible, but the files are hidden from
    // every list and from future scans until they are restored in Settings,
    // so this mode confirms too.
    Dialog {
        id: deleteFromLibraryDialog
        title: qsTr("Remove from library")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        // Same explicit width as the sibling dialog, for the same reason.
        width: 340

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("%n song(s) will be removed from the library and from any playlist.", "",
                           songsView.selectedCount)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("The audio files stay on disk and are skipped by future scans. Restore them from Settings, under “Hidden items”.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11 * Theme.fontScale
            }
        }

        onAccepted: songsView.applyDelete(false)
    }

    // ---- shared tag editor (single song + batch album/artist) ----
    // A successful edit can move songs out of the visible list (search
    // filter, drill-down), so the selection is dropped; the dialog itself
    // only reports success via editApplied and never touches selection.
    EditSongDialog {
        id: editSongDialog
        onEditApplied: songsView.clearSelection()
    }
}