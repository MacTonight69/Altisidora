pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    }

    onSelectedPlaylistIdChanged: reloadSongs()

    function formatDuration(ms) {
        var total = Math.floor(Number(ms || 0) / 1000)
        if (!isFinite(total) || total < 0)
            total = 0
        var m = Math.floor(total / 60)
        var s = total % 60
        return m + ":" + (s < 10 ? "0" + s : "" + s)
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
        var songs = playlistSongs.slice()
        for (var i = songs.length - 1; i > 0; --i) {
            var j = Math.floor(Math.random() * (i + 1))
            var tmp = songs[i]
            songs[i] = songs[j]
            songs[j] = tmp
        }
        Player.setQueue(songs)
        Player.loadSong(songs[0])
        Player.play()
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
                    font.pixelSize: 15
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    TextField {
                        id: newPlaylistField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: qsTr("New playlist name")
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textPrimary
                        selectionColor: Theme.fieldSelection
                        selectedTextColor: Theme.fieldSelectedText
                        font.pixelSize: 13
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
                        id: createButton
                        Layout.preferredHeight: 34
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
                        required property var modelData
                        width: playlistList.width
                        height: 48
                        radius: 6

                        property bool isSelected: playlistsView.selectedPlaylistId === modelData.id
                        property bool renaming: false

                        color: isSelected ? Theme.rowActive
                               : plMouse.containsMouse ? Theme.rowHover : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: 100 }
                        }

                        function startRename() {
                            renaming = true
                            renameField.text = modelData.name
                            renameField.forceActiveFocus()
                            renameField.selectAll()
                        }

                        MouseArea {
                            id: plMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: playlistsView.selectedPlaylistId = modelData.id
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
                                    text: modelData.name
                                    color: plRow.isSelected ? Theme.accent : Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: plRow.isSelected
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.songCount + (modelData.songCount === 1
                                                                  ? qsTr(" song") : qsTr(" songs"))
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            TextField {
                                id: renameField
                                visible: plRow.renaming
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                font.pixelSize: 13
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
                                        Playlists.renamePlaylist(modelData.id, name)
                                    plRow.renaming = false
                                }
                                Keys.onEscapePressed: function (event) {
                                    plRow.renaming = false
                                    event.accepted = true
                                }
                            }

                            ToolButton {
                                id: renameButton
                                visible: !plRow.renaming
                                         && (plMouse.containsMouse || plRow.isSelected)
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: qsTr("Rename")
                                    color: renameButton.hovered ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: 11
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
                                id: deleteButton
                                visible: !plRow.renaming
                                         && (plMouse.containsMouse || plRow.isSelected)
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: qsTr("Delete")
                                    color: deleteButton.hovered ? Theme.danger : Theme.textSecondary
                                    font.pixelSize: 11
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
                                    Playlists.deletePlaylist(modelData.id)
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
                    font.pixelSize: 13
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
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: playlistsView.playlistSongs.length
                              + (playlistsView.playlistSongs.length === 1
                                 ? qsTr(" song") : qsTr(" songs"))
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Button {
                        id: playPlaylistButton
                        text: qsTr("Play")
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 12
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
                        id: shuffleButton
                        text: qsTr("Shuffle")
                        focusPolicy: Qt.NoFocus
                        font.pixelSize: 12
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

                ListView {
                    id: playlistSongsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: playlistsView.selectedPlaylistId >= 0
                    model: playlistsView.playlistSongs
                    clip: true
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Rectangle {
                        id: plSongRow
                        required property var modelData
                        width: playlistSongsList.width
                        height: 44
                        radius: 6

                        property bool isCurrent: Player.currentSong !== undefined
                                                 && Player.currentSong !== null
                                                 && Player.currentSong.id !== undefined
                                                 && Player.currentSong.id === modelData.id

                        color: isCurrent ? Theme.rowActive
                               : plSongMouse.containsMouse ? Theme.rowHover : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: 100 }
                        }

                        MouseArea {
                            id: plSongMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: {
                                // Make the playlist the navigation context for next/previous.
                                Player.setQueue(playlistsView.playlistSongs)
                                Player.loadSong(modelData)
                                Player.play()
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 6
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: modelData.title ? modelData.title : qsTr("Unknown title")
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.artist ? modelData.artist : ""
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignRight
                                text: playlistsView.formatDuration(modelData.duration)
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }
                            ToolButton {
                                id: removeFromPlaylist
                                visible: plSongMouse.containsMouse
                                focusPolicy: Qt.NoFocus
                                hoverEnabled: true
                                padding: 4

                                contentItem: Text {
                                    text: "\u2715"
                                    color: removeFromPlaylist.hovered ? Theme.danger : Theme.textSecondary
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: removeFromPlaylist.hovered ? Theme.dangerBg : "transparent"
                                }
                                onClicked: Playlists.removeSong(playlistsView.selectedPlaylistId,
                                                                modelData.id)
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
                    font.pixelSize: 14
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
                    font.pixelSize: 14
                }
            }
        }
    }
}
