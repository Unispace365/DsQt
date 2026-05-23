pragma Singleton
import QtQuick

// Two-tier design-token singleton (see Docs/articles/color_system.md).
//
//   Tier 1 — primitives: the raw tonal ramp + brand hue. Values, not meanings.
//   Tier 2 — semantic roles: alias the primitives by purpose; these map onto Qt's palette and
//            are what viewers/controls reference.
//   Glass tokens: the non-colour design tokens palette can't carry.
//
// Everything is writable, so a white-label app re-skins by assigning the primitives (or roles)
// once at startup — e.g. DsTheme.tonal0 = "#102018" — and the palette + glass re-derive.
QtObject {
    id: theme

    // --- Tier 1: primitives (tonal ramp) ---
    property color tonal0:  "#191F25"
    property color tonal10: "#2E3439"
    property color white:   "#FFFFFF"
    property color accent:  "#00ADF7"

    // --- Tier 2: semantic roles (alias primitives) ---
    property color surface:        tonal0     // panels / window / control backgrounds
    property color surfaceVariant: tonal10    // raised surfaces
    property color stroke:         tonal10    // borders / outlines
    // NB: avoid names starting with "on"+Capital (e.g. onSurface) — QML reserves those for signal
    // handlers, so such a property silently fails to resolve. Use surfaceText / accentText.
    property color surfaceText:    white      // text / icons on a surface
    property color accentText:     white      // text / icons on an accent fill
    // Translucent surface (e.g. the glass tint as a single premultiplied colour, if needed).
    readonly property color scrim: Qt.rgba(tonal0.r, tonal0.g, tonal0.b, glassTintOpacity)

    // --- Glass tokens (non-colour; palette can't carry these) ---
    property real glassTintOpacity: 0.8       // surface @ 80%
    property real glassBlur:        0.5
    property int  glassBlurMax:     32
    property real glassRadius:      12
    property real glassBorderWidth: 1
}
