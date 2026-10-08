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
    title: qsTr("Altisidora")
    color: Theme.bg

    palette.window: Theme.bg
    palette.windowText: Theme.textPrimary
    palette.base: Theme.bgAlt
    palette.alternateBase: Theme.rowActive
    palette.text: Theme.textPrimary
    palette.placeholderText: Theme.textMuted
    palette.button: Theme.border
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.toolTipBase: Theme.field
    palette.toolTipText: Theme.textPrimary

    Component.onCompleted: root.applyTheme(AppSettings.theme)

    function applyTheme(name) {
        Theme.apply(name)
        if (name !== AppSettings.theme)
            AppSettings.theme = name
    }

    // ---- state ----
    property int currentNav: 0
    property string searchQuery: ""
    property var filteredSongs: null // null => show the whole library
    property bool scanning: false

    readonly property var navLabels: [qsTr("Songs"), qsTr("Albums"), qsTr("Artists"),
                                      qsTr("Playlists"), qsTr("Settings")]

    // Alt+1..Alt+5 jump between sections (leave the window-scope to the
    // Shortcut items below so they work regardless of the focused widget).
    Shortcut { sequence: "Alt+1"; enabled: root.visible; onActivated: root.selectNav(0) }
    Shortcut { sequence: "Alt+2"; enabled: root.visible; onActivated: root.selectNav(1) }
    Shortcut { sequence: "Alt+3"; enabled: root.visible; onActivated: root.selectNav(2) }
    Shortcut { sequence: "Alt+4"; enabled: root.visible; onActivated: root.selectNav(3) }
    Shortcut { sequence: "Alt+5"; enabled: root.visible; onActivated: root.selectNav(4) }

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
                color: Theme.bgAlt

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: Theme.border
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 4

                    // Brand logo: large and centered, covering most of the sidebar.
                    Image {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 168
                        Layout.topMargin: 4
                        // Theme-specific brand marks take priority; otherwise pick the
                        // light/dark variant so the logo stays readable.
                        source: Theme.themeName === "Anarchy"
                                ? "qrc:/images/LogoAppRed.png"
                                : Theme.themeName === "Comunism"
                                  ? "qrc:/images/LogoAppYellow.png"
                                  : Theme.isLight(Theme.bg)
                                    ? "qrc:/images/LogoAppDark.png"
                                    : "qrc:/images/LogoAppLight.png"
                        sourceSize.width: 512
                        sourceSize.height: 512
                        fillMode: Image.PreserveAspectFit
                        smooth: true
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
                            focusPolicy: Qt.NoFocus

                            color: root.currentNav === navIndex ? Theme.rowActive
                                   : navMouse.containsMouse ? Theme.rowHover : "transparent"
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
                                color: Theme.accent
                                visible: root.currentNav === navRow.navIndex
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                // Show the "Alt+N" number hint next to the label.
                                text: (navRow.navIndex + 1) + "\u2002\u2002" + navRow.label
                                color: root.currentNav === navRow.navIndex ? Theme.accent : Theme.textPrimary
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
                        color: Theme.border
                    }

                    Item {
                        Layout.preferredHeight: 6
                    }

                    Button {
                        id: scanButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Scan library")
                        font.pixelSize: 13
                        font.bold: true
                        onClicked: folderDialog.open()

                        contentItem: Text {
                            text: scanButton.text
                            font: scanButton.font
                            color: Theme.accentText
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: scanButton.down ? Theme.accentPressed
                                   : scanButton.hovered ? Theme.accentHover : Theme.accent
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
                    color: Theme.bg

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        Text {
                            text: root.navLabels[root.currentNav]
                            color: Theme.textPrimary
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
        color: Theme.scrim

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
                color: Theme.textPrimary
                font.pixelSize: 15
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("This may take a moment")
                color: Theme.textMuted
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
