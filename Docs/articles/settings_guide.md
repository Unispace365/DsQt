# DsQt Settings Guide

This guide covers how to write and use TOML configuration files in DsQt applications. The settings system supports two main file types: `engine.toml` for engine-level configuration and app settings files (like `app_settings.toml`) for application-specific configuration.

> **Note:** This guide describes the rewritten settings system (`dsqt::Settings` / `dsqt::SettingsFile`, built on toml++). The old `DsSettings` class (shared-pointer collections, `getWithMeta`, raw `toml::node` access, `setDateFormat`) has been removed.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Simple Values](#simple-values)
3. [Legacy Metadata Format](#legacy-metadata-format)
4. [Data Types](#data-types)
5. [Colors](#colors)
6. [Geometry Types](#geometry-types)
7. [Date and Time](#date-and-time)
8. [Lists and Objects](#lists-and-objects)
9. [Path Variables](#path-variables)
10. [Value References](#value-references)
11. [Overrides, Defaults, and Live Reload](#overrides-defaults-and-live-reload)
12. [Accessing Settings from QML](#accessing-settings-from-qml)
13. [Accessing Settings from C++](#accessing-settings-from-c)
14. [Settings Viewer](#settings-viewer)
15. [File Organization](#file-organization)

---

## Basic Concepts

DsQt uses [TOML](https://toml.io/) format for configuration files. Settings are organized in a hierarchy using sections (tables) denoted by `[section.name]` headers. The settings system supports:

- **Live, bindable QML access.** Every settings collection is stored in a `QQmlPropertyMap` tree exposed through the `Settings` singleton, so `Settings.engine.window.width` is a real QML binding, not a function call — it re-evaluates automatically when the underlying file changes on disk. This reactivity was the main motivation for rewriting the settings system; see [Accessing Settings from QML](#accessing-settings-from-qml).
- Automatic type conversion, including structural detection of colors, rectangles, points, sizes, and vectors from plain TOML tables (no metadata required)
- Path variable expansion (`%APP%`, `%LOCAL%`, etc.)
- File stacking (later files override earlier ones), plus a runtime **override** layer on top that survives reloads
- Automatic live reload — settings files (and the folders they live in) are watched on disk, and any change reloads and re-publishes the affected settings automatically
- `@a.b.c`-style references that resolve to another value in the same merged settings tree
- A legacy `[value, {metadata}]` array format, kept only for backward compatibility with older settings files

Named collections of settings are called **settings files** (e.g. `"engine"`, `"app_settings"`). Each is loaded from a `.toml` file of the same name (by convention) found by searching a configurable list of directories.

---

## Simple Values

The simplest settings are just key-value pairs:

```toml
# Strings
app_name = "My Application"
version = "1.0.0"

# Numbers
width = 1920
height = 1080
opacity = 0.85

# Booleans
debug_mode = true
fullscreen = false
```

Settings can be organized into sections:

```toml
[window]
width = 1920
height = 1080
fullscreen = false

[app]
name = "My Application"
version = "1.0.0"
```

Access these with dot notation: `window.width`, `app.name`

---

## Legacy Metadata Format

Older settings files (and some hand-written examples still in the repo) wrap a value together with a metadata table:

```toml
# Format: key = [value, {metadata}]
project_path = ["my_project", {type="string", restart="full"}]
volume = [75, {type="int", min=0, max=100}]
```

**This format is only kept for backward compatibility.** A two-element array is treated as `[value, {metadata}]` only when **both** of these hold:

1. The second element is a table **containing a `type` key**. A trailing table without a `type` key — `[10, {a=1}]` — is not metadata; the array loads literally, as a 2-element list.
2. The first element is **not** a table, *or* `type` is `"color"` or `"rect"`. This keeps an ordinary array of two tables (an inline list of objects, or a two-entry `[[array.of.tables]]`) from being collapsed to its first element, while still allowing the table-valued colour and rect forms below.

When both hold, the metadata is stripped and the first element becomes the value — with two exceptions, where the metadata actually changes how the value is read:

- `{type="color", element_type="float"|"int", array_color_type="rgb"|"hsv"|"hsl"|"cmyk"}` — reinterprets an array or table value as a `QColor` (see [Colors](#colors)).
- `{type="rect"}` on a 4-element array — reinterprets it as a `QRectF`. A table value with `{type="rect"}` needs no reinterpretation: it is picked up by the normal structural detection.

Every other metadata field (`restart`, `test`, `min`, `max`, `step`, custom `name`/`desc`, etc.) is **read and discarded**. It has no effect on the loaded value, on validation, or on UI behavior — nothing in the current codebase consumes those fields. Don't rely on them for new settings; they exist purely so old files still load without errors.

For new settings files, prefer plain values and plain TOML tables — the structural detection described in [Colors](#colors) and [Geometry Types](#geometry-types) covers the same ground without any metadata.

---

## Data Types

### Booleans

```toml
# Native boolean
enabled = true
visible = false

# String booleans (also work)
active = "true"      # → true
inactive = "false"   # → false
empty = ""           # → false
other = "anything"   # → true (non-empty, non-"false", non-"0")
```

### Integers

```toml
# Native integers
count = 42
negative = -100
large = 1000000

# From strings (auto-converted)
from_string = "1024"

# From floats (truncated)
from_float = 99.9  # → 99
```

### Floats

```toml
# Native floats
ratio = 1.5
percentage = 0.75
scientific = 3.14e-10

# From strings
from_string = "123.456"

# From integers
from_int = 100  # → 100.0
```

### Strings

```toml
# Basic strings
name = "Hello World"
path = "C:/path/to/file"

# Multi-line strings
description = """
This is a
multi-line string
"""

# Literal strings (no escape processing)
regex = 'C:\Users\name'
```

> **Caveat:** every string value is checked against `QColor::fromString()` while the file is loaded. If a string happens to be a valid hex color (`"#RRGGBB"`) or a recognized SVG/Qt color name (`"blue"`, `"steelblue"`, ...), it is silently loaded as a `QColor`, not a `QString` — even if the key has nothing to do with colors. Avoid color-name-like strings for unrelated settings, or read them back with `getString()` and be aware the underlying stored value is a `QColor` if this happens.

---

## Colors

Colors can be specified in multiple formats. DsQt automatically detects the format from a plain TOML table — no metadata is required.

### Object Format (Recommended)

Each color model is detected purely from its key names:

| Format | Keys | Channel ranges |
|--------|------|-----------------|
| RGB(A) | `r,g,b[,a]` | **Auto-detected:** if every given channel is between 0.0 and 1.0, all are treated as float 0.0–1.0; otherwise all are treated as 0–255 |
| HSV(A) | `h,s,v[,a]` | Always `h`: 0–360, `s`/`v`: 0–100, `a`: 0–255 |
| HSL(A) | `h,s,l[,a]` | Always `h`: 0–360, `s`/`l`: 0–100, `a`: 0–255 |
| CMYK(A) | `c,m,y,k[,a]` | Always `c`/`m`/`y`/`k`: 0–100, `a`: 0–255 |

Unlike RGB, HSV/HSL/CMYK channels are **not** auto-scaled between 0–1 and their natural range — they're always read on their natural scale. Any channel may also be given as a percentage string (e.g. `"50%"`), which is always evaluated as a fraction of that channel's max, regardless of how the other channels are written.

```toml
# RGB, auto-detected as float (all channels 0.0-1.0)
background = {r=0.2, g=0.4, b=0.6, a=1.0}

# RGB, auto-detected as 0-255 int (any channel outside 0.0-1.0 switches all of them)
highlight = {r=255, g=128, b=0}

# Percentage channel mixed with an int channel — still 0-255 for g
tinted = {r="50%", g=128, b=0}

# HSV — h in degrees (0-360), s/v in percent (0-100)
accent_hsv = {h=216, s=80, v=90}

# HSL — h in degrees (0-360), s/l in percent (0-100)
accent_hsl = {h=216, s=80, l=50}

# CMYK — all channels 0-100
print_color = {c=20, m=40, y=60, k=10}
```

### String Format

```toml
# Hex colors
hex_color = "#FF5500"
hex_with_alpha = "#80FF5500"  # alpha=0x80

# Named colors (SVG 1.0 color names)
named = "blue"
named2 = "steelblue"
```

For a complete list of supported named colors, see the [SVG Color Reference](svg_color_reference.md).

### Legacy Array Format (Deprecated)

Plain TOML arrays are **not** auto-detected as colors — `[0.5, 0.3, 0.1]` is just a list of numbers. To load an array as a color you must use the [legacy metadata](#legacy-metadata-format) form with `type="color"`:

```toml
# Float RGB (0.0-1.0)
color_rgb = [[0.5, 0.3, 0.1], {type="color", element_type="float", array_color_type="rgb"}]

# Float RGBA
color_rgba = [[0.5, 0.3, 0.1, 0.8], {type="color", element_type="float", array_color_type="rgb"}]

# Integer RGB (0-255)
color_int = [[128, 64, 32], {type="color", element_type="int", array_color_type="rgb"}]

# HSV / HSL / CMYK with metadata
color_hsv  = [[0.3, 0.8, 0.9], {type="color", element_type="float", array_color_type="hsv"}]
color_hsl  = [[0.3, 0.8, 0.5], {type="color", element_type="float", array_color_type="hsl"}]
color_cmyk = [[0, 0.26, 0.99, 0.1], {type="color", element_type="float", array_color_type="cmyk"}]

# Grayscale
gray       = [[0.5], {type="color", element_type="float", array_color_type="rgb"}]
gray_alpha = [[0.5, 0.8], {type="color", element_type="float", array_color_type="rgb"}]
```

`element_type` is honoured literally when present: `"float"` reads channels as 0.0–1.0, `"int"` as 0–255 (or 0–360 / 0–100 for hue and the HSV/HSL/CMYK channels). When it is **omitted**, the range is auto-detected the same way the plain object format does it — every channel within 0.0–1.0 is read as float, anything else as int:

```toml
# element_type omitted — auto-detected as float, since all channels are <= 1.0
color_auto = [[0.5, 0.5, 0.5, 1.0], {type="color"}]
```

Spell out `element_type` anyway on legacy entries whose channels could be read either way — `[[1, 0, 0]]` is a valid float red *and* a valid int near-black.

### Organizing Colors

```toml
[colors]
primary = {r=0.2, g=0.4, b=0.8, a=1.0}
secondary = {r=0.8, g=0.4, b=0.2, a=1.0}
background = {r=0.1, g=0.1, b=0.12, a=1.0}

[colors.status]
success = {r=0, g=0.8, b=0.2}
warning = {r=0.9, g=0.7, b=0}
error = {r=0.9, g=0.2, b=0.2}
```

---

## Geometry Types

Like colors, geometry types are detected structurally from a TOML **table**'s key names. Plain arrays are **not** auto-converted to points, sizes, rects, or vectors — see the caution below.

| Shape | Keys | Result |
|-------|------|--------|
| Point | `x, y` | `QPointF` |
| Size | `w, h` or `width, height` | `QSizeF` |
| Rect (XYWH) | `x, y, w, h` or `x, y, width, height` | `QRectF` |
| Rect (two-point) | `x1, y1, x2, y2` | `QRectF` |
| Vector3 | `x, y, z` | `QVector3D` |
| Vector4 | `w, x, y, z` | `QVector4D` |
| Quaternion | `scalar, x, y, z` | `QQuaternion` |

### Points

```toml
# Object format — recognized
position = {x=100, y=200}
```

> `{x1=100, y1=200}` on its own is **not** recognized as a point (only the full 4-key `{x1,y1,x2,y2}` shape is recognized, as a rect). If you need a lone alternate-key point, use `{x=100, y=200}`.

### Sizes

```toml
window_size = {w=1920, h=1080}
```

### Rectangles

```toml
# XYWH format (x, y, width, height)
bounds = {x=10, y=20, w=400, h=300}

# Point-to-point format (x1, y1, x2, y2)
region = {x1=10, y1=20, x2=410, y2=320}
```

### Vectors and Quaternions

```toml
[transform]
# 3D vector
position = {x=100, y=200, z=50}

# 4D vector
rotation4 = {w=1, x=0, y=0, z=0}

# Quaternion
rotation = {scalar=1, x=0, y=0, z=0}
```

There is no object-based `QVector2D` detection — use the point format (`{x=100, y=200}`, which loads as `QPointF`) for a 2D value.

### Plain Arrays Are Not Auto-Typed

```toml
# This loads as a plain list [10, 20] — NOT a QPointF or QVector2D
offset = [[10, 20]]

# This loads as a plain list [10, 20, 400, 300] — NOT a QRectF
area = [[10, 20, 400, 300]]
```

If you need a rect built from an array, use the [legacy metadata](#legacy-metadata-format) form:

```toml
crop_area = [[0, 0, 1920, 1080], {type="rect"}]
```

There is no array-based equivalent for points, sizes, or vectors — use the object formats above instead.

---

## Date and Time

### Native TOML Date/Time

```toml
# Local date
release_date = 2024-03-15

# Local time
start_time = 09:30:00

# Local datetime
meeting = 2024-03-15T09:30:00

# Datetime with timezone offset
deadline = 2024-03-15T17:00:00-05:00

# UTC datetime
created_utc = 2024-03-15T12:00:00Z

# With fractional seconds
precise_time = 2024-03-15T12:00:00.123456Z
```

### String Formats (ISO 8601 only)

```toml
date_iso = "2024-03-15"
time_iso = "17:30:30"
datetime_iso = "2024-03-15T17:30:30"
```

> The old system supported configurable custom/RFC 2822/free-text date-string formats (`setDateFormat`, `setCustomDateFormat`). That machinery has been removed. String values are converted to `QDate`/`QTime`/`QDateTime` using Qt's default (ISO 8601) string conversion only — formats like `"15 Mar 2024"` or `"Fri Mar 15 2024"` will no longer parse. Prefer native TOML date/time literals (shown above) wherever possible.

---

## Lists and Objects

### Simple Lists

A plain TOML array is read as a `QVariantList` directly, with no wrapping required:

```toml
tags = ["red", "green", "blue"]
numbers = [1, 2, 3, 4, 5]
```

You only need to wrap a list in an extra set of brackets — `[[ ... ]]` — to dodge two specific ambiguities that come from the still-supported [legacy metadata](#legacy-metadata-format) format:

- **A 2-item list whose 2nd item is a table carrying a `type` key**, e.g. `[10, {type="int", min=0}]`, is read as `[value, metadata]` — keeping `10` and discarding the table. Wrap it to keep both items: `pair = [[10, {type="int", min=0}]]`. A trailing table *without* a `type` key is not metadata, so `[10, {a=1}]` already loads as a 2-element list and needs no wrapping.
- **A 1-item list whose item is itself an array**, e.g. `[[1, 2]]` meant as "a list containing the sublist `[1, 2]`", is instead auto-unwrapped one level to the flat list `[1, 2]`. A genuinely nested single-sublist list needs a third bracket: `[[[1, 2]]]`.

Any other shape (empty, a single non-array item, 3+ items, or a 2-item list whose 2nd item isn't a `type`-bearing table) doesn't need extra brackets at all. That said, since it's easy to lose track of which shape you have, the settings files in this repo double-bracket raw lists defensively as a habit — it's always correct, even where it isn't strictly necessary:

```toml
# Defensive double-bracket style used throughout the example settings files
tags = [["red", "green", "blue"]]
numbers = [[1, 2, 3, 4, 5]]

# Mixed types — the trailing table has no `type` key, so this is never
# mistaken for metadata; the convention wraps it anyway
mixed = [[10, "string", 3.14, {r=1, g=0, b=0, a=1}]]
```

### Lists of Objects

```toml
# Method 1: Inline array. The double bracket is the house style, not a
# requirement — a list of objects is never mistaken for [value, metadata],
# because the 1st element is a table and the 2nd has no `type` key.
buttons = [[
    {label="OK", action="confirm"},
    {label="Cancel", action="cancel"}
]]

# Method 2: Array of tables syntax
[[menu.items]]
label = "File"
shortcut = "Ctrl+F"

[[menu.items]]
label = "Edit"
shortcut = "Ctrl+E"

[[menu.items]]
label = "View"
shortcut = "Ctrl+V"
```

> Older files sometimes wrap list values in the [legacy metadata](#legacy-metadata-format) form. `[[...], {type="QVariantMap"}]` unwraps as expected, and the metadata is discarded — it has no effect beyond the color/rect cases described earlier.
>
> Watch out for the `types` (plural) variant, e.g. `[[10, "a"], {types=["int", "string"]}]`. The unwrap keys off `type`, so `types` is **not** recognised as metadata and the entry loads as a literal 2-element list — `[[10, "a"], {types: [...]}]` — rather than the intended `[10, "a"]`. Rename the key to `type`, or drop the metadata table entirely. New files don't need it.

### Nested Objects

```toml
# Inline tables
user = {name="John", email="john@example.com", preferences={theme="dark", language="en"}}

# Section headers (more readable)
[user]
name = "John"
email = "john@example.com"

[user.preferences]
theme = "dark"
language = "en"
notifications = true

[user.preferences.display]
font_size = 14
line_height = 1.5
```

> **Caveat:** because color/rect/point/size/vector detection is purely structural (see above), a plain object whose keys happen to exactly match one of those key sets (e.g. a table with only `x` and `y` keys, or `r`, `g`, `b`) will be silently converted to that type instead of staying a `QVariantMap`. Avoid those exact key combinations for unrelated data.

---

## Path Variables

DsQt supports path variable expansion for portable configurations:

| Variable | Description | Example |
|----------|-------------|---------|
| `%APP%` | Application folder | `C:/MyApp/` |
| `%LOCAL%` | User's downstream documents folder | `Documents/downstream/` |
| `%DOCUMENTS%` | User's Documents folder | `C:/Users/Name/Documents/` |
| `%SHARED%` | Shared/ProgramData folder | `C:/ProgramData/Downstream/` |
| `%PP%` | Project path (from `engine.project_path`) | `my_project` |
| `%CFG_FOLDER%` | Configuration folder (from `configuration.toml`'s `config_folder`) | `config_v2` |
| `%RES%` | Resolved resource location (from `engine.resource.location`) | `file:///C:/.../resources/` |
| `%ENV%(VAR)` | Environment variable | `%ENV%(HOME)` |

A path containing a variable that can't be resolved is dropped rather than used as-is (e.g. when building search paths before `engine.project_path` has loaded).

### Usage Examples

```toml
[engine.resource]
# Local user data
location = "%LOCAL%/myapp_data/"
database = "%LOCAL%/myapp/db.sqlite"

# Application resources
fonts = "%APP%/data/fonts/"
images = "%APP%/data/images/"

# Environment variables
custom_path = "%ENV%(MY_APP_PATH)/resources/"

[engine.reload]
# Watch paths for hot reloading of QML/data (separate from settings-file watching)
paths = [
    {path = "%APP%/data/", recurse = true},
    {path = "%APP%/qml/", recurse = true},
]

# URL prefix mappings
prefixes = [
    {from = "qrc:/qt/qml/MyApp/", to = "file:///%APP%/"},
]
```

---

## Value References

A string of the form `@a.b.c` (an `@` followed by a dotted key path) is treated as a reference. It's resolved against the fully merged settings tree, so it can point at a value defined anywhere — even in a different file, or the same file at a different location:

```toml
[colors]
primary = {r=0.2, g=0.4, b=0.8, a=1.0}

[ui.button]
# Resolves to the same QColor as colors.primary
background = "@colors.primary"
```

References chase through chains of references (`@a` → `@b` → final value) and detect cycles (an unresolvable or circular reference is left as the raw string and logged as a warning). References inside array elements are **not** resolved — only references used as whole table values.

---

## Overrides, Defaults, and Live Reload

These capabilities are new in the rewritten settings system (the old system was read-only from TOML files).

### Live Reload

Every settings file's resolved paths — and every directory in the current search paths — are watched on disk automatically. Editing a `.toml` file, or dropping a new file into a watched folder, triggers an automatic reload and republishes the merged values (and any `bind()` callbacks fire again) with no extra code required.

### Defaults

`setDefault(key, value)` registers a fallback that fills in a key if it's absent from every loaded file. It doesn't overwrite a value that a file already provides.

### Overrides

A runtime override always wins over file data (and over defaults), and — unlike a plain in-memory variable — survives `reload()`:

```cpp
settingsFile->set<int>("window.width", 1024);      // typed, C++-only
settingsFile->setOverride("window.width", 1024);   // QVariant form, also callable from QML

settingsFile->resetOverride("window.width");       // back to the file/default value
settingsFile->resetOverrides();                    // clear all overrides on this file
```

Merge order (lowest to highest priority): **defaults → primary file → extra files → runtime overrides.**

If a file is reloaded and now contains the same value an override was masking, the override is automatically pruned so the file value shows through again.

`saveOverridesTo(filePath)` writes the current overrides out as TOML — merging into the existing file's other keys if it already exists, or creating a new file containing just the overrides. (toml++ doesn't preserve comments when rewriting an existing file.) The in-memory overrides are kept after saving.

### Inspecting where a value came from

```cpp
QString source = settingsFile->provenance("window.width");
// → a file path, "default", "override", or "" if the key isn't set anywhere
```

---

## Accessing Settings from QML

There are two ways to read settings from QML. **Direct property binding is the recommended, preferred approach** — it's the main reason the settings system was rewritten. `DsSettingsProxy` still exists as a non-reactive, drop-in-compatible fallback.

### Option A: Direct Property Binding (Recommended)

`Settings` is a QML singleton. Each registered settings collection is exposed as a named property on it whose value is a `SettingsFile` — and both `Settings` and `SettingsFile` are `QQmlPropertyMap`s, so every nested TOML table becomes a nested, live map. Dot-separated keys become chained property accesses, and — unlike calling a getter function — **these are real QML bindings**: when a file changes on disk (or an override is applied), the bound properties re-evaluate automatically, with no manual wiring:

```qml
import Dsqt

Item {
    width:  Settings.engine.world_dimensions.width  ?? 1920
    height: Settings.engine.world_dimensions.height ?? 1080
    color:  Settings.engine.ui.background_color     ?? "black"
}
```

Typed leaf values (colors, points, rects, dates, ...) come through as their native QML value type, so you can chain straight into their sub-properties too: `Settings.engine.window.destination.width`.

Because a collection (or a nested table within it) might not be loaded yet — e.g. `Settings.engine` is `undefined` until `Settings.add("engine")` has actually run — chain accesses with `?.` (optional chaining) and fall back with `??` (nullish coalescing) so the binding doesn't throw and instead resolves once the value becomes available:

```qml
Item {
    readonly property var setup: Settings.engine

    width:  setup?.world_dimensions?.width  ?? 1920
    height: setup?.world_dimensions?.height ?? 1080
    color:  setup?.ui?.background_color     ?? "black"
}
```

`?.` short-circuits to `undefined` if `setup` or an intermediate table isn't there yet; `??` supplies the fallback in that case. The binding keeps re-evaluating and will pick up the real value the moment the file (or table) loads — you don't need to guard against load order manually.

```qml
Item {
    // Multiple collections, same pattern
    readonly property var engine: Settings.engine
    readonly property var app: Settings.app_settings

    width: engine?.window?.width ?? 800
    primaryColor: app?.colors?.primary ?? "blue"
}
```

### Option B: `DsSettingsProxy` (Non-Reactive, Drop-In)

`DsSettingsProxy` wraps a settings collection with the same imperative getter methods the old system used. It's a safe first step when porting existing QML without rewriting bindings, but **calling a getter is a plain function call — QML cannot track it as a dependency, so the UI will not update when the underlying setting changes.** Prefer Option A for anything that should stay in sync with a live-edited settings file.

```qml
import Dsqt

Item {
    DsSettingsProxy {
        id: settings
        target: "app_settings"  // Name of the settings collection — assigning this
                                 // registers/loads the file immediately
        prefix: "window"        // Optional: prepended to all keys
    }

    // Access values with type-specific methods (evaluated once, not reactive)
    width: settings.getInt("width", 800)       // → looks up "window.width"
    height: settings.getInt("height", 600)
    opacity: settings.getFloat("opacity", 1.0)
    title: settings.getString("title", "Untitled")
    fullscreen: settings.getBool("fullscreen", false)
}
```

Available methods:

```qml
// Primitives
settings.getString("key", "default")
settings.getInt("key", 0)
settings.getFloat("key", 0.0)
settings.getBool("key", false)

// Qt Types
settings.getColor("key", "black")     // Returns Qt.color
settings.getPoint("key", Qt.point(0,0))
settings.getSize("key", Qt.size(0,0))
settings.getRect("key", Qt.rect(0,0,0,0))
settings.getDate("key", new Date())
settings.getVec3("key", Qt.vector3d(0,0,0))
settings.getVec4("key", Qt.vector4d(0,0,0,0))
settings.getQuat("key", Qt.quaternion(1,0,0,0))

// Collections
settings.getList("key", [])           // Returns array
settings.getObj("key", {})            // Returns object/map
```

> There is no `loadFromFile()` method on `DsSettingsProxy` anymore. To load additional files into a settings collection, use `engine.extra` (see [File Organization](#file-organization)) or register another named collection with its own proxy `target`.

Prefixes work the same way they always did:

```qml
// Without prefix - full key paths
DsSettingsProxy {
    id: engineSettings
    target: "engine"
}
// Access: engineSettings.getInt("engine.window.width")

// With prefix - shorter keys
DsSettingsProxy {
    id: windowSettings
    target: "engine"
    prefix: "engine.window"
}
// Access: windowSettings.getInt("width")
```

---

## Accessing Settings from C++

The old `DsSettings` class (shared-pointer collections, `getWithMeta`, `getNodeViewWithMeta`, `getNodeViewStackWithMeta`, `getRawNode`) has been removed. TOML parsing is now fully isolated inside the settings implementation and is never exposed through a header — all C++ access goes through `QVariant`/`QVariantMap`, via two classes:

- **`dsqt::Settings`** — a QML singleton that owns the shared search paths and a registry of named `SettingsFile` instances.
- **`dsqt::SettingsFile`** — one loaded (and merged, and watched) settings collection.

### Getting a settings file

```cpp
#include <settings/dsSettings.h>
#include <core/dsEnvironment.h>

// The two built-in collections DsEnvironment loads at startup:
dsqt::SettingsFile* engine = dsqt::DsEnvironment::engineSettings();
dsqt::SettingsFile* app    = dsqt::DsEnvironment::appSettings();

// Register/load any other named collection (idempotent — safe to call repeatedly)
dsqt::Settings::add("content_settings");                       // loads content_settings.toml
dsqt::Settings::add("bridgesync", "bridgesync_config.toml");   // explicit filename

dsqt::SettingsFile* content = dsqt::Settings::instance().settingsFile("content_settings");
```

### Reading values

```cpp
// Static, thread-safe convenience lookup by collection name
QString path = dsqt::Settings::find<QString>("engine", "engine.project_path");

// Or via a SettingsFile pointer
int width = engine->find<int>("engine.window.width", 1920);   // find<T>(key, default)
int w2    = engine->getOr<int>("engine.window.width", 1920);  // same thing
int w3    = engine->get<int>("engine.window.width");          // default-constructed fallback

// Generic access
QVariant raw = engine->value("engine.window.width");
bool exists  = engine->contains("engine.window.width");

// The whole merged, reference-resolved tree
QVariantMap all = app->allSettings();
```

`find`/`get`/`getOr`/`value`/`contains`/`allSettings` are all safe to call from any thread. Mutating calls (`reload`, `setOverride`, `resetOverride`, `resetOverrides`, `setDefault`) must be called from the thread that owns the `SettingsFile` (normally the main thread) — calling them from another thread logs a `qCritical` and asserts in debug builds.

### Reacting to changes

```cpp
// Calls the callback immediately with the current value, then again on every change.
// Cleans itself up automatically when `this` is destroyed.
engine->bind<int>("engine.window.width", this, [this](int width) {
    setWindowWidth(width);
});

// Equivalent one-liner if you only have the collection name, not the SettingsFile pointer:
dsqt::Settings::bind<int>("engine", "engine.window.width", this, [this](int width) {
    setWindowWidth(width);
});

// Or listen for "something changed" without caring what:
connect(engine, &dsqt::SettingsFile::settingsRebuilt, this, &MyClass::onSettingsChanged);
```

### Writing values (overrides)

See [Overrides, Defaults, and Live Reload](#overrides-defaults-and-live-reload) for `setDefault`, `set`/`setOverride`, `resetOverride`, `resetOverrides`, `saveOverridesTo`, and `provenance`.

### Standalone SettingsFile (e.g. in tools/tests)

A `SettingsFile` doesn't need a `Settings` manager — construct it directly with explicit search paths:

```cpp
dsqt::SettingsFile settings(nullptr, {"/path/to/settings/dir"});
settings.setFileName("my_settings.toml");
auto value = settings.find<QString>("some.key", "fallback");
```

---

## Settings Viewer

A built-in debug UI lets you inspect and edit every registered settings collection while the app is running. Add it to your application shell:

```qml
import Dsqt

DsSettingsViewerHelper {
    id: settingsViewer
    onVisibleChanged: (isVisible) => { /* e.g. sync a menu checkbox */ }
}

// e.g. wire up to a checkable menu item
onSettingsTriggered: (isChecked) => { settingsViewer.setVisible(isChecked) }
```

It opens a window (a plain Qt Widgets `QWidget`, not QML) with one tab per registered `SettingsFile` — tabs rebuild automatically as collections are added — each with a search box above a three-column tree (Key / Value / Type):

- The search box filters the tree as you type: a case-insensitive substring match against the key, display value, or full dotted path. Only matching rows (and the ancestors needed to reach them) stay visible — everything else is hidden rather than shown in a separate results list.
- Overridden values are shown in **bold**; hovering any leaf shows a tooltip with its provenance (a file path, `"default"`, or `"override"`).
- Color values show a swatch, and double-clicking one opens a `QColorDialog`. Double-clicking any other leaf turns it into an inline text editor; the parsed value is applied as a runtime override (`setOverride`) as soon as you commit it.
- Double-clicking a list (array) node opens a dedicated editor dialog for adding, removing, and drag-reordering its elements; accepting it writes the whole list back as an override.
- Right-clicking a leaf offers **Revert** (if it's currently overridden) or **Open File** (if its provenance is a real file path, via the OS default application).
- **Save…** and **Restore…** buttons at the bottom write the current tab's overrides out to a chosen `.toml` file (`saveOverridesTo()`) or clear them (`resetOverrides()`), respectively.

This is a developer/debugging tool, not something to embed in an end-user QML UI.

---

## File Organization

### Standard Structure

```
my_project/
├── settings/
│   ├── engine.toml           # Core engine configuration
│   ├── app_settings.toml     # Main app settings
│   ├── engine.font.toml      # Font configuration
│   ├── bridgesync.toml       # Sync service config
│   ├── content_settings.toml # Content-specific settings
│   └── menu.toml             # Menu configuration
├── data/
│   ├── fonts/
│   └── images/
└── qml/
    └── Main.qml
```

### Engine.toml Structure

```toml
[engine]
project_path = "my_project"
idle_timeout = 600

# Additional settings files to load into other named collections
[engine.extra]
app_settings = ["content_settings.toml", "menu.toml"]
engine = ["bridgesync.toml", "engine.font.toml"]

# Hot reload configuration (watched data/qml paths — separate from settings-file watching)
[engine.reload]
active = true
paths = [
    {path = "%APP%/data/", recurse = true},
    {path = "%APP%/qml/", recurse = true},
]
prefixes = [
    {from = "qrc:/qt/qml/MyApp/", to = "file:///%APP%/"},
]

# Window settings
[engine.window]
mode = "window"           # "window", "fullscreen", "borderless"
displayIndex = 0
width = 1920
height = 1080

# Resource storage
[engine.resource]
location = "%LOCAL%/myapp/"
resource_db = "db.sqlite"

# Logging configuration
[engine.logging]
rules = [[
    "*.verbose=false",
    "settings.*=true",
]]
```

`[engine.extra]` entries load additional files into the *named* collection given by the key. That collection must already be registered — by the time `DsEnvironment::loadEngineSettings()` processes `engine.extra`, both `engine` and `app_settings` are already registered, so those two are always valid targets; any other collection name must be registered earlier. Extra-file content is merged in **after** that collection's own primary file and **before** any runtime overrides.

### App Settings Structure

```toml
# Root-level color
color = {r=0.2, g=0.4, b=0.8, a=1.0}

# Named colors
[colors]
primary = {r=0.2, g=0.4, b=0.8, a=1.0}
secondary = {r=0.8, g=0.4, b=0.2, a=1.0}

[colors.status]
success = {r=0, g=0.8, b=0.2}
error = {r=0.9, g=0.2, b=0.2}

# Platform identification
[platform]
id = "ABC123"

# UI configuration
[ui]
theme = "dark"
font_size = 14

# Template mappings
[playlist.templateMap]
default = "qrc:/qt/qml/MyApp/templates/Default.qml"
special = "qrc:/qt/qml/MyApp/templates/Special.qml"

# Application settings
[app]
mainView = "Main"
```

---

## Quick Reference

### Value Formats

| Type | Format |
|------|--------|
| String | `key = "value"` |
| Integer | `key = 42` |
| Float | `key = 3.14` |
| Boolean | `key = true` |
| Color (RGB, auto float/int) | `key = {r=1, g=0, b=0}` |
| Color (HSV/HSL, fixed range) | `key = {h=216, s=80, v=90}` |
| Color (CMYK, fixed 0-100) | `key = {c=0, m=100, y=100, k=0}` |
| Point | `key = {x=0, y=0}` |
| Size | `key = {w=100, h=100}` |
| Rect | `key = {x=0, y=0, w=100, h=100}` |
| Array (plain list) | `key = [[1,2,3]]` |
| Reference | `key = "@other.key"` |

The `key = [value, {metadata}]` form is legacy-only (see [Legacy Metadata Format](#legacy-metadata-format)) — don't use it in new files except for the color/rect array cases that have no plain-table equivalent.

### QML Access Cheatsheet

```qml
DsSettingsProxy { id: s; target: "app_settings" }

s.getString("key", "default")
s.getInt("key", 0)
s.getFloat("key", 0.0)
s.getBool("key", false)
s.getColor("key", "black")
s.getPoint("key", Qt.point(0,0))
s.getSize("key", Qt.size(0,0))
s.getRect("key", Qt.rect(0,0,0,0))
s.getVec3("key", Qt.vector3d(0,0,0))
s.getVec4("key", Qt.vector4d(0,0,0,0))
s.getQuat("key", Qt.quaternion(1,0,0,0))
s.getList("key", [])
s.getObj("key", {})
```
