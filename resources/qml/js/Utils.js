// Shared pure helpers for the QML views.
//
// This is a .pragma library module, so it has no access to QML scope: every
// function below is pure and only depends on its arguments (plus the standard
// JavaScript built-ins). Import it per document, e.g.
//
//     import "../js/Utils.js" as Utils
//
.pragma library

// Fisher-Yates shuffle. Works on a copy, so the caller's array (and any
// selection state tied to it) is never reordered. Returns the new array.
function shuffle(list) {
    var copy = list.slice()
    for (var i = copy.length - 1; i > 0; --i) {
        var j = Math.floor(Math.random() * (i + 1))
        var tmp = copy[i]
        copy[i] = copy[j]
        copy[j] = tmp
    }
    return copy
}

// Milliseconds -> "m:ss". Negative / non-finite input clamps to 0.
function formatDuration(ms) {
    var total = Math.floor(Number(ms || 0) / 1000)
    if (!isFinite(total) || total < 0)
        total = 0
    var minutes = Math.floor(total / 60)
    var seconds = total % 60
    return minutes + ":" + (seconds < 10 ? "0" + seconds : "" + seconds)
}

// Drop a leading file:// scheme. No percent-decoding.
function stripFileScheme(path) {
    var s = String(path)
    if (s.indexOf("file://") === 0)
        s = s.substring("file://".length)
    return s
}

// QUrl (or file:// string) -> plain, decoded local path.
function urlToPath(url) {
    return decodeURIComponent(stripFileScheme(url.toString()))
}
