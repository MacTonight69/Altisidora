pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../js/Utils.js" as Utils

// Shared tag-editing dialog.
//
// One component covers the three edit flows:
//   - "single": edit one song's title / artist / album / year, and pick or
//     remove its artwork. Save -> Library.updateSong(); a freshly picked
//     image then goes through Library.setArtwork() so it lands in the
//     content-hash artwork cache exactly like scan-extracted covers.
//   - "album" / "artist": reassign exactly ONE field on every song id in
//     batchIds. Save -> Library.updateSongs() with minimal {id, <field>}
//     maps: absent keys keep their stored values, which is what
//     structurally prevents a batch edit from equalizing any other column.
//
// The dialog owns the save pipeline and stays open on failure, so there
// are no standardButtons (the stock Ok auto-closes on accept). Callers
// open it by setting mode (+ songMap or batchIds) and calling open();
// refreshing the active search after a successful write is already wired
// in main.qml via libraryChanged, so this dialog never touches search.
Dialog {
    id: editSongDialog

    property string mode: "single"   // "single" | "album" | "artist"
    property var songMap: ({})       // single mode: the full song map
    property var batchIds: []        // batch modes: the selected song ids
    readonly property int targetCount: mode === "single" ? 1 : batchIds.length

    // Transient state, reset on every open (onOpened below).
    property string errorText: ""
    property string pickedArtwork: ""   // local path of a newly picked image
    property bool removeArtwork: false

    signal editApplied()

    // The stored artwork coalesced to a real string: the default songMap is
    // an empty map (still truthy in QML JS), and a missing/null value must
    // read as "" or the preview source would become "file://undefined".
    readonly property string currentArtwork:
        songMap && songMap.artwork ? String(songMap.artwork) : ""

    // What the preview shows: a fresh pick wins over the stored artwork;
    // "Remove artwork" suppresses both so the preview clears immediately.
    readonly property string effectiveArtwork:
        removeArtwork ? ""
        : pickedArtwork !== "" ? pickedArtwork : currentArtwork

    // Single mode requires a non-empty title. Batch mode always saves: an
    // empty value is the documented "clear it for all" intent.
    readonly property bool saveEnabled:
        mode !== "single" || titleField.text.trim().length > 0

    modal: true
    title: mode === "single" ? qsTr("Edit song")
           : mode === "album" ? qsTr("Set album") : qsTr("Set artist")

    function apply() {
        if (!saveEnabled)
            return
        errorText = ""
        if (mode === "single") {
            var map = {
                id: songMap.id,
                title: titleField.text.trim(),
                artist: artistField.text.trim(),
                album: albumField.text.trim(),
                // Year is sent as an int or null, never a string: the
                // backend silently zeroes anything it cannot parse.
                year: yearField.text.trim() === "" ? null : parseInt(yearField.text)
            }
            // Removing the artwork is a plain field clear (an explicit
            // empty string), so it rides the same updateSong emission.
            if (removeArtwork)
                map.artwork = ""
            if (!Library.updateSong(map)) {
                errorText = qsTr("Could not save the changes. Try again.")
                return
            }
            // A new image goes through setArtwork so it is validated and
            // cached like scan-extracted art, never referenced in place.
            if (pickedArtwork !== ""
                    && !Library.setArtwork(songMap.id, pickedArtwork)) {
                errorText = qsTr("Could not save the changes. Try again.")
                return
            }
            editApplied()
            close()
            return
        }
        // Batch: minimal maps with only the reassigned field. Absent keys
        // keep their stored values on the backend side, so titles, years
        // and per-song data are structurally untouched.
        var key = mode === "album" ? "album" : "artist"
        var value = batchField.text.trim()
        var maps = []
        for (var i = 0; i < batchIds.length; ++i) {
            var entry = { id: batchIds[i] }
            entry[key] = value
            maps.push(entry)
        }
        if (!Library.updateSongs(maps)) {
            errorText = qsTr("Could not save the changes. Try again.")
            return
        }
        editApplied()
        close()
    }

    onOpened: {
        // Reset the transient state, then pre-fill. User typing assigns
        // the text properties directly, so initial values must be set
        // here (a text binding would be broken by the first edit and go
        // stale on reopen).
        errorText = ""
        pickedArtwork = ""
        removeArtwork = false
        if (mode === "single") {
            titleField.text = songMap && songMap.title !== undefined ? songMap.title : ""
            artistField.text = songMap && songMap.artist !== undefined ? songMap.artist : ""
            albumField.text = songMap && songMap.album !== undefined ? songMap.album : ""
            yearField.text = songMap && songMap.year > 0 ? String(songMap.year) : ""
            titleField.forceActiveFocus()
        } else {
            // A batch action is a reassignment: the one field starts empty.
            batchField.text = ""
            batchField.forceActiveFocus()
        }
    }

    contentItem: ColumnLayout {
        spacing: 10
        // Constant width, exactly like the other dialogs: a contentItem
        // that references its own implicitWidth is the binding-loop trap.
        implicitWidth: 420

        // ---- batch summary + semantics ----
        Text {
            visible: editSongDialog.mode !== "single"
            Layout.fillWidth: true
            text: qsTr("Applies to %n song(s)", "", editSongDialog.targetCount)
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: 13 * Theme.fontScale
        }
        Text {
            visible: editSongDialog.mode !== "single"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Leave empty to clear it for all selected songs")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: 11 * Theme.fontScale
        }

        // ---- single mode: full tag form ----
        ColumnLayout {
            visible: editSongDialog.mode === "single"
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Title:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }
            TextField {
                id: titleField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: qsTr("Song title")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: editSongDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: titleField.activeFocus ? Theme.accent : Theme.border
                }
            }

            Text {
                text: qsTr("Artist:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }
            TextField {
                id: artistField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: qsTr("Artist")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: editSongDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: artistField.activeFocus ? Theme.accent : Theme.border
                }
            }

            Text {
                text: qsTr("Album:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }
            TextField {
                id: albumField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: qsTr("Album")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: editSongDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: albumField.activeFocus ? Theme.accent : Theme.border
                }
            }

            Text {
                text: qsTr("Year:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }
            TextField {
                id: yearField
                Layout.preferredWidth: 120
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: qsTr("Year")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                validator: IntValidator { bottom: 0; top: 9999 }
                inputMethodHints: Qt.ImhDigitsOnly
                onAccepted: editSongDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: yearField.activeFocus ? Theme.accent : Theme.border
                }
            }

            // ---- artwork picker ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Image {
                    id: artworkPreview
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    visible: status === Image.Ready
                    source: editSongDialog.effectiveArtwork !== ""
                            ? "file://" + editSongDialog.effectiveArtwork : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    sourceSize.width: 64
                    sourceSize.height: 64
                }

                Button {
                    font.family: Theme.fontFamily
                    id: chooseArtworkButton
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Choose…")
                    onClicked: artworkFileDialog.open()
                }

                Button {
                    font.family: Theme.fontFamily
                    id: removeArtworkButton
                    focusPolicy: Qt.NoFocus
                    visible: editSongDialog.currentArtwork !== ""
                             || editSongDialog.pickedArtwork !== ""
                    text: qsTr("Remove artwork")
                    onClicked: {
                        editSongDialog.removeArtwork = true
                        editSongDialog.pickedArtwork = ""
                    }
                }
            }
        }

        // ---- batch modes: exactly one reassigned field ----
        ColumnLayout {
            visible: editSongDialog.mode !== "single"
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: editSongDialog.mode === "album"
                       ? qsTr("Album:") : qsTr("Artist:")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
            }

            TextField {
                id: batchField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                font.family: Theme.fontFamily
                font.pixelSize: 13 * Theme.fontScale
                color: Theme.textPrimary
                placeholderText: editSongDialog.mode === "album"
                                 ? qsTr("Album") : qsTr("Artist")
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.fieldSelection
                selectedTextColor: Theme.fieldSelectedText
                leftPadding: 10
                rightPadding: 10
                onAccepted: editSongDialog.apply()
                background: Rectangle {
                    radius: 6
                    color: Theme.field
                    border.width: 1
                    border.color: batchField.activeFocus ? Theme.accent : Theme.border
                }
            }
        }

        // ---- inline error (the dialog stays open on failure) ----
        Text {
            Layout.fillWidth: true
            visible: editSongDialog.errorText !== ""
            wrapMode: Text.Wrap
            text: editSongDialog.errorText
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: 12 * Theme.fontScale
        }

        // ---- footer: Cancel / Save (no standardButtons) ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                font.family: Theme.fontFamily
                id: cancelButton
                focusPolicy: Qt.NoFocus
                text: qsTr("Cancel")
                onClicked: editSongDialog.close()
            }

            Button {
                font.family: Theme.fontFamily
                id: saveButton
                focusPolicy: Qt.NoFocus
                text: qsTr("Save")
                enabled: editSongDialog.saveEnabled
                onClicked: editSongDialog.apply()
            }
        }
    }

    // ---- artwork file picker (built-in QML dialog; native dialogs are
    //      disabled app-wide in main.cpp) ----
    FileDialog {
        id: artworkFileDialog
        title: qsTr("Choose artwork")
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            editSongDialog.pickedArtwork = Utils.urlToPath(selectedFile)
            editSongDialog.removeArtwork = false
        }
    }
}
