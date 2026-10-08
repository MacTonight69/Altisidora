pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "views"
import "components"

ApplicationWindow {
    id: root

    width: 1200
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: qsTr("MusicRep")
    color: "#1e1e2e"

    palette.window: "#1e1e2e"
    palette.windowText: "#cdd6f4"
    palette.base: "#181825"
    palette.alternateBase: "#232435"
    palette.text: "#cdd6f4"
    palette.placeholderText: "#6c7086"
    palette.button: "#313244"
    palette.buttonText: "#cdd6f4"
    palette.highlight: "#89b4fa"
    palette.highlightedText: "#1e1e2e"
    palette.toolTipBase: "#313244"
    palette.toolTipText: "#cdd6f4"

    // ---- state ----
    property int currentNav: 0
    property string searchQuery: ""
    property var filteredSongs: null // null => show the whole library
    property bool scanning: false

    readonly property var navLabels: [qsTr("Songs"), qsTr("Albums"), qsTr("Artists"),
                                      qsTr("Playlists"), qsTr("Settings")]

    function selectNav(index) {
        currentNav = index
        if (index === 0)
            contentStack.replace(songsComponent)
        else if (index === 1)
            contentStack.replace(albumsComponent)
        else if (index === 2)
            contentStack.replace(artistsComponent)
        else if (index === 3)
            contentStack.replace(playlistsComponent)
        else
            contentStack.replace(settingsComponent)
    }

    // QUrl -> plain local path ("/home/user/Music")
    function folderToPath(url) {
        var s = url.toString()
        if (s.indexOf("file://") === 0)
            s = s.substring(7)
        return decodeURIComponent(s)
    }

    property string pendingScanPath: ""

    // Scan on the next event-loop turn so the overlay gets a chance to
    // paint before Library.scan() blocks the UI thread.
    Timer {
        id: scanTimer
        interval: 100
        onTriggered: Library.scan(root.pendingScanPath)
    }

    // ---- folder picker for "Scan library" ----
    FolderDialog {
        id: folderDialog
        title: qsTr("Select music folder")
        onAccepted: {
            var path = root.folderToPath(selectedFolder)
            AppSettings.musicDirectory = path
            AppSettings.save()
            root.pendingScanPath = path
            root.scanning = true
            scanTimer.restart()
        }
    }

    Connections {
        target: Library
        function onScanStarted() {
            root.scanning = true
        }
        function onScanFinished() {
            root.scanning = false
        }
        function onLibraryChanged() {
            if (root.searchQuery.length > 0)
                root.filteredSongs = Library.search(root.searchQuery)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ================= sidebar =================
            Rectangle {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                color: "#181825"

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: "#313244"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 4

                    RowLayout {
                        spacing: 8

                        Text {
                            text: "\u266A"
                            color: "#89b4fa"
                            font.pixelSize: 22
                        }
                        Text {
                            text: "MusicRep"
                            color: "#cdd6f4"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }

                    Item {
                        Layout.preferredHeight: 18
                    }

                    Repeater {
                        model: root.navLabels

                        delegate: Rectangle {
                            id: navRow
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 6

                            required property int index
                            required property var modelData
                            property int navIndex: index
                            property string label: modelData

                            color: root.currentNav === navIndex ? "#252637"
                                   : navMouse.containsMouse ? "#20212f" : "transparent"
                            Behavior on color {
                                ColorAnimation { duration: 120 }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: parent.height - 16
                                radius: 2
                                color: "#89b4fa"
                                visible: root.currentNav === navRow.navIndex
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                text: navRow.label
                                color: root.currentNav === navRow.navIndex ? "#89b4fa" : "#cdd6f4"
                                font.pixelSize: 14
                                font.bold: root.currentNav === navRow.navIndex
                            }

                            MouseArea {
                                id: navMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectNav(navRow.navIndex)
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#313244"
                    }

                    Item {
                        Layout.preferredHeight: 6
                    }

                    Button {
                        id: scanButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: qsTr("Scan library")
                        font.pixelSize: 13
                        font.bold: true
                        onClicked: folderDialog.open()

                        contentItem: Text {
                            text: scanButton.text
                            font: scanButton.font
                            color: "#1e1e2e"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: scanButton.down ? "#74a8f7"
                                   : scanButton.hovered ? "#9cc7fb" : "#89b4fa"
                            Behavior on color {
                                ColorAnimation { duration: 120 }
                            }
                        }
                    }
                }
            }

            // ================= content =================
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // top bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: "#1e1e2e"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#313244"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        Text {
                            text: root.navLabels[root.currentNav]
                            color: "#cdd6f4"
                            font.pixelSize: 17
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SearchBar {
                            id: searchBar
                            Layout.preferredWidth: 320
                            Layout.preferredHeight: 34

                            onQueryChanged: function (query) {
                                root.searchQuery = query
                                root.filteredSongs = query.length > 0
                                                     ? Library.search(query) : null
                                if (query.length > 0 && root.currentNav !== 0)
                                    root.selectNav(0)
                            }
                        }
                    }
                }

                StackView {
                    id: contentStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    initialItem: songsComponent

                    // Direct, animation-free view switching: the default
                    // StackView transitions would slide/fade between views
                    // with the old and new ones overlapping.
                    pushEnter: null
                    pushExit: null
                    popEnter: null
                    popExit: null
                    replaceEnter: null
                    replaceExit: null
                }
            }
        }

        PlayerControls {
            id: playerControls
            Layout.fillWidth: true
            Layout.preferredHeight: 80
        }
    }

    // ================= scanning overlay =================
    Rectangle {
        id: scanOverlay
        z: 100
        anchors.fill: parent
        visible: root.scanning
        color: "#d91e1e2e"

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: scanOverlay.visible
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Scanning library…")
                color: "#cdd6f4"
                font.pixelSize: 15
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("This may take a moment")
                color: "#6c7086"
                font.pixelSize: 12
            }
        }
    }

    // ================= view components =================
    Component {
        id: songsComponent
        SongsView {
            songs: root.filteredSongs !== null ? root.filteredSongs : Library.songs
        }
    }

    Component {
        id: albumsComponent
        AlbumsView { }
    }

    Component {
        id: artistsComponent
        ArtistsView { }
    }

    Component {
        id: playlistsComponent
        PlaylistsView { }
    }

    Component {
        id: settingsComponent
        SettingsView { }
    }
}
