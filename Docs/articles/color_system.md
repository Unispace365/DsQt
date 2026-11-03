\page color_system Color System: Two-Tier Tokens and Qt Palette

[TOC]

This document describes how to structure colors for white-label Waffles UIs using a
**two-tier token system** that maps cleanly onto Qt Quick's `palette`. It explains the model,
what changes in Figma and in code, and what `palette` deliberately does *not* cover.

\see best_practices For general QML authoring guidance.

# The problem

The design system names colors by **tone** — a numbered lightness ramp such as
`Tonal 0 (#191F25)`, `Tonal 10 (#2E3439)`, `White`, plus alpha variants like `Tonal 0 80%`.
A tonal ramp is great for designers: it is systematic and easy to extend.

Qt Quick's `palette`, on the other hand, names colors by **role** — a small, fixed set of
semantic slots (`window`, `base`, `text`, `button`, `highlight`, `accent`, …) split across
three state groups (`active`, `disabled`, `inactive`).

These two systems do not line up. `Tonal 0` does not "mean" anything to Qt — it is just a
value. If we shove tonal values straight into palette slots we lose the design intent and end
up with brittle, hard-to-retheme code. Today the codebase shows the symptom: some places use
`palette.*` (e.g. `IconButton`, `TopOuterControls`) while others hardcode the same hex values
(`#191F25`, `#2E3439` in `DsQuickMenu`, the glass defaults in `DsWaffleStage`). Those hardcoded
values *are* `Tonal 0` and `Tonal 10` — they just aren't connected to anything.

# The two-tier model

Do not choose tonal *or* roles. Layer them. This is the standard approach used by mature design
systems (Material 3, the W3C design-tokens work, etc.).

## Tier 1 — Primitives (the raw ramp)

The literal palette designers build from. These are values, not meanings.

| Primitive     | Value        |
|---------------|--------------|
| `tonal0`      | `#191F25`    |
| `tonal0_80`   | `#191F25CC`  |
| `tonal10`     | `#2E3439`    |
| `white`       | `#FFFFFF`    |
| `accent`      | brand colour |

(The ramp typically extends — `tonal20`, `tonal30`, … — and brands may add hues. Primitives
are the only place raw hex appears.)

## Tier 2 — Semantic roles (aliases that point at primitives)

A role says **what a colour is for**, not how light it is. Each role is an alias to a primitive.
Role names are chosen to line up with Qt's palette.

| Role            | Points at    | Used for                                  |
|-----------------|--------------|-------------------------------------------|
| `surface`       | `tonal0`     | Panel / window / control backgrounds      |
| `surfaceVariant`| `tonal10`    | Raised surfaces, separators               |
| `stroke`        | `tonal10`    | Borders / outlines                        |
| `surfaceText`     | `white`      | Text and icons on a surface               |
| `accent`        | `accent`     | Selection, focus, primary actions         |
| `accentText`      | `white`      | Text/icons on an accent fill              |
| `scrim`         | `tonal0_80`  | Translucent overlays (e.g. the glass tint)|

> **QML naming gotcha:** the Material convention is `onSurface`/`onAccent`, but QML reserves
> identifiers starting with `on` + an uppercase letter for signal handlers. A `property color
> onSurface` silently fails to resolve (reads as default/black), so we use `surfaceText`/
> `accentText` instead.

Designers keep their tonal ramp; developers get stable role names. Re-theming means
**re-pointing the roles**, never touching the call sites.

# How Qt's palette works

- In Qt 6, **every `Item` has a `palette`** and it **propagates down the item tree**. Set it once
  high up (the window or `DsWaffleStage`) and all descendants — including Qt Quick Controls —
  inherit it. This is the white-label lever: swap the palette, re-theme everything.
- The role set is fixed and semantic: `window`, `windowText`, `base`, `alternateBase`, `text`,
  `button`, `buttonText`, `brightText`, `light`, `midlight`, `mid`, `dark`, `shadow`,
  `highlight`, `highlightedText`, `placeholderText`, `accent`.
- There are three groups — `active`, `disabled`, `inactive`. Unspecified groups fall back to
  `active`. This is why **disabled handling is automatic**: define `palette.disabled.*` once and
  every control dims correctly.

## Mapping Tier-2 roles to palette slots

| Semantic role    | Qt palette slot(s)                          |
|------------------|---------------------------------------------|
| `surface`        | `window`, `base`                            |
| `surfaceVariant` | `midlight` / `mid`                          |
| `stroke`         | `dark` / `mid`                              |
| `surfaceText`      | `text`, `windowText`, `buttonText`          |
| `surface` (btn)  | `button`                                    |
| `accent`         | `accent`, `highlight`                       |
| `accentText`       | `highlightedText`, `brightText`             |

# What changes

## In Figma

Add a **semantic roles** variable collection that *references* the existing tonal primitives
(Figma variables can alias other variables). The tonal collection stays as-is. Components bind
to **roles** (`surface`, `surfaceText`, …), not to `Tonal N` directly.

## In code

1. A **`Theme` singleton** holds Tier-1 primitives and Tier-2 role aliases (the single source of
   truth, synced from the Figma variables — now possible via the Figma MCP).
2. Assign `palette` from the roles **once**, high in the tree (the app window or `DsWaffleStage`),
   so Controls inherit it.
3. Controls already read `palette.*` (see `IconButton`, `TopOuterControls`) — they need no
   change. Remove the scattered hardcoded hex (`DsQuickMenu`, glass defaults) in favour of
   `Theme` / `palette`.

```qml
// Theme.qml  (pragma Singleton)
pragma Singleton
import QtQuick
QtObject {
    id: theme
    // Tier 1 — primitives
    readonly property color tonal0:    "#191F25"
    readonly property color tonal0_80: "#191F25CC"
    readonly property color tonal10:   "#2E3439"
    readonly property color white:     "#FFFFFF"
    readonly property color accent:    "#00ADF7"
    // Tier 2 — semantic roles (alias primitives)
    readonly property color surface:        tonal0
    readonly property color surfaceVariant: tonal10
    readonly property color stroke:         tonal10
    readonly property color surfaceText:      white
    readonly property color accentText:       white
    readonly property color scrim:          tonal0_80   // glass tint
}
```

```qml
// High in the tree (window / DsWaffleStage): derive palette from roles once.
palette.window:          Theme.surface
palette.base:            Theme.surface
palette.button:          Theme.surface
palette.text:            Theme.surfaceText
palette.buttonText:      Theme.surfaceText
palette.dark:            Theme.stroke
palette.accent:          Theme.accent
palette.highlight:       Theme.accent
palette.highlightedText: Theme.accentText
// disabled group → automatic dimming
palette.disabled.text:   Qt.alpha(Theme.surfaceText, 0.4)
```

# What palette does NOT carry

`palette` is for **flat, opaque, semantic colours**. Keep the following as `Theme` tokens (or
component properties), not palette roles:

- **Non-colour values** — corner radius, blur amount, spacing. Not colours at all.
- **The glass effect** — the viewer body is a blurred *sample* of the backdrop with a translucent
  tint on top. The tint colour is a token (`scrim` / `tonal0_80`); the blur and radius are tokens;
  none of these are a palette role. Palette only themes a viewer's *chrome* (title text, buttons),
  not its glass body.
- **Interaction states beyond disabled** — the design has `Default / Disabled / Selected /
  Pressed`. Palette groups cover only `disabled`. `Selected`/`Pressed`/`Hovered` are control
  *states* handled by control logic (and may use the `accent` role), not palette groups.

# White-label workflow

1. Per brand, define a role set (`surface`, `surfaceText`, `accent`, …). Brands may share the tonal
   primitives or supply their own.
2. Point the brand's `Theme` roles at its primitives.
3. Everything else is unchanged: `palette` re-derives from the roles, Controls inherit it, and the
   glass reads its tint/blur/radius tokens. One swap re-themes the whole UI.

# Summary

- **Primitives** = the tonal ramp (values). **Roles** = what colours are for (meaning). Keep both.
- Name roles to match Qt's palette; assign `palette` from roles once, high in the tree; let it
  propagate. Controls theme for free, disabled handling is automatic.
- Palette is colours only — glass tint/blur/radius and `Selected`/`Pressed` stay as tokens.
- White-label = re-point roles, not rewrite call sites.
