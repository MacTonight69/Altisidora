pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: songsView

    // The song list area uses the current theme's page background.
    color: Theme.viewBg

    property var songs: []

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
        for (var i = 0; i < songs.length; ++i) {
            if (songs[i] && songs[i].id !== undefined && songs[i].id === id)
                return songs[i]
        }
        return null
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
        var hi = Math.min(Math.max(fromIndex, toIndex), songs.length - 1)
        clearSelection()
        for (var i = lo; i <= hi; ++i) {
            var song = songs[i]
            if (song && song.id !== undefined)
                selectedIds.add(song.id)
        }
        selectedCount = selectedIds.size
    }

    function selectAll() {
        selectRange(0, songs.length - 1)
    }

    function onRowClicked(index, id, modifiers) {
        var ctrl = (modifiers & Qt.ControlModifier) !== 0
        var shift = (modifiers & Qt.ShiftModifier) !== 0
        if (ctrl) {
            toggle(id)
            return
        }
        if (shift && selectionAnchor >= 0) {
            selectRange(selectionAnchor, index)
            return
        }
        // Plain left click only plays: it never changes the selection on its
        // own, but re-anchors Shift+click ranges. Selection happens with
        // right click, Ctrl+left click, or Shift+left click.
        selectionAnchor = index
        playSong(index)
    }

    // Right click only selects this song (replacing the selection); it
    // never starts playback. A right click on an already-selected song
    // deselects it (toggle off).
    function onRowRightClicked(index, id) {
        if (selectedIds.has(id)) {
            selectedIds.delete(id)
            selectedCount = selectedIds.size
            if (selectedCount === 0)
                selectionAnchor = -1
            return
        }
        clearSelection()
        selectedIds.add(id)
        selectedCount = 1
        selectionAnchor = index
    }

    function playSong(index) {
        var song = songs[index]
        if (!song)
            return
        // The visible list is the navigation context for next/previous.
        Player.setQueue(songs)
        Player.loadSong(song)
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
        var total = Math.floor(Number(ms || 0) / 1000)
        if (!isFinite(total) || total < 0)
            total = 0
        var m = Math.floor(total / 60)
        var s = total % 60
        return m + ":" + (s < 10 ? "0" + s : "" + s)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 0

        // ---- column headers ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: qsTr("Title")
                color: Theme.textMuted
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Artist")
                color: Theme.textMuted
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Album")
                color: Theme.textMuted
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
                text: qsTr("Duration")
                color: Theme.textMuted
                font.pixelSize: 11
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
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            ToolButton {
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

            Button {
                id: hideSelectedButton
                text: qsTr("Hide")
                focusPolicy: Qt.NoFocus
                flat: true
                font.pixelSize: 12
                contentItem: Text {
                    text: hideSelectedButton.text
                    font: hideSelectedButton.font
                    color: hideSelectedButton.hovered ? Theme.danger : Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
                onClicked: {
                    // Hide the selected songs: they stay on disk and in the
                    // database, they just disappear from the lists.
                    var ids = Array.from(songsView.selectedIds)
                    for (var i = 0; i < ids.length; ++i) {
                        var s = songsView.findSongById(ids[i])
                        if (s && s.path)
                            Library.ignorePath(s.path)
                    }
                    songsView.clearSelection()
                }
            }

            Button {
                id: hideFolderButton
                text: qsTr("Hide folder")
                focusPolicy: Qt.NoFocus
                flat: true
                font.pixelSize: 12
                contentItem: Text {
                    text: hideFolderButton.text
                    font: hideFolderButton.font
                    color: hideFolderButton.hovered ? Theme.danger : Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
                onClicked: {
                    // Hide the folder containing each selected song.
                    var seen = new Set()
                    var ids = Array.from(songsView.selectedIds)
                    for (var i = 0; i < ids.length; ++i) {
                        var s = songsView.findSongById(ids[i])
                        if (s && s.path) {
                            var folder = songsView.folderOf(s.path)
                            if (folder && !seen.has(folder)) {
                                seen.add(folder)
                                Library.ignorePath(folder)
                            }
                        }
                    }
                    songsView.clearSelection()
                }
            }

            Button {
                id: clearSelectionButton
                text: qsTr("Clear selection")
                focusPolicy: Qt.NoFocus
                flat: true
                font.pixelSize: 12
                contentItem: Text {
                    text: clearSelectionButton.text
                    font: clearSelectionButton.font
                    color: clearSelectionButton.hovered ? Theme.danger : Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
                onClicked: songsView.clearSelection()
            }
        }

        // ---- song list ----
        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: songsView.songs
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            focus: true
            ScrollBar.vertical: ScrollBar { }

            Keys.onPressed: function (event) {
                if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                    songsView.selectAll()
                    event.accepted = true
                } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_D) {
                    // Delete the selected songs (single or group).
                    if (songsView.selectedCount > 0) {
                        Library.removeSongs(Array.from(songsView.selectedIds))
                        songsView.clearSelection()
                    }
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
                required property var modelData
                property var song: modelData
                property bool isCurrent: Player.currentSong !== undefined
                                         && Player.currentSong !== null
                                         && Player.currentSong.id !== undefined
                                         && Player.currentSong.id === song.id
                // Depends on selectedCount so it re-evaluates when the
                // selection (either as a Set or its size) changes.
                property bool isSelected: songsView.selectedCount > 0
                                          && songsView.selectedIds.has(song.id)

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
                    // Left = play, right = select, Ctrl/Shift+left = multi.
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            songsView.onRowRightClicked(songRow.index, songRow.song.id)
                            return
                        }
                        songsView.onRowClicked(songRow.index, songRow.song.id, mouse.modifiers)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: songRow.song.title ? songRow.song.title : qsTr("Unknown title")
                        // Accent at rest, lighter on hover, selection colour when selected.
                        color: songRow.isSelected ? Theme.selectionText
                               : songMouse.containsMouse ? Theme.accentAltHover : Theme.accentAlt
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: songRow.song.artist ? songRow.song.artist : ""
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: songRow.song.album ? songRow.song.album : ""
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: songsView.formatDuration(songRow.song.duration)
                        color: songRow.isSelected ? Theme.selectionTextMuted : Theme.textMuted
                        font.pixelSize: 12
                    }

                    ToolButton {
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
                            font.pixelSize: 16
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
                            songsView.targetSongIds = [songRow.song.id]
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
        visible: songsView.songs.length === 0
        text: Library.songs.length === 0
              ? (Library.ignoredPaths.length > 0
                 ? qsTr("All songs are hidden — manage them in Settings")
                 : qsTr("No songs yet — press Scan library"))
              : qsTr("No songs match your search")
        color: Theme.textMuted
        font.pixelSize: 15
    }

    // ---- add to playlist menu (acts on songsView.targetSongIds) ----
    Menu {
        id: addToMenu
        title: qsTr("Add to playlist")

        Instantiator {
            model: Playlists.playlists
            delegate: MenuItem {
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
                font.pixelSize: 13
            }

            TextField {
                id: newPlaylistNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                placeholderText: qsTr("Playlist name")
                placeholderTextColor: Theme.textMuted
                color: Theme.textPrimary
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                font.pixelSize: 13
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
}