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

The default desktop output is one console destination plus one rotating file sink
in both Debug and Release. On Windows the console destination sends each message
through `OutputDebugString` while a native debugger is attached, and to stderr
otherwise. This keeps Qt Creator's CDB output visible without sending the same
message through both channels. Attaching or detaching a debugger changes the route
for subsequent messages. Explicit INI stdout/stderr sinks keep their chosen stream.

## logging.ini reference

```ini
[logger]
; --- Output targets ---
stdout=false              ; Mirror log messages to stdout
stdout_color=false        ; Colorize stdout output (ANSI terminals)
platform_std_log=true     ; Automatic desktop console destination (see above)

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

## Category rules and precedence

`logging.ini` configures destinations, formatting, and file rotation. The merged
`engine.toml` settings configure Qt category rules:

```toml
[engine.logging]
rules = [[
    "*.verbose=false",
    "settings.*=true",
]]
```

Both `rules = ["..."]` and the legacy `rules = [["..."]]` are supported.
Rules are applied when `DsEnvironment::loadEngineSettings()` loads the engine,
then updated when its settings files reload or runtime overrides change. Messages
emitted before engine settings load use the Qt rules already in effect. Removing
the rules or setting an empty list clears the application rule layer.

Engine settings merge from lowest to highest priority:

1. Registered defaults.
2. `%APP%/settings/engine.toml`.
3. `%APP%/settings/%CFG_FOLDER%/engine.toml`, when configured.
4. `%LOCAL%/settings/%PP%/engine.toml`, when the project path is configured.
5. `%LOCAL%/settings/%PP%/%CFG_FOLDER%/engine.toml`, when configured.
6. Files listed in `engine.extra.engine`, in list order, each using that same
   directory order.
7. Runtime overrides.

Rule lists replace earlier lists; they are not concatenated across files.
`Settings.engine.provenance("engine.logging.rules")` in QML, or the equivalent
`SettingsFile::provenance()` call in C++, identifies the winning source.

DsQt applies this merged list through `QLoggingCategory::setFilterRules()`.
[Qt's category-rule precedence](https://doc.qt.io/qt-6/qloggingcategory.html#logging-rules),
from lowest to highest, is Qt's installation `qtlogging.ini`, user QtProject
`qtlogging.ini`, the application rules (including the engine list),
`QT_LOGGING_CONF`, then `QT_LOGGING_RULES`. Within each source, later matching
rules win. A custom `QLoggingCategory::installFilter()` bypasses text rules.
Other application calls to `setFilterRules()` replace that same application layer.

The optional INI `filter_rules` and `regexp_filter` are additional QtLogger
pipeline filters. They can discard messages Qt enabled; they cannot restore a
message disabled by Qt category rules. The DsQt INI is read only from
`%APP%/settings/logging.ini` at application construction; it does not participate
in TOML stacking or live reload.

### Checking output in terminals and Qt Creator

Build the `ds_logging_probe` target with tests enabled. It prints unique
`DSQT_PROBE_*` markers and the resolved log path. Run it in Qt Creator using both
Run and Debug, and from PowerShell or Command Prompt. Test Debug and Release,
with no INI, the standard INI, and explicitly enabled multiple sinks.

Each enabled destination should receive one `DSQT_PROBE_VISIBLE` marker. The
standard desktop configuration produces one on stderr (or Windows debugger output
while attached) and one in the log file. Outside a Windows debugger, enabling both
`stdout` and `platform_std_log` produces one on each console stream, so combining
those streams displays two. Enabling `stderr` together with `platform_std_log`
also displays two when the platform route uses stderr. Under a Windows debugger
those explicitly requested streams remain separate from the automatic debugger
destination. Use only one console destination when one visible copy is wanted.

In the tested Qt Creator 20.0.2 CDB configuration, Run captures stdout and stderr,
while Debug displays debugger output and does not display the explicit stdout
copy. Thus `stdout=true` plus `platform_std_log=true` displays two copies in Run
and one in that Debug view, while the file receives one in both modes.

The rule hierarchy is platform independent. The bundled default platform sink is
stderr on Linux and macOS; QtLogger can instead use Apple unified
logging if built with `QTLOGGER_OSLOG`. Native IDE/debugger capture should be
verified separately on each operating system.

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

DsQt checks that the directory and file can be opened before enabling file logging.
If this fails, it reports one `DsQt file logging disabled:` warning and continues
with console output. A console destination is added if a file-only configuration
would otherwise have no console output. `getLogPath()` returns an empty string in
this case. This check covers startup; it does not guarantee that a later disk or
permission failure can be recovered automatically.

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
