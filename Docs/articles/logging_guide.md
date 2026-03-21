# Logging Configuration Guide

DsQt applications use [QtLogger](https://github.com/yamixst/qtlogger/blob/main/docs/index.md)
with rotating file sinks for structured, size-managed log output. Logging is
initialised automatically when your application inherits from `DsGuiApplication`.

## Quick start

Place a `logging.ini` file in your application's `settings/` directory
(i.e. `<exe_dir>/settings/logging.ini`). If the file is missing, sensible
defaults are used.

## Default behaviour (no INI file)

When no `logging.ini` is found the logger writes to:

```
<Documents>/downstream/logs/<AppName>/<AppName>.log.txt
```

With the following defaults:

| Setting              | Default   |
|----------------------|-----------|
| Max file size        | 1 MB      |
| Max file count       | 5         |
| Rotate on startup    | Yes       |
| Rotate daily         | No        |
| Async                | Yes       |

## logging.ini reference

```ini
[logger]
; --- Output targets ---
stdout=false              ; Mirror log messages to stdout
stdout_color=false        ; Colorize stdout output (ANSI terminals)
platform_std_log=true     ; Use platform logging (e.g. OutputDebugString on Windows)

; --- File rotation ---
max_file_size=1048576     ; Rotate when the active log exceeds this size (bytes)
max_file_count=730        ; Number of rotated files to keep (oldest are deleted)
rotate_on_startup=false   ; Rotate the previous log on application start
rotate_daily=true         ; Rotate when the date changes
compress_old_files=false  ; GZip-compress rotated log files
async=true                ; Write log messages on a background thread

; --- Log file path ---
; Can be a full file path, a directory, or omitted entirely.
;   Full path : C:/logs/myapp/output.log.txt   -> used as-is
;   Directory : C:/logs/myapp                   -> <AppName>.log.txt is appended
;   Omitted   :                                 -> falls back to the default path
; The path supports DsEnv variable expansion (e.g. %APP%, %DOCUMENTS%).
path=
```

> For a full list of INI keys and their behaviour, see the
> [QtLogger docs](https://github.com/yamixst/qtlogger/blob/main/docs/index.md).
> **Note:** `DsGuiApplication` intercepts the `path` key before passing the
> INI to QtLogger. It expands DsEnv variables (e.g. `%APP%`), treats
> extension-less values as directories and appends `<AppName>.log.txt`, and
> falls back to a default path when the key is empty. The resolved path is
> written back into the settings so QtLogger always receives a fully qualified
> file path.

### Path resolution

The `path` key accepts three forms:

1. **Full file path** (has a file extension) -- used verbatim.
   ```ini
   path=C:/logs/myapp/output.log.txt
   ```
2. **Directory only** (no file extension) -- the default log filename
   `<AppName>.log.txt` is appended automatically.
   ```ini
   path=C:/logs/myapp
   ```
3. **Empty / commented out** -- the built-in default path is used:
   ```
   <Documents>/downstream/logs/<AppName>/<AppName>.log.txt
   ```

Intermediate directories are created automatically.

> **Caveat:** Directory detection is based on the presence of a file
> extension. If a directory name contains a dot (e.g. `C:/my.logs`), it
> will be treated as a file path. If the directory already exists on disk
> it will be detected correctly, but for new paths, either include the
> full filename or avoid dots in directory names.

## How log rotation works

The active log file always keeps its base name (e.g. `MyApp.log.txt`).
When a rotation is triggered the **current** file is renamed with a date
stamp and an index, then a fresh file is opened:

```
MyApp.log.txt                   <-- active log
MyApp.log.2026-03-20.1.txt      <-- first rotation on that date
MyApp.log.2026-03-20.2.txt      <-- second rotation on that date
MyApp.log.2026-03-19.1.txt      <-- previous day
```

### Rotation triggers

| Trigger            | INI key              | Description |
|--------------------|----------------------|-------------|
| **Size**           | `max_file_size`      | Rotates when the active log exceeds the configured byte limit. |
| **Startup**        | `rotate_on_startup`  | Rotates the previous session's log when the application starts. |
| **Daily**          | `rotate_daily`       | Rotates when the first message of a new calendar day arrives. |

Multiple triggers can be active at the same time.

### Old file cleanup

After every rotation the logger counts existing rotated files. If the
count exceeds `max_file_count` the oldest files are deleted. When
`compress_old_files` is enabled, rotated files are GZip-compressed
(`.gz` suffix) before the count check.

## Startup banner

Every DsQt application prints a banner at the top of each log session:

```
========================================
MyApp v 1.0.0
DsQt 0.5.0 | Qt 6.9.0 | PID: 12345
Started: 2026-03-20T10:30:00
========================================
```

This includes the application name and version, the DsQt library version,
the Qt runtime version, the process ID, and the start timestamp.

## Adding logging to an existing project

A starter `logging.ini` is included in `Examples/ClonerSource/settings/`.
Copy it into your application's `settings/` directory and adjust the values
to suit your needs:

```
cp Examples/ClonerSource/settings/logging.ini <your_app>/settings/logging.ini
```

At minimum, review the `path` key — clear it to use the default location,
or set it to your preferred log directory or file path.
