# DsQt Settings Guide

This guide covers how to write and use TOML configuration files in DsQt applications. The settings system supports two main file types: `engine.toml` for engine-level configuration and app settings files (like `app_settings.toml`) for application-specific configuration.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Simple Values](#simple-values)
3. [Values with Metadata](#values-with-metadata)
4. [Data Types](#data-types)
5. [Colors](#colors)
6. [Geometry Types](#geometry-types)
7. [Date and Time](#date-and-time)
8. [Lists and Objects](#lists-and-objects)
9. [Path Variables](#path-variables)
10. [Accessing Settings from QML](#accessing-settings-from-qml)
11. [Accessing Raw TOML Data](#accessing-raw-toml-data)
12. [File Organization](#file-organization)

---

## Basic Concepts

DsQt uses [TOML](https://toml.io/) format for configuration files. Settings are organized in a hierarchy using sections (tables) denoted by `[section.name]` headers. The settings system supports:

- Automatic type conversion
- Metadata for UI hints and validation
- Path variable expansion (`%APP%`, `%LOCAL%`, etc.)
- File stacking (later files override earlier ones)

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

## Values with Metadata

For UI editors or validation, you can add metadata to any value using the array format:

```toml
# Format: key = [value, {metadata}]

# Simple string with metadata
project_path = ["my_project", {type="string", restart="full"}]

# Integer with range hints
volume = [75, {type="int", min=0, max=100}]

# Float with precision hint
scale = [1.5, {type="float", step=0.1}]
```

### Common Metadata Fields

| Field | Description |
|-------|-------------|
| `type` | Semantic type hint (`"string"`, `"int"`, `"float"`, `"color"`, `"rect"`, etc.) |
| `restart` | Action required on change (`"full"`, `"partial"`, `"none"`) |
| `test` | Test/development flag |
| `min`, `max` | Range constraints for numbers |
| `step` | Increment step for UI sliders |

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

---

## Colors

Colors can be specified in multiple formats. DsQt automatically detects the format.

### Simple Object Format (Recommended)

```toml
# RGB with alpha (0.0-1.0 range)
background = {r=0.2, g=0.4, b=0.6, a=1.0}

# RGB without alpha (defaults to 1.0)
foreground = {r=1, g=0.5, b=0}

# HSV format
accent_hsv = {h=0.6, s=0.8, v=0.9}

# HSL format
accent_hsl = {h=0.6, s=0.8, l=0.5}

# CMYK format
print_color = {c=0.2, m=0.4, y=0.6, k=0.1}
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

### Array Format with Metadata

When you need to specify the color space explicitly:

```toml
# Float RGB (0.0-1.0)
color_rgb = [[0.5, 0.3, 0.1], {type="color", element_type="float", array_color_type="rgb"}]

# Float RGBA
color_rgba = [[0.5, 0.3, 0.1, 0.8], {type="color", element_type="float", array_color_type="rgb"}]

# Integer RGB (0-255)
color_int = [[128, 64, 32], {type="color", element_type="int", array_color_type="rgb"}]

# HSV with metadata
color_hsv = [[0.3, 0.8, 0.9], {type="color", element_type="float", array_color_type="hsv"}]

# HSL with metadata
color_hsl = [[0.3, 0.8, 0.5], {type="color", element_type="float", array_color_type="hsl"}]

# CMYK with metadata
color_cmyk = [[0, 0.26, 0.99, 0.1], {type="color", element_type="float", array_color_type="cmyk"}]

# Grayscale
gray = [[0.5], {type="color", element_type="float", array_color_type="rgb"}]
gray_alpha = [[0.5, 0.8], {type="color", element_type="float", array_color_type="rgb"}]
```

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

### Points

```toml
# Object format
position = {x=100, y=200}

# Alternative keys
position2 = {x1=100, y1=200}

# Array format (double brackets)
position3 = [[100, 200]]
```

### Sizes

```toml
# Object format
window_size = {w=1920, h=1080}

# Array format
icon_size = [[64, 64]]
```

### Rectangles

```toml
# XYWH format (x, y, width, height)
bounds = {x=10, y=20, w=400, h=300}

# Point-to-point format (x1, y1, x2, y2)
region = {x1=10, y1=20, x2=410, y2=320}

# Array format (defaults to XYWH)
area = [[10, 20, 400, 300]]

# With layout metadata
crop_area = [{x=0, y=0, w=1920, h=1080}, {type="rect", layout="xywh"}]
```

### Vectors

```toml
[transform]
# 2D vector
offset = [[10, 20]]

# 3D vector
position = [[100, 200, 50]]

# 4D vector (or quaternion)
rotation = [[0, 0, 0, 1]]
```

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

### String Formats

```toml
# ISO 8601 (recommended)
date_iso = "2024-03-15"
time_iso = "17:30:30"
datetime_iso = "2024-03-15T17:30:30"

# RFC 2822
date_rfc = "15 Mar 2024"

# Text format
date_text = "Fri Mar 15 2024"
```

---

## Lists and Objects

### Simple Lists

```toml
# Note: Use double brackets for raw arrays
tags = [["red", "green", "blue"]]

numbers = [[1, 2, 3, 4, 5]]

# Mixed types
mixed = [[10, "string", 3.14, {r=1, g=0, b=0, a=1}]]
```

### Lists with Metadata

```toml
# List with type hints
items = [
    [10, "string", 3.14, {r=1, g=0, b=0, a=1}],
    {types=["int", "string", "float", "color"]}
]
```

### Lists of Objects

```toml
# Method 1: Inline array
buttons = [
    [
        {label="OK", action="confirm"},
        {label="Cancel", action="cancel"}
    ],
    {type="QVariantMap"}
]

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

---

## Path Variables

DsQt supports path variable expansion for portable configurations:

| Variable | Description | Example |
|----------|-------------|---------|
| `%APP%` | Application folder | `C:/MyApp/` |
| `%LOCAL%` | User's downstream documents | `Documents/downstream/` |
| `%PP%` | Project path (from engine.project_path) | `my_project` |
| `%CFG_FOLDER%` | Configuration folder | `config_v2` |
| `%DOCUMENTS%` | User's Documents folder | `C:/Users/Name/Documents/` |
| `%SHARE%` | Shared/ProgramData folder | `C:/ProgramData/` |
| `%ENV%(VAR)` | Environment variable | `%ENV%(HOME)` |

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
# Watch paths for hot reloading
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

## Accessing Settings from QML

Use `DsSettingsProxy` to access settings from QML:

### Basic Usage

```qml
import Dsqt

Item {
    // Create a settings proxy
    DsSettingsProxy {
        id: settings
        target: "app_settings"  // Name of the settings collection
        prefix: "window"        // Optional: prepended to all keys
    }

    // Access values with type-specific methods
    width: settings.getInt("width", 800)       // → looks up "window.width"
    height: settings.getInt("height", 600)
    opacity: settings.getFloat("opacity", 1.0)
    title: settings.getString("title", "Untitled")
    fullscreen: settings.getBool("fullscreen", false)
}
```

### Available Methods

```qml
DsSettingsProxy {
    id: settings
    target: "app_settings"
}

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

// Collections
settings.getList("key", [])           // Returns array
settings.getObj("key", {})            // Returns object/map

// Load additional settings file
settings.loadFromFile("extra_settings.toml")
```

### Using Prefixes

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

### Multiple Settings Collections

```qml
Item {
    // Engine settings
    DsSettingsProxy {
        id: engineSettings
        target: "engine"
        prefix: "engine"
    }

    // App-specific settings
    DsSettingsProxy {
        id: appSettings
        target: "app_settings"
    }

    // Use both
    width: engineSettings.getInt("window.width", 800)
    primaryColor: appSettings.getColor("colors.primary", "blue")
}
```

---

## Accessing Raw TOML Data

For advanced use cases, you can access the raw TOML data from C++:

### Getting Values with Metadata

```cpp
#include <settings/dsSettings.h>

// Get or create settings collection
auto [existed, settings] = dsqt::DsSettings::getSettingsOrCreate("app_settings");

// Load a settings file
DsEnv::loadSettings("app_settings", "app_settings.toml");

// Get value with metadata
auto result = settings->getWithMeta<QString>("engine.project_path");
if (result.has_value()) {
    auto [value, metaTable, filePath] = result.value();

    qDebug() << "Value:" << value;
    qDebug() << "From file:" << QString::fromStdString(filePath);

    if (metaTable) {
        // Access metadata
        auto type = metaTable->get("type");
        auto restart = metaTable->get("restart");
    }
}
```

### Getting Raw TOML Nodes

```cpp
// Get raw node view with metadata
auto nodeResult = settings->getNodeViewWithMeta("my.complex.setting");
if (nodeResult.has_value()) {
    auto [nodeView, metaTable, filePath] = nodeResult.value();

    // Work with raw TOML node
    if (nodeView.is_array()) {
        auto* arr = nodeView.as_array();
        for (auto& item : *arr) {
            // Process each item
        }
    } else if (nodeView.is_table()) {
        auto* tbl = nodeView.as_table();
        for (auto& [key, value] : *tbl) {
            // Process each key-value pair
        }
    }
}

// Convert any node to QVariant
QVariant variant = dsqt::tomlNodeViewToQVariant(nodeView);
```

### Inspecting the Settings Stack

When debugging overrides from multiple files:

```cpp
// Get all values for a key from all loaded files
auto stack = settings->getNodeViewStackWithMeta("engine.window.width");

for (auto& [filePath, nodeWithMeta] : stack) {
    if (nodeWithMeta.has_value()) {
        auto [node, meta, path] = nodeWithMeta.value();
        qDebug() << "File:" << QString::fromStdString(filePath);
        // The last entry is the effective value
    }
}
```

### Raw Node Access

```cpp
// Get pointer to raw TOML node
toml::node* rawNode = settings->getRawNode("some.setting");
if (rawNode) {
    if (rawNode->is_string()) {
        std::string value = rawNode->as_string()->get();
    } else if (rawNode->is_integer()) {
        int64_t value = rawNode->as_integer()->get();
    }
    // etc.
}

// Get only from base file (ignore overrides)
toml::node* baseNode = settings->getRawNode("some.setting", true);
```

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
project_path = ["my_project", {type="string", restart="full"}]
idle_timeout = 600

# Additional settings files to load
[engine.extra]
app_settings = ["content_settings.toml", "menu.toml"]
engine = ["bridgesync.toml", "engine.font.toml"]

# Hot reload configuration
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

| Type | Simple | With Metadata |
|------|--------|---------------|
| String | `key = "value"` | `key = ["value", {type="string"}]` |
| Integer | `key = 42` | `key = [42, {min=0, max=100}]` |
| Float | `key = 3.14` | `key = [3.14, {step=0.1}]` |
| Boolean | `key = true` | `key = [true, {type="bool"}]` |
| Color | `key = {r=1,g=0,b=0}` | `key = [{r=1,g=0,b=0}, {type="color"}]` |
| Array | `key = [[1,2,3]]` | `key = [[1,2,3], {type="array"}]` |

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
s.getList("key", [])
s.getObj("key", {})
```
