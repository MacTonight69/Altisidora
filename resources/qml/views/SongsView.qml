pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: songsView

    property var songs: []
    property int targetSongId: -1
    color: "transparent"

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
                color: "#6c7086"
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Artist")
                color: "#6c7086"
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 160
                text: qsTr("Album")
                color: "#6c7086"
                font.pixelSize: 11
            }
            Text {
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
                text: qsTr("Duration")
                color: "#6c7086"
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#d9c9b3"
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
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                id: songRow
                width: songList.width
                height: 40
                radius: 6

                required property var modelData
                property var song: modelData
                property bool isCurrent: Player.currentSong !== undefined
                                         && Player.currentSong !== null
                                         && Player.currentSong.id !== undefined
                                         && Player.currentSong.id === song.id

                color: isCurrent ? "#e7dfd0"
                       : songMouse.containsMouse ? "#e7dfd0" : "transparent"
                
                }

                MouseArea {
                    id: songMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: {
                        Player.loadSong(songRow.song)
                        Player.play()
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
                        color: "#4f3f2f"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: songRow.song.artist ? songRow.song.artist : ""
                        color: "#7a6a55"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 160
                        text: songRow.song.album ? songRow.song.album : ""
                        color: "#7a6a55"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                        text: songsView.formatDuration(songRow.song.duration)
                        color: "#6c7086"
                        font.pixelSize: 12
                    }

                    ToolButton {
                        id: addToButton
                        visible: songMouse.containsMouse
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: true
                        padding: 4

                        contentItem: Text {
                            text: qsTr("+ Playlist")
                            color: addToButton.hovered ? "#b9a06c" : "#7a6a55"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: addToButton.hovered ? "#d9c9b3" : "transparent"
                        }
                        onClicked: {
                            songsView.targetSongId = songRow.song.id
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
        text: Library.songs.length === 0 ? qsTr("No songs yet — press Scan library")
                                         : qsTr("No songs match your search")
        color: "#6c7086"
        font.pixelSize: 15
    }

    // ---- add to playlist menu ----
    Menu {
        id: addToMenu
        title: qsTr("Add to playlist")

        Instantiator {
            model: Playlists.playlists
            delegate: MenuItem {
                required property var modelData
                text: modelData.name
                onTriggered: Playlists.addSong(modelData.id, songsView.targetSongId)
            }
            onObjectAdded: (index, object) => addToMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => addToMenu.removeItem(object)
        }

        MenuItem {
            text: qsTr("New playlist…")
            onTriggered: {
                var name = qsTr("Playlist %1").arg(Playlists.playlists.length + 1)
                Playlists.createPlaylist(name)
                var list = Playlists.playlists
                for (var i = 0; i < list.length; ++i) {
                    if (list[i].name === name) {
                        Playlists.addSong(list[i].id, songsView.targetSongId)
                        break
                    }
                }
            }
        }
    }
}
