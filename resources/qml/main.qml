pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "views"
import "components"
import "js/Utils.js" as Utils

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

    // Every settings change re-applies the theme so the typography scale
    // assigned at theme-change time stays the source of truth, no matter
    // where the change comes from (in-app combo, config edit, ...).
    Connections {
        target: AppSettings
        function onSettingsChanged() {
            root.applyTheme(AppSettings.theme)
        }
    }

    function applyTheme(name) {
        Theme.apply(name)
        if (name !== AppSettings.theme)
            AppSettings.theme = name
        AppSettings.fontScale = Theme.fontScale
    }

    // Apply the saved theme once at startup. The Connections above only
    // fires on later settings changes, so without this the first frame
    // stayed on the Theme.qml default (Mimy) while Settings showed the
    // saved theme — the reopened-on-Mimy bug.
    Component.onCompleted: root.applyTheme(AppSettings.theme)

    // ---- state ----
    property int currentNav: 0
    property string searchQuery: ""
    property var filteredAlbums: null
    property var filteredArtists: null
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

    // Global song-list shortcuts, same philosophy as Alt+1..5: they live at
    // window scope so they keep working no matter which widget has keyboard
    // focus. (The old Ctrl+D lived in Keys.onPressed on the list, which only
    // fires while the list itself owns focus.)
    //   Ctrl+D       -> delete the selected song(s) from the library
    //   Ctrl+H       -> hide the selected song(s)
    //   Ctrl+Shift+H -> hide the folder(s) of the selected song(s)
    Shortcut {
        sequence: "Ctrl+D"
        enabled: root.visible
        onActivated: {
            var v = root.currentSongsView()
            if (v)
                v.deleteSelected()
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        enabled: root.visible
        onActivated: {
            var v = root.currentSongsView()
            if (v)
                v.hideSelected()
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+H"
        enabled: root.visible
        onActivated: {
            var v = root.currentSongsView()
            if (v)
                v.hideSelectedFolders()
        }
    }

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
        return Utils.urlToPath(url)
    }

    // Per-section search: match album titles (Albums) and artist names
    // (Artists), both case-insensitive substring matches. Songs are filtered
    // by the controller via setSearch(). The current section is never changed
    // by searching.
    function filterAlbums(query) {
        var q = query.toLowerCase()
        var out = []
        var all = Library.albums
        for (var i = 0; i < all.length; ++i) {
            var t = all[i].title !== undefined && all[i].title !== null
                    ? String(all[i].title).toLowerCase() : ""
            if (t.indexOf(q) !== -1)
                out.push(all[i])
        }
        return out
    }

    function filterArtists(query) {
        var q = query.toLowerCase()
        var out = []
        var all = Library.artists
        for (var i = 0; i < all.length; ++i) {
            var n = all[i].name !== undefined && all[i].name !== null
                    ? String(all[i].name).toLowerCase() : ""
            if (n.indexOf(q) !== -1)
                out.push(all[i])
        }
        return out
    }

    function applySearch(query) {
        // The song list filters through the controller's search model; the
        // albums/artists sections keep their JS substring filters.
        Library.setSearch(query)
        if (query.length === 0) {
            root.filteredAlbums = null
            root.filteredArtists = null
            return
        }
        root.filteredAlbums = root.filterAlbums(query)
        root.filteredArtists = root.filterArtists(query)
    }

    // The song list that the global shortcuts should act on: the Songs
    // section shows a SongsView directly on the stack, while the
    // album/artist drill-down nests one inside SongListDetailView.
    function currentSongsView() {
        var item = contentStack.currentItem
        if (!item)
            return null
        // A SongsView root, or a view exposing the nested SongsView.
        if (typeof item.deleteSelected === "function")
            return item
        if (item.songsList)
            return item.songsList
        return null
    }

    // Drill-down: push a view that resolves the clicked collection's songs
    // (album/artist) into a playlist-like list on top of the current
    // section; the back button pops back to it.
    function openAlbumDetail(album) {
        contentStack.push(songDetailComponent, {
            filterType: "album",
            primaryKey: album && album.title ? album.title : "",
            secondaryKey: album && album.artist ? album.artist : ""
        })
    }

    function openArtistDetail(artist) {
        contentStack.push(songDetailComponent, {
            filterType: "artist",
            primaryKey: artist && artist.name ? artist.name : "",
            secondaryKey: ""
        })
    }

    property string pendingScanPath: ""

    // Scan on the next event-loop turn so the overlay gets a chance to
    // paint before Library.scan() blocks the UI thread.
    Timer {
        id: scanTimer
        interval: 100
        onTriggered: Library.scan(root.pendingScanPath)
    }

    // Debounce search: a keystroke only (re)schedules the re-filter, which
    // then runs Library.setSearch() + the album/artist JS filters once the
    // user pauses for 200 ms instead of on every keystroke.
    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: root.applySearch(root.searchQuery)
    }

    // ---- folder picker for "Scan library" ----
    FolderDialog {
        id: folderDialog
        title: qsTr("Select music folder")
        // Open in the user's personal folder so the scan targets are easy
        // to reach when scanning a folder.
        currentFolder: personalMusicFolder
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
        function onScanFinished(removedCount) {
            root.scanning = false
            // The scan drops the songs whose file no longer exists; those
            // rows are unrecoverable, so the removal is announced instead of
            // happening silently. The count travels with the signal.
            if (removedCount > 0) {
                scanNotice.count = removedCount
                scanNoticeTimer.restart()
            }
        }
        function onLibraryChanged() {
            if (root.searchQuery.length > 0)
                root.applySearch(root.searchQuery)
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
                Layout.preferredWidth: Theme.sidebarWidth
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
                        source: (Theme.themeName === "Anarchism"
                                 || Theme.themeName === "Maya")
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
                                font.family: Theme.fontFamily
                                font.pixelSize: 14 * Theme.fontScale
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
                        font.family: Theme.fontFamily
                        id: scanButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        focusPolicy: Qt.NoFocus
                        text: qsTr("Scan library")
                        font.pixelSize: 13 * Theme.fontScale
                        font.bold: true
                        onClicked: {
                            // Re-assert the start folder right before
                            // opening, in case the dialog kept a previous
                            // selection.
                            folderDialog.currentFolder = personalMusicFolder
                            folderDialog.open()
                        }

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
                            font.family: Theme.fontFamily
                            font.pixelSize: 17 * Theme.fontScale
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SearchBar {
                            id: searchBar
                            objectName: "searchBar"
                            Layout.preferredWidth: 320
                            Layout.preferredHeight: 34

                            onQueryChanged: function (query) {
                                root.searchQuery = query
                                searchDebounce.restart()
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
                font.family: Theme.fontFamily
                font.pixelSize: 15 * Theme.fontScale
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("This may take a moment")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 12 * Theme.fontScale
            }
        }
    }

    // ================= scan reconciliation notice =================
    // Bottom-centered, above the player bar. Shown only when a scan dropped
    // songs (scanFinished(removedCount)); hides itself after the timer.
    Rectangle {
        id: scanNotice
        z: 90
        anchors.horizontalCenter: parent.horizontalCenter
        // Above the player bar. The bar lives inside the root ColumnLayout,
        // so it is a descendant, not a sibling: anchoring to its top would
        // fail ("cannot anchor to an item that isn't a parent or sibling").
        // Its height as a bottom margin keeps the notice clear of it.
        anchors.bottom: parent.bottom
        anchors.bottomMargin: playerControls.height + 14
        visible: count > 0
        width: noticeText.implicitWidth + 32
        height: 34
        radius: 6
        color: Theme.bgAlt
        border.width: 1
        border.color: Theme.border

        property int count: 0

        Text {
            id: noticeText
            anchors.centerIn: parent
            text: qsTr("%n song(s) removed: the file no longer exists", "", scanNotice.count)
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 12 * Theme.fontScale
        }
    }

    Timer {
        id: scanNoticeTimer
        interval: 6000
        onTriggered: scanNotice.count = 0
    }

    // ================= view components =================
    Component {
        id: songsComponent
        SongsView {
            // Both the full library and the active search render through a
            // role-based SongListModel; QML only switches which model feeds
            // the list. songsModel is always set here, so the array fallback
            // (used by the album/artist drill-downs) is deliberately left
            // empty: binding `songs: Library.songs` built a QVariantList of
            // every song on each launch, and the list never read it.
            songsModel: Library.searchActive ? Library.searchModel : Library.songsModel
        }
    }

    Component {
        id: albumsComponent
        AlbumsView {
            albums: root.filteredAlbums !== null ? root.filteredAlbums : Library.albums
            onOpenAlbum: function (album) { root.openAlbumDetail(album) }
        }
    }

    Component {
        id: artistsComponent
        ArtistsView {
            artists: root.filteredArtists !== null ? root.filteredArtists : Library.artists
            onOpenArtist: function (artist) { root.openArtistDetail(artist) }
        }
    }

    Component {
        id: songDetailComponent
        SongListDetailView {
            onBackRequested: contentStack.pop()
        }
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
