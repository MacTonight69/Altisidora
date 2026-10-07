pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: artistsView
    color: "transparent"

    ListView {
        id: artistList
        anchors.fill: parent
        anchors.margins: 16
        model: Library.artists
        clip: true
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        delegate: Rectangle {
            id: artistRow
            required property var modelData
            width: artistList.width
            height: 52
            radius: 6
            color: artistMouse.containsMouse ? "#e7dfd0" : "transparent"
            
            }

            MouseArea {
                id: artistMouse
                anchors.fill: parent
                hoverEnabled: true
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 14
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 17
                    color: "#2a2b3d"

                    Text {
                        anchors.centerIn: parent
                        text: modelData.name && modelData.name.length > 0
                              ? modelData.name.charAt(0).toUpperCase() : "?"
                        color: "#b9a06c"
                        font.pixelSize: 15
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: modelData.name
                    color: "#4f3f2f"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    horizontalAlignment: Text.AlignRight
                    text: modelData.songCount + (modelData.songCount === 1
                                                  ? qsTr(" song") : qsTr(" songs"))
                    color: "#7a6a55"
                    font.pixelSize: 12
                }
            }
        }
    }

    // ---- empty state ----
    Text {
        anchors.centerIn: parent
        visible: Library.artists.length === 0
        text: qsTr("No artists yet — press Scan library")
        color: "#6c7086"
        font.pixelSize: 15
    }
}
