pragma Singleton
import QtQuick
import Dsqt.Core

// Two-tier design-token singleton (see Docs/articles/color_system.md).
//
//   Tier 1 — primitives: the raw tonal ramp + brand hue. Values, not meanings.
//   Tier 2 — semantic roles: alias the primitives by purpose; these map onto Qt's palette and
//            are what viewers/controls reference.
//   Glass tokens: the non-colour design tokens palette can't carry.
//
// Tier-1 primitives and the glass tokens are seeded from settings ([waffles.theme] in
// waffles_settings.toml), falling back to the defaults below. They remain writable, so a
// white-label app can also re-skin at runtime — e.g. DsTheme.tonal0 = "#102018" — and the roles,
// palette and glass re-derive.
QtObject {
    id: theme

    // Reads [waffles.theme] from the merged app_settings collection.
    property DsSettingsProxy _s: DsSettingsProxy { target: "app_settings"; prefix: "waffles.theme" }

    // Waffles bundles Roboto (SIL OFL 1.1) so any consumer app gets `font.family: "Roboto"` for
    // free — no per-app font setup required. Loaded once via a FontLoader on the singleton, the
    // file lives in this module's QRC (see Waffles CMakeLists RESOURCES). Apps that bundle their
    // own Roboto can do so; QFontDatabase will keep the first registration as "Roboto".
    property FontLoader _robotoLoader: FontLoader {
        source: "qrc:/qt/qml/Dsqt/Waffles/data/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf"
    }

    // --- Tier 1: primitives (tonal ramp + brand hue) ---
    property color tonal0:  _s.getColor("tonal0",  "#191F25")
    property color tonal10: _s.getColor("tonal10", "#2E3439")
    property color tonal30: _s.getColor("tonal30", "#5D6166")
    property color white:   _s.getColor("white",   "#FFFFFF")
    property color accent:  _s.getColor("accent",  "#00ADF7")
    // Light brand tint — used for selected-row highlight on the content launcher (Figma's
    // "Primary 50"). Becomes the `selectedSurface` role below.
    property color primary50: _s.getColor("primary50", "#B1DAFC")

    // --- Tier 2: semantic roles (alias primitives) ---
    property color surface:        tonal0     // panels / window / control backgrounds
    property color surfaceVariant: tonal10    // raised surfaces
    property color stroke:         tonal10    // borders / outlines
    // NB: avoid names starting with "on"+Capital (e.g. onSurface) — QML reserves those for signal
    // handlers, so such a property silently fails to resolve. Use surfaceText / accentText.
    property color surfaceText:    white      // text / icons on a surface
    property color accentText:     white      // text / icons on an accent fill
    property color track:          tonal30    // slider / scrubber tracks
    property color selectedSurface:     primary50  // selected-row background (content launcher)
    property color selectedSurfaceText: tonal0     // text / icons on a selected-row surface
    // Translucent surface (e.g. the glass tint as a single premultiplied colour, if needed).
    readonly property color scrim: Qt.rgba(tonal0.r, tonal0.g, tonal0.b, glassTintOpacity)

    // --- Glass tokens (non-colour; palette can't carry these) ---
    property real glassTintOpacity: _s.getFloat("glassTintOpacity", 0.8)   // surface @ 80%
    property real glassBlur:        _s.getFloat("glassBlur", 0.5)
    property int  glassBlurMax:     _s.getInt("glassBlurMax", 32)
    // MultiEffect blur multiplier — extends the blur radius beyond glassBlurMax cheaply
    // (no extra texture lookups, at some quality loss). 0.0 disables. See the tour doc.
    property real glassBlurMultiplier: _s.getFloat("glassBlurMultiplier", 0.0)
    property real glassRadius:      _s.getFloat("glassRadius", 12)
    property real glassBorderWidth: _s.getFloat("glassBorderWidth", 1)
}
