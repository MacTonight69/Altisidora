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
            color: "#181825"
            border.width: 1
            border.color: "#313244"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: qsTr("Playlists")
                    color: "#cdd6f4"
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
                        placeholderTextColor: "#6c7086"
                        color: "#cdd6f4"
                        selectionColor: "#45475a"
                        selectedTextColor: "#11111b"
                        font.pixelSize: 13
                        leftPadding: 10
                        rightPadding: 10
                        background: Rectangle {
                            radius: 6
                            color: "#1e1e2e"
                            border.width: 1
                            border.color: newPlaylistField.activeFocus ? "#89b4fa" : "#313244"
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

                        color: isSelected ? "#252637"
                               : plMouse.containsMouse ? "#20212f" : "transparent"
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
                                    color: plRow.isSelected ? "#89b4fa" : "#cdd6f4"
                                    font.pixelSize: 13
                                    font.bold: plRow.isSelected
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.songCount + (modelData.songCount === 1
                                                                  ? qsTr(" song") : qsTr(" songs"))
                                    color: "#6c7086"
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
                                color: "#cdd6f4"
                                selectionColor: "#45475a"
                                selectedTextColor: "#11111b"
                                leftPadding: 8
                                rightPadding: 8
                                background: Rectangle {
                                    radius: 4
                                    color: "#1e1e2e"
                                    border.width: 1
                                    border.color: "#89b4fa"
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
                                    color: renameButton.hovered ? "#cdd6f4" : "#a6adc8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: renameButton.hovered ? "#313244" : "transparent"
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
                                    color: deleteButton.hovered ? "#f38ba8" : "#a6adc8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: deleteButton.hovered ? "#3b1e2a" : "transparent"
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
                    color: "#6c7086"
                    font.pixelSize: 13
                }
            }
        }

        // ================= right: playlist songs =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#181825"
            border.width: 1
            border.color: "#313244"

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
                        color: "#cdd6f4"
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: playlistsView.playlistSongs.length
                              + (playlistsView.playlistSongs.length === 1
                                 ? qsTr(" song") : qsTr(" songs"))
                        color: "#6c7086"
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
                            color: "#1e1e2e"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: playPlaylistButton.down ? "#74a8f7"
                                   : playPlaylistButton.hovered ? "#9cc7fb" : "#89b4fa"
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
                            color: "#1e1e2e"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: shuffleButton.down ? "#e3945a"
                                   : shuffleButton.hovered ? "#f7bd92" : "#fab387"
                        }
                        onClicked: playlistsView.playShuffled()
                    }
                }

                Rectangle {
                    visible: playlistsView.selectedPlaylistId >= 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#313244"
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

                        color: isCurrent ? "#252637"
                               : plSongMouse.containsMouse ? "#20212f" : "transparent"
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
                                color: "#cdd6f4"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.artist ? modelData.artist : ""
                                color: "#a6adc8"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignRight
                                text: playlistsView.formatDuration(modelData.duration)
                                color: "#6c7086"
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
                                    color: removeFromPlaylist.hovered ? "#f38ba8" : "#a6adc8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: removeFromPlaylist.hovered ? "#3b1e2a" : "transparent"
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
                    color: "#6c7086"
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
                    color: "#6c7086"
                    font.pixelSize: 14
                }
            }
        }
    }
}
