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
            color: "#f3efe7"
            border.width: 1
            border.color: "#d9c9b3"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: qsTr("Playlists")
                    color: "#4f3f2f"
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
                        color: "#4f3f2f"
                        selectionColor: "#45475a"
                        selectedTextColor: "#11111b"
                        font.pixelSize: 13
                        leftPadding: 10
                        rightPadding: 10
                        background: Rectangle {
                            radius: 6
                            color: "#f8f6f1"
                            border.width: 1
                            border.color: newPlaylistField.activeFocus ? "#b9a06c" : "#d9c9b3"
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

                        color: isSelected ? "#e7dfd0"
                               : plMouse.containsMouse ? "#e7dfd0" : "transparent"
                        
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
                                    color: plRow.isSelected ? "#b9a06c" : "#4f3f2f"
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
                                color: "#4f3f2f"
                                selectionColor: "#45475a"
                                selectedTextColor: "#11111b"
                                leftPadding: 8
                                rightPadding: 8
                                background: Rectangle {
                                    radius: 4
                                    color: "#f8f6f1"
                                    border.width: 1
                                    border.color: "#b9a06c"
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
                                    color: renameButton.hovered ? "#4f3f2f" : "#7a6a55"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: renameButton.hovered ? "#d9c9b3" : "transparent"
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
                                    color: deleteButton.hovered ? "#f38ba8" : "#7a6a55"
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
            color: "#f3efe7"
            border.width: 1
            border.color: "#d9c9b3"

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
                        color: "#4f3f2f"
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
                }

                Rectangle {
                    visible: playlistsView.selectedPlaylistId >= 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#d9c9b3"
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

                        color: isCurrent ? "#e7dfd0"
                               : plSongMouse.containsMouse ? "#e7dfd0" : "transparent"
                        
                        }

                        MouseArea {
                            id: plSongMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: {
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
                                color: "#4f3f2f"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.artist ? modelData.artist : ""
                                color: "#7a6a55"
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
                                    color: removeFromPlaylist.hovered ? "#f38ba8" : "#7a6a55"
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
