pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: albumsView
    color: "transparent"

    // The albums to show; defaults to the whole library so the view also
    // works standalone. main.qml passes the search-filtered list.
    property var albums: Library.albums
    signal openAlbum(var album)

    // Stable placeholder colour derived from a hash of the album title.
    function albumColor(title) {
        var s = (title === undefined || title === null) ? "" : String(title)
        var hash = 0
        for (var i = 0; i < s.length; ++i)
            hash = (hash * 31 + s.charCodeAt(i)) >>> 0

        var hue = (hash % 360) / 60
        var sat = 0.45
        var lig = 0.55
        var c = (1 - Math.abs(2 * lig - 1)) * sat
        var x = c * (1 - Math.abs((hue % 2) - 1))
        var m = lig - c / 2
        var r, g, b

        if (hue < 1)      { r = c; g = x; b = 0 }
        else if (hue < 2) { r = x; g = c; b = 0 }
        else if (hue < 3) { r = 0; g = c; b = x }
        else if (hue < 4) { r = 0; g = x; b = c }
        else if (hue < 5) { r = x; g = 0; b = c }
        else              { r = c; g = 0; b = x }

        return Qt.rgba(r + m, g + m, b + m, 1.0)
    }

    GridView {
        id: albumGrid
        anchors.fill: parent
        anchors.margins: 16
        model: albumsView.albums
        cellWidth: 176
        cellHeight: 240
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        delegate: Rectangle {
            required property var modelData
            width: albumGrid.cellWidth
            height: albumGrid.cellHeight
            color: "transparent"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                radius: 6
                color: albumMouse.containsMouse ? Theme.rowHover : Theme.bgAlt
                border.width: 1
                border.color: albumMouse.containsMouse ? Theme.accent : Theme.border

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
                Behavior on border.color {
                    ColorAnimation { duration: 120 }
                }

                MouseArea {
                    id: albumMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // Drill down: show this album's songs as a playlist.
                    onClicked: albumsView.openAlbum(modelData)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width
                        radius: 6
                        color: albumsView.albumColor(modelData.title)

                        Text {
                            anchors.centerIn: parent
                            text: "\u266A"
                            color: "#ffffff"
                            opacity: 0.85
                            font.pixelSize: 36
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.title ? modelData.title : qsTr("Unknown album")
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.artist ? modelData.artist : ""
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.year ? String(modelData.year) : qsTr("Unknown year")
                        color: Theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // ---- empty state ----
    Text {
        anchors.centerIn: parent
        visible: albumsView.albums.length === 0
        text: Library.albums.length === 0
              ? qsTr("No albums yet — press Scan library")
              : qsTr("No albums match your search")
        color: Theme.textMuted
        font.pixelSize: 15
    }
}
