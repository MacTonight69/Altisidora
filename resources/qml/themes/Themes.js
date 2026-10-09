// Theme palettes for Altisidora.
//
// Every theme provides the same base colour roles; Theme.qml derives the
// hover/pressed/active variants from them at runtime with Qt.darker /
// Qt.lighter so each palette only needs its essential tones.
.pragma library

var order = [
    "Mimy",
    "Coco",
    "Maya",
    "Comunism",
    "Good Technic",
    "Vacui",
    "Kawai",
    "The Niggest"
]

var themes = {
    // Orange / blue / white, like the flag of the Netherlands.
    "Mimy": {
        name: "Mimy",
        fontFamily: "Baskervville",
        bg: "#eef0f6",            // window / top bar
        bgAlt: "#ffffff",         // sidebar / panels / cards
        viewBg: "#f7f8fc",        // songs page
        field: "#ffffff",         // text inputs
        border: "#c3c9d6",
        textPrimary: "#1f2740",
        textSecondary: "#4a5470",
        textMuted: "#7d87a3",
        accent: "#ff7900",        // orange
        accentAlt: "#1d54c8",     // blue
        accentText: "#ffffff",
        selection: "#ffd9b3",
        selectionBorder: "#ff7900",
        selectionText: "#a14c00",
        selectionTextMuted: "#b06a20",
        danger: "#e0484d",
        dangerBg: "#fbdcdc",
        fieldSelection: "#ffcf9f",
        fieldSelectedText: "#4a3000",
        success: "#2e9e5b"
    },

    // Soft, warm yellows like a desert at noon.
    "Coco": {
        name: "Coco",
        fontScale: 1.4,
        fontFamily: "Sunny Brunch",
        bg: "#f5ecd7",
        bgAlt: "#efe2c1",
        viewBg: "#faf5e6",
        field: "#fbf6e9",
        border: "#d8c79c",
        textPrimary: "#46391f",
        textSecondary: "#7a6a40",
        textMuted: "#a08b5c",
        accent: "#d9a514",
        accentAlt: "#b3812e",
        accentText: "#fff7e0",
        selection: "#f0d98a",
        selectionBorder: "#d9a514",
        selectionText: "#7a5c00",
        selectionTextMuted: "#93781f",
        danger: "#c95a3d",
        dangerBg: "#f7ddd0",
        fieldSelection: "#f0d98a",
        fieldSelectedText: "#5a4700",
        success: "#7c8a1f"
    },

    // Red and black, like the colours of anarchism.
    "Maya": {
        name: "Maya",
        fontScale: 1.4,
        fontFamily: "Anarchaos",
        bg: "#141414",
        bgAlt: "#0a0a0a",
        viewBg: "#161616",
        field: "#0d0d0d",
        border: "#333333",
        textPrimary: "#e8e8e8",
        textSecondary: "#a8a8a8",
        textMuted: "#6f6f6f",
        accent: "#e02020",
        accentAlt: "#d9d9d9",
        accentText: "#ffffff",
        selection: "#4d1111",
        selectionBorder: "#e02020",
        selectionText: "#ff9a9a",
        selectionTextMuted: "#d58a8a",
        danger: "#ff5c5c",
        dangerBg: "#2a1010",
        fieldSelection: "#5c1414",
        fieldSelectedText: "#ffffff",
        success: "#4fc44f"
    },

    // Red and yellow, like the colours of communism.
    "Comunism": {
        name: "Comunism",
        fontFamily: "Konstruktor",
        // Red-dominant flag colours: a dark red base with golden-yellow
        // ornaments (inverted from the original yellow/red look).
        bg: "#591214",
        bgAlt: "#470d0f",
        viewBg: "#611719",
        field: "#4c1012",
        border: "#8a2d2f",
        textPrimary: "#fdeec9",
        textSecondary: "#eccf95",
        textMuted: "#c3a260",
        accent: "#f2c14e",
        accentAlt: "#ffd87a",
        accentText: "#571010",
        selection: "#7e1f1f",
        selectionBorder: "#f2c14e",
        selectionText: "#ffe9ae",
        selectionTextMuted: "#e0c57f",
        danger: "#ff8585",
        dangerBg: "#7a1f1f",
        fieldSelection: "#8a2626",
        fieldSelectedText: "#fff3cf",
        success: "#7fe08d"
    },

    // Blue and black, a calm technical look.
    "Good Technic": {
        name: "Good Technic",
        fontFamily: "Lower Pixel",
        bg: "#0d1117",
        bgAlt: "#060a10",
        viewBg: "#0f151c",
        field: "#0a0f15",
        border: "#222b36",
        textPrimary: "#d7e0ea",
        textSecondary: "#9fb0c0",
        textMuted: "#5f7184",
        accent: "#1f6feb",
        accentAlt: "#58a6ff",
        accentText: "#ffffff",
        selection: "#14304f",
        selectionBorder: "#1f6feb",
        selectionText: "#a8ccff",
        selectionTextMuted: "#77a8e0",
        danger: "#f85149",
        dangerBg: "#2a1416",
        fieldSelection: "#193d66",
        fieldSelectedText: "#e6f1ff",
        success: "#3fb950"
    },

    // Pure white with fine gray borders.
    "Vacui": {
        name: "Vacui",
        fontScale: 1.4,
        fontFamily: "A Charming Font Leftleaning",
        bg: "#fbfbfb",
        bgAlt: "#f4f4f4",
        viewBg: "#fdfdfd",
        field: "#ffffff",
        border: "#d5d5d5",
        textPrimary: "#202020",
        textSecondary: "#555555",
        textMuted: "#999999",
        accent: "#4a4a4a",
        accentAlt: "#6e6e6e",
        accentText: "#ffffff",
        selection: "#e8e8e8",
        selectionBorder: "#8f8f8f",
        selectionText: "#333333",
        selectionTextMuted: "#666666",
        danger: "#c0392b",
        dangerBg: "#f3ddda",
        fieldSelection: "#d9d9d9",
        fieldSelectedText: "#111111",
        success: "#4a7a4a"
    },

    // Soft pastel pink tones.
    "Kawai": {
        name: "Kawai",
        fontFamily: "Handmade Alphabet",
        bg: "#fbeef3",
        bgAlt: "#f6dce7",
        viewBg: "#fdf5f8",
        field: "#fef8fa",
        border: "#e3c6d2",
        textPrimary: "#523145",
        textSecondary: "#8a5f75",
        textMuted: "#b392a5",
        accent: "#f08cb0",
        accentAlt: "#d96f96",
        accentText: "#ffffff",
        selection: "#f7c7d7",
        selectionBorder: "#f08cb0",
        selectionText: "#8a2f52",
        selectionTextMuted: "#b05a80",
        danger: "#e05f8a",
        dangerBg: "#f9d9e2",
        fieldSelection: "#f7c7d7",
        fieldSelectedText: "#5a1f38",
        success: "#7cbf8a"
    },

    // Completely black.
    "The Niggest": {
        name: "The Niggest",
        fontScale: 1.4,
        fontFamily: "Elaesy",
        bg: "#000000",
        bgAlt: "#050505",
        viewBg: "#000000",
        field: "#0a0a0a",
        border: "#1c1c1c",
        textPrimary: "#a8a8a8",
        textSecondary: "#666666",
        textMuted: "#404040",
        accent: "#7a7a7a",
        accentAlt: "#8f8f8f",
        accentText: "#000000",
        selection: "#181818",
        selectionBorder: "#7c7c7c",
        selectionText: "#e0e0e0",
        selectionTextMuted: "#a0a0a0",
        danger: "#b04848",
        dangerBg: "#180d0d",
        fieldSelection: "#2b2b2b",
        fieldSelectedText: "#dddddd",
        success: "#6faf6f"
    }
}