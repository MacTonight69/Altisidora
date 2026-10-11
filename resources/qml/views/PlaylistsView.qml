pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../js/Utils.js" as Utils

Rectangle {
    id: playlistsView

    color: "transparent"

    property int selectedPlaylistId: -1
    property var playlistSongs: []

    readonly property string selectedName: {
        var list = Playlists.playlists
        for (var i = 0; i < list.length; ++i) {
            if (list[i].id === selectedPlaylistId)
                return list[i].name
        }
        return ""
    }

    function reloadSongs() {
        playlistSongs = selectedPlaylistId >= 0
                        ? Playlists.getPlaylistSongs(selectedPlaylistId) : []
        playlistsView.clearSongSelection()
    }

    onSelectedPlaylistIdChanged: reloadSongs()

    function formatDuration(ms) {
        return Utils.formatDuration(ms)
    }

    // Play the playlist from the beginning, in its stored order.
    function playPlaylist() {
        if (playlistSongs.length === 0)
            return
        Player.setQueue(playlistSongs)
        Player.loadSong(playlistSongs[0])
        Player.play()
    }

    // Play the whole playlist in random order (Fisher–Yates over a copy).
    // next()/previous() afterwards navigate the shuffled order.
    function playShuffled() {
        if (playlistSongs.length === 0)
            return
        var songs = Utils.shuffle(playlistSongs)
        Player.setQueue(songs)
        Player.loadSong(songs[0])
        Player.play()
    }

    // ---- playlist song selection (mirrors the Songs section) ----
    property var selectedSongIds: new Set()
    property int selectedSongCount: 0
    property int songSelectionAnchor: -1

    function clearSongSelection() {
        selectedSongIds.clear()
        selectedSongCount = 0
        songSelectionAnchor = -1
    }

    function findPlaylistSongById(id) {
        var list = playlistSongs
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i].id !== undefined && list[i].id === id)
                return list[i]
        }
        return null
    }

    function songFolderOf(path) {
        var p = String(path)
        var i = p.lastIndexOf("/")
        return i > 0 ? p.substring(0, i) : p
    }

    function toggleSong(id) {
        if (selectedSongIds.has(id))
            selectedSongIds.delete(id)
        else
            selectedSongIds.add(id)
        selectedSongCount = selectedSongIds.size
    }

    function selectSongRange(fromIndex, toIndex) {
        var lo = Math.min(fromIndex, toIndex)
        var hi = Math.min(Math.max(fromIndex, toIndex), playlistSongs.length - 1)
        clearSongSelection()
        for (var i = lo; i <= hi; ++i) {
            var s = playlistSongs[i]
            if (s && s.id !== undefined)
                selectedSongIds.add(s.id)
        }
        selectedSongCount = selectedSongIds.size
    }

    function selectAllSongs() {
        selectSongRange(0, playlistSongs.length - 1)
    }

    function onPlaylistRowClicked(index, id, modifiers) {
        var ctrl = (modifiers & Qt.ControlModifier) !== 0
        var shift = (modifiers & Qt.ShiftModifier) !== 0
        if (ctrl) {
            toggleSong(id)
            return
        }
        if (shift && songSelectionAnchor >= 0) {
            selectSongRange(songSelectionAnchor, index)
            return
        }
        // Plain left click plays the song with this playlist as the
        // navigation context; it re-anchors Shift+click ranges.
        songSelectionAnchor = index
        var song = playlistSongs[index]
        if (song) {
            Player.setQueue(playlistSongs)
            Player.loadSong(song)
            Player.play()
        }
    }

    // Right click replaces the selection with this song (or drops it if it
    // was already selected); it never starts playback.
    function onPlaylistRowRightClicked(index, id) {
        if (selectedSongIds.has(id)) {
            selectedSongIds.delete(id)
            selectedSongCount = selectedSongIds.size
            if (selectedSongCount === 0)
                songSelectionAnchor = -1
            return
        }
        clearSongSelection()
        selectedSongIds.add(id)
        selectedSongCount = 1
        songSelectionAnchor = index
    }

    // Targets for the global shortcuts (main.qml routes Ctrl+D / Ctrl+H /
    // Ctrl+Shift+H here through currentSongsView()). Unlike the Songs
    // section, "delete" means "remove from this playlist".
    function deleteSelected() {
        if (selectedSongCount === 0 || selectedPlaylistId < 0)
            return
        var ids = Array.from(selectedSongIds)
        for (var i = 0; i < ids.length; ++i) {
            Playlists.removeSong(selectedPlaylistId, ids[i])
        }
        clearSongSelection()
    }

    function hideSelected() {
        if (selectedSongCount === 0)
            return
        var ids = Array.from(selectedSongIds)
        for (var i = 0; i < ids.length; ++i) {
            var s = findPlaylistSongById(ids[i])
            if (s && s.path)
                Library.ignorePath(s.path)
        }
        clearSongSelection()
    }

    function hideSelectedFolders() {
        if (selectedSongCount === 0)
            return
        var seen = new Set()
        var ids = Array.from(selectedSongIds)
        for (var i = 0; i < ids.length; ++i) {
            var s = findPlaylistSongById(ids[i])
            if (s && s.path) {
                var folder = songFolderOf(s.path)
                if (folder && !seen.has(folder)) {
                    seen.add(folder)
                    Library.ignorePath(folder)
                }
            }
        }
        clearSongSelection()
    }

    Connections {
        target: Playlists
        function onPlaylistsChanged() {
            var list = Playlists.playlists
            var found = false
            for (var i = 0; i < list.length; ++i) {
                if (list[i].id === playlistsView.selectedPlaylistId) {
                    found = true
                    break
                }
            }
            if (!found)
                playlistsView.selectedPlaylistId = -1
            playlistsView.reloadSongs()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ================= left: playlist list =================
        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: qsTr("Playlists")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 15 * Theme.fontScale
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    TextField {
                        font.family: Theme.fontFamily
                        id: newPlaylistField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: qsTr("New playlist name")
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textPrimary
                        selectionColor: Theme.fieldSelection
                        selectedTextColor: Theme.fieldSelectedText
                        font.pixelSize: 13 * Theme.fontScale
                        leftPadding: 10
                        rightPadding: 10
                        background: Rectangle {
                            radius: 6
                            color: Theme.field
                            border.width: 1
                            border.color: newPlaylistField.activeFocus ? Theme.accent : Theme.border
                        }
                        onAccepted: createButton.clicked()
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: createButton
                        Layout.preferredHeight: 34
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Add")
                        enabled: newPlaylistField.text.trim().length > 0
                        onClicked: {
                            Playlists.createPlaylist(newPlaylistField.text.trim())
                            newPlaylistField.text = ""
                        }
                    }
                }

                ListView {
                    id: playlistList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: Playlists.playlists.length > 0
                    model: Playlists.playlists
                    clip: true
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Rectangle {
                        id: plRow
                        width: playlistList.width
                        height: 48
                        radius: 6

                        // model.<prop> proxies the playlist map (JS array model).
                        property bool isSelected: playlistsView.selectedPlaylistId === model.id
                        property bool renaming: false

                        color: isSelected ? Theme.rowActive
                               : plMouse.containsMouse ? Theme.rowHover : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: 100 }
                        }

                        function startRename() {
                            renaming = true
                            renameField.text = model.name
                            renameField.forceActiveFocus()
                            renameField.selectAll()
                        }

                        MouseArea {
                            id: plMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: playlistsView.selectedPlaylistId = model.id
                            onDoubleClicked: plRow.startRename()
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 6
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 6

                            ColumnLayout {
                                visible: !plRow.renaming
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: plRow.isSelected ? Theme.accent : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 13 * Theme.fontScale
                                    font.bold: plRow.isSelected
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: model.songCount + (model.songCount === 1
                                                              ? qsTr(" song") : qsTr(" songs"))
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11 * Theme.fontScale
                                    elide: Text.ElideRight
                                }
                            }

                            TextField {
                                font.family: Theme.fontFamily
                                id: renameField
                                visible: plRow.renaming
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                font.pixelSize: 13 * Theme.fontScale
                                color: Theme.textPrimary
                                selectionColor: Theme.fieldSelection
                                selectedTextColor: Theme.fieldSelectedText
                                leftPadding: 8
                                rightPadding: 8
                                background: Rectangle {
                                    radius: 4
                                    color: Theme.field
                                    border.width: 1
                                    border.color: Theme.accent
                                }
                                onAccepted: {
                                    var name = text.trim()
                                    if (name.length > 0)
                                        Playlists.renamePlaylist(model.id, name)
                                    plRow.renaming = false
                                }
                                Keys.onEscapePressed: function (event) {
                                    plRow.renaming = false
                                    event.accepted = true
                                }
                            }

                            ToolButton {
                                font.family: Theme.fontFamily
                                id: renameButton
                                visible: !plRow.renaming
                                         && (plMouse.containsMouse || plRow.isSelected)
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: qsTr("Rename")
                                    color: renameButton.hovered ? Theme.textPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11 * Theme.fontScale
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: renameButton.hovered ? Theme.border : "transparent"
                                }
                                onClicked: plRow.startRename()
                            }

                            ToolButton {
                                font.family: Theme.fontFamily
                                id: deleteButton
                                visible: !plRow.renaming
                                         && (plMouse.containsMouse || plRow.isSelected)
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: qsTr("Delete")
                                    color: deleteButton.hovered ? Theme.danger : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11 * Theme.fontScale
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: deleteButton.hovered ? Theme.dangerBg : "transparent"
                                }
                                onClicked: {
                                    if (plRow.isSelected)
                                        playlistsView.selectedPlaylistId = -1
                                    Playlists.deletePlaylist(model.id)
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: Playlists.playlists.length === 0
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("No playlists yet — create one above")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 13 * Theme.fontScale
                }
            }
        }

        // ================= right: playlist songs =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: Theme.bgAlt
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    visible: playlistsView.selectedPlaylistId >= 0
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: playlistsView.selectedName
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 15 * Theme.fontScale
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: playlistsView.playlistSongs.length
                              + (playlistsView.playlistSongs.length === 1
                                 ? qsTr(" song") : qsTr(" songs"))
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: playPlaylistButton
                        text: qsTr("Play")
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 12 * Theme.fontScale
                        enabled: playlistsView.playlistSongs.length > 0
                        opacity: enabled ? 1.0 : 0.4
                        contentItem: Text {
                            text: playPlaylistButton.text
                            font: playPlaylistButton.font
                            color: Theme.accentText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: playPlaylistButton.down ? Theme.accentPressed
                                   : playPlaylistButton.hovered ? Theme.accentHover : Theme.accent
                        }
                        onClicked: playlistsView.playPlaylist()
                    }

                    Button {
                        font.family: Theme.fontFamily
                        id: shuffleButton
                        text: qsTr("Shuffle")
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 12 * Theme.fontScale
                        enabled: playlistsView.playlistSongs.length > 0
                        opacity: enabled ? 1.0 : 0.4
                        contentItem: Text {
                            text: shuffleButton.text
                            font: shuffleButton.font
                            color: Theme.accentText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: shuffleButton.down ? Theme.accentAltPressed
                                   : shuffleButton.hovered ? Theme.accentAltHover : Theme.accentAlt
                        }
                        onClicked: playlistsView.playShuffled()
                    }
                }

                Rectangle {
                    visible: playlistsView.selectedPlaylistId >= 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                // ---- selection toolbar (visible when songs are selected) ----
                RowLayout {
                    id: playlistSelectionBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    spacing: 10
                    visible: playlistsView.selectedSongCount > 0

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%n song(s) selected", "", playlistsView.selectedSongCount)
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: 12 * Theme.fontScale
                        elide: Text.ElideRight
                    }

                    ThemedButton {
                        text: qsTr("Remove")
                        onClicked: playlistsView.deleteSelected()
                    }

                    ThemedButton {
                        text: qsTr("Hide")
                        onClicked: playlistsView.hideSelected()
                    }

                    ThemedButton {
                        text: qsTr("Hide folder")
                        onClicked: playlistsView.hideSelectedFolders()
                    }

                    ThemedButton {
                        text: qsTr("Clear selection")
                        onClicked: playlistsView.clearSongSelection()
                    }
                }

                ListView {
                    id: playlistSongsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: playlistsView.selectedPlaylistId >= 0
                    model: playlistsView.playlistSongs
                    clip: true
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds
                    focus: true
                    ScrollBar.vertical: ScrollBar { }

                    Keys.onPressed: function (event) {
                        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                            playlistsView.selectAllSongs()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            playlistsView.clearSongSelection()
                            event.accepted = true
                        }
                    }

                    delegate: Rectangle {
                        id: plSongRow
                        required property int index
                        width: playlistSongsList.width
                        height: 44
                        radius: 6

                        // model.<prop> proxies the song map (JS array model).
                        property bool isCurrent: Player.currentSong !== undefined
                                                 && Player.currentSong !== null
                                                 && Player.currentSong.id !== undefined
                                                 && Player.currentSong.id === model.id
                        property bool isSelected: playlistsView.selectedSongCount > 0
                                                  && playlistsView.selectedSongIds.has(model.id)

                        color: isSelected ? Theme.selection
                               : isCurrent ? Theme.rowActive
                               : plSongMouse.containsMouse ? Theme.rowHover : "transparent"
                        border.color: isSelected ? Theme.selectionBorder : "transparent"
                        border.width: isSelected ? 2 : 0
                        Behavior on color {
                            ColorAnimation { duration: 100 }
                        }

                        MouseArea {
                            id: plSongMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            // Left = play, right = select, Ctrl/Shift+left = multi.
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function (mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    playlistsView.onPlaylistRowRightClicked(plSongRow.index, model.id)
                                    return
                                }
                                playlistsView.onPlaylistRowClicked(plSongRow.index, model.id, mouse.modifiers)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 6
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: model.title ? model.title : qsTr("Unknown title")
                                color: plSongRow.isSelected ? Theme.selectionText : Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: 13 * Theme.fontScale
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 150
                                text: model.artist ? model.artist : ""
                                color: plSongRow.isSelected ? Theme.selectionTextMuted : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 12 * Theme.fontScale
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignRight
                                text: playlistsView.formatDuration(model.duration)
                                color: plSongRow.isSelected ? Theme.selectionTextMuted : Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 12 * Theme.fontScale
                            }
                            ToolButton {
                                font.family: Theme.fontFamily
                                id: removeFromPlaylist
                                visible: plSongMouse.containsMouse && !plSongRow.isSelected
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: "\u2715"
                                    color: removeFromPlaylist.hovered ? Theme.danger : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11 * Theme.fontScale
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: removeFromPlaylist.hovered ? Theme.dangerBg : "transparent"
                                }
                                onClicked: {
                                    Playlists.removeSong(playlistsView.selectedPlaylistId, model.id)
                                }
                            }
                        }
                    }
                }

                // ---- empty states ----
                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: playlistsView.selectedPlaylistId < 0
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("Select a playlist, or create a new one")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 14 * Theme.fontScale
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: playlistsView.selectedPlaylistId >= 0
                             && playlistsView.playlistSongs.length === 0
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("This playlist is empty")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 14 * Theme.fontScale
                }
            }
        }
    }
}
