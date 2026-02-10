#!/usr/bin/env python3
"""
DsQt Migration Script: Subdirectory -> Library

Migrates a DsQt project from using add_subdirectory() with the
DS_QT_PLATFORM_100 environment variable to using find_package(Dsqt)
with a pre-built, installed DsQt library.

Usage:
    python migrate_to_library.py <project_directory> [options]

Options:
    --dsqt-root <path>   Path to DsQt repo root (default: derived from script location)
    --dry-run            Show what would change without modifying files
    --no-backup          Skip the git backup prompt
    --reset              Reset project to the pre-migration backup branch
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


# ── DsQt internal targets that get replaced by library targets ──────────────

DSQT_INTERNAL_LINK_TARGETS = {
    "Dsqt",
    "Dsqtplugin",
    "Dsqtplugin_init",
    "Dsqt_resources_1",
    "Dsqt_resources_2",
    "Dsqt_resources_3",
    "Dsqt_resources_4",
    "Dsqt_resources_5",
    "TouchEngineQt",
    "WafflesUx",
}

DSQT_LIBRARY_LINK_TARGETS = [
    "Dsqt::Core",
    "Dsqt::Bridge",
    "Dsqt::TouchEngine",
    "Dsqt::Waffles",
]

DSQT_QML_DEPENDENCY_MAP = {
    "Dsqt": "Dsqt::Core",
    "WafflesUx": "Dsqt::Waffles",
    "TouchEngineQt": "Dsqt::TouchEngine",
}

DSQT_PLUGIN_MACROS = [
    ("Dsqt_CorePlugin", "Dsqt::Core"),
    ("Dsqt_BridgePlugin", "Dsqt::Bridge"),
    ("Dsqt_TouchEnginePlugin", "Dsqt::TouchEngine"),
    ("Dsqt_WafflesPlugin", "Dsqt::Waffles"),
]


# ── Parsing helpers ─────────────────────────────────────────────────────────

def parse_cmake_set(content, var_name):
    """Extract a set(VAR_NAME value) or set(VAR_NAME "value") from CMake content."""
    pattern = rf'set\(\s*{re.escape(var_name)}\s+"([^"]*?)"\s*\)'
    m = re.search(pattern, content)
    if m:
        return m.group(1)
    pattern = rf'set\(\s*{re.escape(var_name)}\s+(\S+)\s*\)'
    m = re.search(pattern, content)
    if m:
        return m.group(1)
    return None


def parse_project_version(content):
    """Extract version from project(... VERSION x.y ...) call."""
    m = re.search(r'project\s*\([^)]*VERSION\s+(\S+)', content)
    if m:
        return m.group(1)
    return None


def parse_qt_components(content):
    """Extract Qt6 component list from find_package(Qt6 ...)."""
    m = re.search(
        r'find_package\s*\(\s*Qt6[^)]*COMPONENTS\s+(.*?)\)',
        content, re.DOTALL
    )
    if not m:
        return []
    components_str = m.group(1)
    return re.findall(r'[A-Za-z]\w+', components_str)


def find_balanced_parens(content, start_pos):
    """Find the closing paren matching the open paren at start_pos."""
    depth = 0
    i = start_pos
    while i < len(content):
        if content[i] == '(':
            depth += 1
        elif content[i] == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def extract_block(content, keyword):
    """Extract the full block for a CMake command like target_link_libraries(...)."""
    pattern = re.compile(rf'\b{re.escape(keyword)}\s*\(')
    blocks = []
    for m in pattern.finditer(content):
        open_pos = m.end() - 1  # position of '('
        close_pos = find_balanced_parens(content, open_pos)
        if close_pos > 0:
            blocks.append(content[m.start():close_pos + 1])
    return blocks


def parse_qml_files_from_module(content):
    """Extract the QML_FILES list from qt_add_qml_module(...)."""
    blocks = extract_block(content, "qt_add_qml_module")
    if not blocks:
        return []
    block = blocks[0]
    qml_files = []
    # Find all QML_FILES sections (there can be multiple)
    parts = re.split(r'\bQML_FILES\b', block)
    for part in parts[1:]:  # skip the first part (before first QML_FILES)
        # Extract file paths until we hit another keyword or closing paren
        lines = part.split('\n')
        for line in lines:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            # Stop at CMake keywords
            if re.match(r'^(IMPORT_PATH|DEPENDENCIES|RESOURCES|URI|VERSION|NO_CACHEGEN|SOURCES|OUTPUT_DIRECTORY)\b', line):
                break
            # Could be a bare filename or a quoted filename
            tokens = re.findall(r'[^\s#]+', line)
            for token in tokens:
                token = token.strip('"')
                if token.endswith('.qml'):
                    qml_files.append(token)
    return qml_files


def parse_qml_dependencies(content):
    """Extract the DEPENDENCIES list from qt_add_qml_module(...)."""
    blocks = extract_block(content, "qt_add_qml_module")
    if not blocks:
        return []
    block = blocks[0]
    m = re.search(r'\bDEPENDENCIES\b(.*?)(?:\bQML_FILES\b|\bRESOURCES\b|\bSOURCES\b|\bIMPORT_PATH\b|\))', block, re.DOTALL)
    if not m:
        return []
    deps_str = m.group(1)
    deps = []
    for line in deps_str.split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        tokens = re.findall(r'[^\s#]+', line)
        for token in tokens:
            token = token.strip('"')
            if token and not token.startswith('#'):
                deps.append(token)
    return deps


def parse_link_libraries(content):
    """Extract target_link_libraries entries for the main executable.
    If multiple blocks match, use the largest one (most libraries)."""
    blocks = extract_block(content, "target_link_libraries")
    best_libs = []
    for block in blocks:
        if 'DS_EXE_NAME' in block and 'PRIVATE' in block:
            m = re.search(r'PRIVATE\s+(.*?)\)', block, re.DOTALL)
            if m:
                libs_str = m.group(1)
                libs = []
                for line in libs_str.split('\n'):
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    tokens = re.findall(r'[^\s#]+', line)
                    for token in tokens:
                        if token and not token.startswith('#'):
                            libs.append(token)
                if len(libs) > len(best_libs):
                    best_libs = libs
    return best_libs


def find_local_subdirectories(project_dir, content):
    """Find add_subdirectory() calls that are local (not DsQt)."""
    local_subdirs = []
    for m in re.finditer(r'add_subdirectory\s*\(\s*"?([^"\s)]+)"?', content):
        subdir = m.group(1)
        # Skip the DsQt library subdirectory
        if '$ENV' in subdir or 'DS_QT_PLATFORM' in subdir:
            continue
        # Check if it's a local directory
        full_path = os.path.join(project_dir, subdir)
        if os.path.isdir(full_path) and os.path.isfile(os.path.join(full_path, "CMakeLists.txt")):
            local_subdirs.append(subdir)
    return local_subdirs


def has_fetchcontent(content, name):
    """Check if a FetchContent_Declare exists for the given package name."""
    return bool(re.search(rf'FetchContent_Declare\s*\(\s*{re.escape(name)}\b', content))


def extract_fetchcontent_block(content, name):
    """Extract the full FetchContent_Declare(...) block for a package,
    plus FetchContent_GetProperties and FetchContent_MakeAvailable calls."""
    blocks = extract_block(content, "FetchContent_Declare")
    result_lines = []
    for block in blocks:
        if re.search(rf'FetchContent_Declare\s*\(\s*{re.escape(name)}\b', block):
            result_lines.append(block)
            break
    # Also grab GetProperties and MakeAvailable
    for m in re.finditer(rf'FetchContent_GetProperties\s*\(\s*{re.escape(name)}\s*\)', content):
        result_lines.append(m.group(0))
    for m in re.finditer(rf'FetchContent_MakeAvailable\s*\(\s*{re.escape(name)}\s*\)', content):
        result_lines.append(m.group(0))
    return '\n'.join(result_lines) if result_lines else None


def detect_dsqt_subdirectory(content):
    """Detect the DsQt add_subdirectory pattern and environment variable."""
    m = re.search(r'add_subdirectory\s*\(\s*"?\$ENV\{([^}]+)\}[^"]*"?', content)
    if m:
        return m.group(1)
    return None


def detect_dll_subdir(content):
    """Check if DS_QT_DLLS_SUBDIR is used."""
    return 'DS_QT_DLLS_SUBDIR' in content


# ── Generation helpers ──────────────────────────────────────────────────────

def generate_cmakelists(
    app_name, exe_name, project_name, project_desc,
    qt_components, qml_files, local_subdirs,
    extra_link_libs, extra_qml_deps,
    has_toml_fetchcontent, toml_fetchcontent_block,
    has_singleton_fnt,
    has_webengine_init,
    extra_cpp_files,
):
    """Generate the new CMakeLists.txt content."""

    # Build the Qt components line
    qt_comps_str = ' '.join(qt_components)

    # Build QML_FILES block
    qml_files_block = ""
    if qml_files:
        qml_files_block = "\n".join(f"        {f}" for f in qml_files)

    # Build local subdirectory lines
    local_subdir_lines = ""
    if local_subdirs:
        local_subdir_lines = "\n".join(f"add_subdirectory({sd})" for sd in local_subdirs)

    # Build extra link libs (non-DsQt, non-Qt)
    extra_link_block = ""
    if extra_link_libs:
        extra_link_block = "\n".join(f"        {lib}" for lib in extra_link_libs)

    # Build QML dependencies
    all_qml_deps = ["Dsqt::Core", "Dsqt::Bridge", "Dsqt::TouchEngine", "Dsqt::Waffles", "QtQuick"]
    for dep in extra_qml_deps:
        if dep not in all_qml_deps:
            all_qml_deps.append(dep)
    qml_deps_block = "\n".join(f"        {d}" for d in all_qml_deps)

    # Singleton line
    singleton_block = ""
    if has_singleton_fnt:
        singleton_block = '\n#make Fnt.qml a singleton\nset_source_files_properties(Fnt.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)\n'

    # FetchContent block for tomlplusplus
    fetchcontent_block = ""
    fetchcontent_include = ""
    toml_link_line = ""
    toml_include_line = ""
    if has_toml_fetchcontent and toml_fetchcontent_block:
        fetchcontent_include = "include(FetchContent)\n"
        fetchcontent_block = f"\n##Fetch toml++\n{toml_fetchcontent_block}\n"
        toml_link_line = "        tomlplusplus::tomlplusplus"
        toml_include_line = ""
    else:
        toml_link_line = "        PkgConfig::tomlplusplus"

    # Determine extra cpp files in qt_add_executable
    extra_cpp = ""
    if extra_cpp_files:
        extra_cpp = "\n".join(f"    {f}" for f in extra_cpp_files)
        extra_cpp = "\n" + extra_cpp

    lines = f"""cmake_minimum_required(VERSION 3.29)
{fetchcontent_include}
#project info
set(DS_APP_NAME {app_name})
set(DS_EXE_NAME app${{DS_APP_NAME}})
set(DS_PROJECT_NAME "{project_name}")
set(DS_PROJECT_DESC "{project_desc}")

# Read version from VERSION.txt (source of truth)
file(STRINGS "${{CMAKE_CURRENT_SOURCE_DIR}}/VERSION.txt" DS_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${{DS_VERSION_STRING}}" DS_VERSION_STRING)
set(DS_APP_VERSION "${{DS_VERSION_STRING}}.0")

#define the project
project(${{DS_APP_NAME}} VERSION ${{DS_VERSION_STRING}} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${{CMAKE_BINARY_DIR}}/DEPLOY/")
set(QT_FORCE_CMP0156_TO_VALUE NEW)
set(QT_QML_GENERATE_QMLLS_INI ON)
find_package(Qt6 6.9 REQUIRED COMPONENTS {qt_comps_str})
qt_standard_project_setup(REQUIRES 6.9)

# Find Dsqt from installed package (CMAKE_PREFIX_PATH set via CMakePresets.json)
find_package(Dsqt REQUIRED COMPONENTS Core Bridge Waffles TouchEngine)

message("DSQT is at ${{Dsqt_DIR}}")
"""

    if local_subdir_lines:
        lines += f"""
# Local project subdirectories
{local_subdir_lines}
"""

    lines += f"""
#set qml import path. This is used by Qt Creator to find the QML files.
set(QML_TMP_PATH
     "${{CMAKE_SOURCE_DIR}};${{CMAKE_SOURCE_DIR}}/qml;${{QML_IMPORT_PATH}}"
)
list(REMOVE_DUPLICATES QML_TMP_PATH)
set(QML_IMPORT_PATH
     "${{QML_TMP_PATH}}"
     CACHE PATH "QT creator qml import path" FORCE
)

#setup windows resource file
configure_file("cmake/app.rc.in" "app.rc")

qt_add_executable(${{DS_EXE_NAME}}
    main.cpp
    app.rc{extra_cpp}
)

target_link_libraries(${{DS_EXE_NAME}} PRIVATE Qt6::Quick)
qt_add_resources(${{DS_EXE_NAME}} "icon" FILES newds.ico)
{singleton_block}
# Get Dsqt paths from the installed package (set by DsqtConfig.cmake)
message(STATUS "DSQT_INSTALL_PREFIX: ${{DSQT_INSTALL_PREFIX}}")
message(STATUS "DSQT_QML_IMPORT_PATH: ${{DSQT_QML_IMPORT_PATH}}")

qt_add_qml_module(${{DS_EXE_NAME}}
    URI ${{DS_APP_NAME}}
    VERSION 1.0
    QML_FILES
{qml_files_block}

    IMPORT_PATH "${{DSQT_QML_IMPORT_PATH}}"
    DEPENDENCIES
{qml_deps_block}
        #RESOURCES cmake/app.rc.in
)



LIST(APPEND DIR_FOR_INSTALL "./settings/" "./data/")

#get the settings to show up in the IDE
file(GLOB_RECURSE DS_SETTINGS LIST_DIRECTORIES false ./settings/*)
LIST(APPEND DS_SETTINGS .qmlls.ini)
add_custom_target(ds-settings SOURCES ${{DS_SETTINGS}})

#get the data to show up in the IDE
file(GLOB_RECURSE DS_DATA LIST_DIRECTORIES false RELATIVE . ./data/*)
file(GLOB_RECURSE DS_DATA_TARGET LIST_DIRECTORIES false ./data/*)

qt_add_resources(${{DS_EXE_NAME}} data
    PREFIX "/"
    FILES ${{DS_DATA}}
)

add_custom_target(ds-data SOURCES ${{DS_DATA_TARGET}})
add_custom_target(aux SOURCES Readme.md)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
set_target_properties(${{DS_EXE_NAME}} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${{PROJECT_VERSION}}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${{PROJECT_VERSION_MAJOR}}.${{PROJECT_VERSION_MINOR}}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
{fetchcontent_block}
find_package(glm CONFIG REQUIRED)

target_include_directories(${{DS_EXE_NAME}} PRIVATE ${{Dsqt_INCLUDES}} ${{TouchEngineQt_INCLUDES}})

target_link_libraries(${{DS_EXE_NAME}}
    PRIVATE
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles

{extra_link_block}

        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::ShaderTools
        Qt6::GuiPrivate
        Qt6::Sql

{toml_link_line}
        glm::glm
)

message("HEADERS ${{Dsqt_INCLUDES}}")
include(GNUInstallDirs)

# TouchEngine SDK DLL location (from installed package, config-specific subdirectory)
set(TOUCHENGINE_DLL "${{DSQT_BIN_DIR}}/$<CONFIG>/TouchEngine.dll")

# Copy TouchEngine.dll next to the executable during build
if(WIN32)
    add_custom_command(TARGET ${{DS_EXE_NAME}} POST_BUILD
        COMMAND ${{CMAKE_COMMAND}} -E copy_if_different
            "${{TOUCHENGINE_DLL}}"
            $<TARGET_FILE_DIR:${{DS_EXE_NAME}}>
        COMMENT "Copying TouchEngine.dll to output directory"
    )
endif()

install(TARGETS ${{DS_EXE_NAME}}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${{CMAKE_INSTALL_LIBDIR}}
    RUNTIME DESTINATION ${{CMAKE_INSTALL_BINDIR}}
)

install(DIRECTORY "settings/" DESTINATION ${{CMAKE_INSTALL_BINDIR}}/settings OPTIONAL)
install(DIRECTORY "data/" DESTINATION ${{CMAKE_INSTALL_BINDIR}}/data OPTIONAL)

# Install TouchEngine.dll to the deploy directory
if(WIN32)
    install(FILES "${{DSQT_BIN_DIR}}/$<CONFIG>/TouchEngine.dll" DESTINATION ${{CMAKE_INSTALL_BINDIR}} OPTIONAL)
endif()

# Install vcpkg runtime dependencies
if(WIN32)
    file(GLOB VCPKG_RUNTIME_DLLS "${{CMAKE_BINARY_DIR}}/vcpkg_installed/x64-windows/bin/*.dll")
    if(VCPKG_RUNTIME_DLLS)
        install(FILES ${{VCPKG_RUNTIME_DLLS}} DESTINATION ${{CMAKE_INSTALL_BINDIR}})
    endif()
endif()

qt_generate_deploy_qml_app_script(
    TARGET ${{DS_EXE_NAME}}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)

install(SCRIPT ${{deploy_script}})
"""
    return lines


DSQT_INSTALL_SUFFIX = "$env{USERPROFILE}/Documents/DsQt"
VCPKG_TOOLCHAIN = "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"

# Common Qt install root directories to scan (Windows paths, checked via C:/ and /mnt/c/)
QT_SEARCH_ROOTS = ["C:/Qt"]


def detect_qt_installations():
    """Scan for installed Qt versions. Returns a sorted list of
    (version_string, cmake_path) tuples, e.g. ("6.10.1", "C:/Qt/6.10.1/msvc2022_64")."""
    found = []
    for root in QT_SEARCH_ROOTS:
        # Try both native Windows and WSL paths
        candidates = [root]
        if root.startswith("C:/"):
            candidates.append("/mnt/c/" + root[3:])

        for search_root in candidates:
            if not os.path.isdir(search_root):
                continue
            try:
                for entry in sorted(os.listdir(search_root)):
                    # Match Qt version directories like 6.10.1
                    if not re.match(r'^\d+\.\d+\.\d+$', entry):
                        continue
                    version_dir = os.path.join(search_root, entry)
                    if not os.path.isdir(version_dir):
                        continue
                    # Look for msvc2022_64 toolkit (primary target)
                    msvc_dir = os.path.join(version_dir, "msvc2022_64")
                    if os.path.isdir(msvc_dir):
                        # Always use C:/ style paths for CMake on Windows
                        cmake_path = f"C:/Qt/{entry}/msvc2022_64"
                        if (entry, cmake_path) not in found:
                            found.append((entry, cmake_path))
            except OSError:
                continue

    found.sort(key=lambda x: [int(p) for p in x[0].split('.')])
    return found


def extract_preset_qt_versions(presets_content):
    """Extract Qt version strings already referenced in an existing CMakePresets.json.
    Returns a set of version strings like {"6.10.0", "6.10.1"}."""
    versions = set()
    if not presets_content:
        return versions
    try:
        data = json.loads(presets_content)
    except json.JSONDecodeError:
        return versions

    for preset in data.get("configurePresets", []):
        name = preset.get("name", "")
        # Match hidden presets like "qt-6.10.1"
        m = re.match(r'^qt-(\d+\.\d+\.\d+)$', name)
        if m:
            versions.add(m.group(1))
            continue
        # Also check CMAKE_PREFIX_PATH for Qt version references
        prefix_path = preset.get("cacheVariables", {}).get("CMAKE_PREFIX_PATH", "")
        for ver_match in re.finditer(r'Qt/(\d+\.\d+\.\d+)/', prefix_path):
            versions.add(ver_match.group(1))

    return versions


def prompt_qt_version_selection(installed_versions, existing_versions):
    """Present an interactive checkbox UI for selecting Qt versions.

    Args:
        installed_versions: list of (version, cmake_path) from detect_qt_installations()
        existing_versions: set of version strings from extract_preset_qt_versions()

    Returns:
        list of (version, cmake_path) tuples the user selected.
    """
    installed_set = {v for v, _ in installed_versions}

    # Notify about versions in presets that aren't installed
    removed = existing_versions - installed_set
    for ver in sorted(removed):
        print(f"  NOTE: Qt {ver} is in existing presets but not installed - will be removed.")

    if not installed_versions:
        print("  WARNING: No Qt installations found under C:/Qt/")
        print("  You will need to manually edit CMakePresets.json with your Qt paths.")
        return []

    # Build the selection list
    # Existing versions = checked by default, new versions = unchecked
    print()
    print("  Select Qt versions to include in CMakePresets.json:")
    print("  (Enter numbers separated by spaces, or 'a' for all, or press Enter for defaults)")
    print()

    items = []
    for ver, cmake_path in installed_versions:
        is_existing = ver in existing_versions
        items.append((ver, cmake_path, is_existing))

    for i, (ver, cmake_path, is_existing) in enumerate(items):
        marker = "[x]" if is_existing else "[ ]"
        label = f"  {i + 1}. {marker} Qt {ver}  ({cmake_path})"
        if not is_existing:
            label += "  (new)"
        print(label)

    print()
    try:
        response = input("  Selection: ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        response = ""

    if response == 'a':
        return installed_versions

    if not response:
        # Use defaults (existing versions only, or all if none existed)
        if existing_versions:
            return [(v, p) for v, p, ex in items if ex]
        else:
            return installed_versions

    # Parse user selection
    selected = set()
    for token in response.split():
        try:
            idx = int(token) - 1
            if 0 <= idx < len(items):
                selected.add(idx)
        except ValueError:
            continue

    if not selected:
        # Fall back to defaults
        if existing_versions:
            return [(v, p) for v, p, ex in items if ex]
        else:
            return installed_versions

    return [(items[i][0], items[i][1]) for i in sorted(selected)]


def generate_cmake_presets(qt_versions):
    """Generate CMakePresets.json content for the given Qt versions.

    Args:
        qt_versions: list of (version_string, cmake_path) tuples
    """
    configure_presets = [
        {
            "name": "vcpkg-base",
            "hidden": True,
            "toolchainFile": VCPKG_TOOLCHAIN,
        }
    ]
    build_presets = []

    for ver, cmake_path in qt_versions:
        qt_preset_name = f"qt-{ver}"

        # Hidden Qt version preset
        configure_presets.append({
            "name": qt_preset_name,
            "hidden": True,
            "inherits": "vcpkg-base",
            "cacheVariables": {
                "CMAKE_PREFIX_PATH": f"{cmake_path};{DSQT_INSTALL_SUFFIX}"
            }
        })

        # VS2022 preset
        vs_name = f"vs2022-{ver}"
        configure_presets.append({
            "name": vs_name,
            "displayName": f"VS2022 x64 - Qt {ver}",
            "inherits": qt_preset_name,
            "generator": "Visual Studio 17 2022",
            "architecture": "x64",
            "binaryDir": f"${{sourceDir}}/build/{vs_name}"
        })

        # VS2022 build presets
        for config in ["Debug", "Release", "RelWithDebInfo"]:
            build_presets.append({
                "name": f"{vs_name}-{config.lower()}",
                "displayName": config,
                "configurePreset": vs_name,
                "configuration": config
            })

        # Ninja presets (debug, release, relwithdebinfo)
        for config, config_lower in [("Debug", "debug"), ("Release", "release"), ("RelWithDebInfo", "relwithdebinfo")]:
            ninja_name = f"ninja-{ver}-{config_lower}"
            configure_presets.append({
                "name": ninja_name,
                "displayName": f"Ninja {config} - Qt {ver}",
                "inherits": qt_preset_name,
                "generator": "Ninja",
                "binaryDir": f"${{sourceDir}}/build/{ninja_name}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": config,
                    "CMAKE_C_COMPILER": "cl.exe",
                    "CMAKE_CXX_COMPILER": "cl.exe"
                }
            })
            build_presets.append({
                "name": ninja_name,
                "configurePreset": ninja_name
            })

    data = {
        "version": 6,
        "configurePresets": configure_presets,
        "buildPresets": build_presets
    }
    return json.dumps(data, indent=2) + "\n"


def generate_vcpkg_json(app_name):
    """Generate vcpkg.json content."""
    return f"""{{\n  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "{app_name.lower()}",
  "version": "1.0.0",
  "builtin-baseline": "b02e341c927f16d991edbd915d8ea43eac52096c",
  "dependencies": [
    {{
      "name": "pkgconf",
      "version>=": "2.3.0"
    }},
    {{
      "name": "glm",
      "version>=": "1.0.1"
    }}
  ]
}}
"""


def generate_project_ci(app_name):
    """Generate project-ci.yml content."""
    return f"""name: {app_name}
qt_modules: "qtshadertools qtwebengine qtpdf qtmultimedia"
vcpkg_commit: "a7b6122f6b6504d16d96117336a0562693579933"
"""


# ── main.cpp patching ──────────────────────────────────────────────────────

def patch_main_cpp(content):
    """Patch main.cpp for library approach. Returns (new_content, changes_made)."""
    changes = []

    # Remove Q_INIT_RESOURCE calls (no longer needed with library approach)
    new_content = content
    for resource_name in ['data', 'keyboard']:
        pattern = rf'^\s*Q_INIT_RESOURCE\s*\(\s*{resource_name}\s*\)\s*;.*$'
        if re.search(pattern, new_content, re.MULTILINE):
            # Also remove the comment line immediately before if it exists
            new_content = re.sub(
                rf'^\s*//[^\n]*\n\s*Q_INIT_RESOURCE\s*\(\s*{resource_name}\s*\)\s*;[^\n]*\n',
                '', new_content, flags=re.MULTILINE
            )
            # Fallback: just remove the line itself if the above didn't match
            new_content = re.sub(
                rf'^\s*Q_INIT_RESOURCE\s*\(\s*{resource_name}\s*\)\s*;[^\n]*\n',
                '', new_content, flags=re.MULTILINE
            )
            changes.append(f"Removed Q_INIT_RESOURCE({resource_name})")

    # Add #include <QtQml/qqmlextensionplugin.h> if not present
    if 'qqmlextensionplugin.h' not in new_content:
        # Add after the last #include line before main()
        # Find a good insertion point - after the last DsQt include
        insert_marker = '#include <QQuickWindow>'
        if insert_marker in new_content:
            new_content = new_content.replace(
                insert_marker,
                insert_marker + '\n#include <QtQml/qqmlextensionplugin.h>',
                1
            )
        else:
            # Fallback: add before main()
            new_content = new_content.replace(
                'int main(',
                '#include <QtQml/qqmlextensionplugin.h>\n\nint main(',
                1
            )
        changes.append("Added #include <QtQml/qqmlextensionplugin.h>")

    # Add Q_IMPORT_QML_PLUGIN macros if not present
    if 'Q_IMPORT_QML_PLUGIN' not in new_content:
        plugin_lines = "\n".join(
            f"Q_IMPORT_QML_PLUGIN({name})" for name, _ in DSQT_PLUGIN_MACROS
        )
        plugin_block = f"\n// Import statically linked Dsqt QML plugins\n{plugin_lines}\n"

        # Insert after the GPU enablement extern "C" block, or before main()
        extern_c_end = new_content.find('#endif\n\nint main(')
        if extern_c_end < 0:
            # Try just before main
            new_content = new_content.replace(
                'int main(',
                plugin_block + '\nint main(',
                1
            )
        else:
            insert_pos = extern_c_end + len('#endif')
            new_content = new_content[:insert_pos] + '\n' + plugin_block + new_content[insert_pos:]

        changes.append("Added Q_IMPORT_QML_PLUGIN macros for Dsqt modules")

    return new_content, changes


# ── QML import rewriting ───────────────────────────────────────────────────

# Map of old QML import names to new modular names
QML_IMPORT_MAP = {
    "Dsqt": "Dsqt.Core",
    "WafflesUx": "Dsqt.Waffles",
    "TouchEngineQt": "Dsqt.TouchEngine",
}


def rewrite_qml_imports(project_dir, dry_run=False):
    """Scan all .qml files and rewrite old DsQt import names to new modular names.
    Returns list of (filepath, changes) tuples."""
    results = []
    for root, dirs, files in os.walk(project_dir):
        # Skip build directories
        basename = os.path.basename(root)
        if basename in ('build', 'Generated', '.git', 'node_modules'):
            dirs.clear()
            continue
        for filename in files:
            if not filename.endswith('.qml'):
                continue
            filepath = os.path.join(root, filename)
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

            new_content = content
            changes = []
            for old_import, new_import in QML_IMPORT_MAP.items():
                # Match "import OldName" but not "import OldName.Something" (already migrated)
                pattern = rf'^(\s*import\s+){re.escape(old_import)}(\s*)$'
                if re.search(pattern, new_content, re.MULTILINE):
                    new_content = re.sub(
                        pattern,
                        rf'\g<1>{new_import}\2',
                        new_content,
                        flags=re.MULTILINE
                    )
                    changes.append(f"import {old_import} -> import {new_import}")

            if changes:
                rel_path = os.path.relpath(filepath, project_dir)
                if not dry_run:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                results.append((rel_path, changes))

    return results


# ── .gitignore merging ─────────────────────────────────────────────────────

def merge_gitignore(existing_content, reference_content):
    """Merge reference .gitignore entries into existing, preserving existing entries."""
    existing_lines = set(existing_content.strip().splitlines())
    reference_lines = reference_content.strip().splitlines()
    new_lines = []
    for line in reference_lines:
        if line not in existing_lines:
            new_lines.append(line)

    if not new_lines:
        return existing_content, []

    result = existing_content.rstrip() + "\n\n# Added by DsQt migration\n"
    result += "\n".join(new_lines) + "\n"
    return result, new_lines


# ── Git backup ─────────────────────────────────────────────────────────────

def git_backup(project_dir):
    """Create a git backup branch with current state."""
    try:
        # Check if it's a git repo
        result = subprocess.run(
            ['git', 'rev-parse', '--is-inside-work-tree'],
            capture_output=True, text=True, cwd=project_dir
        )
        if result.returncode != 0:
            print("  Not a git repository, skipping backup.")
            return False

        # Get current branch
        result = subprocess.run(
            ['git', 'branch', '--show-current'],
            capture_output=True, text=True, cwd=project_dir
        )
        current_branch = result.stdout.strip()

        # Check for uncommitted changes
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            capture_output=True, text=True, cwd=project_dir
        )
        has_changes = bool(result.stdout.strip())

        if has_changes:
            backup_branch = BACKUP_BRANCH
            print(f"  Creating backup branch '{backup_branch}' with current state...")
            subprocess.run(['git', 'stash', 'push', '-m', 'pre-migration-backup'], cwd=project_dir, check=True)
            subprocess.run(['git', 'branch', '-f', backup_branch], cwd=project_dir, check=True)
            subprocess.run(['git', 'stash', 'pop'], cwd=project_dir, check=True)
            print(f"  Backup branch '{backup_branch}' created from current HEAD.")
        else:
            backup_branch = BACKUP_BRANCH
            subprocess.run(['git', 'branch', '-f', backup_branch], cwd=project_dir, check=True)
            print(f"  Backup branch '{backup_branch}' created at current HEAD (no uncommitted changes).")

        return True

    except subprocess.CalledProcessError as e:
        print(f"  Git backup failed: {e}")
        return False


BACKUP_BRANCH = "pre-migration-backup"


def reset_to_backup(project_dir):
    """Reset the project to the pre-migration backup branch."""
    project_dir = os.path.abspath(project_dir)

    print("=" * 60)
    print("DsQt Migration: Reset to backup")
    print("=" * 60)
    print(f"  Project: {project_dir}")
    print()

    # Check if it's a git repo
    result = subprocess.run(
        ['git', 'rev-parse', '--is-inside-work-tree'],
        capture_output=True, text=True, cwd=project_dir
    )
    if result.returncode != 0:
        print("ERROR: Not a git repository.")
        sys.exit(1)

    # Check if backup branch exists
    result = subprocess.run(
        ['git', 'rev-parse', '--verify', BACKUP_BRANCH],
        capture_output=True, text=True, cwd=project_dir
    )
    if result.returncode != 0:
        print(f"ERROR: Backup branch '{BACKUP_BRANCH}' not found.")
        print("  No migration backup exists to reset to.")
        sys.exit(1)

    # Show what the backup branch points to
    result = subprocess.run(
        ['git', 'log', '--oneline', '-1', BACKUP_BRANCH],
        capture_output=True, text=True, cwd=project_dir
    )
    print(f"  Backup branch: {result.stdout.strip()}")

    # Show current branch
    result = subprocess.run(
        ['git', 'branch', '--show-current'],
        capture_output=True, text=True, cwd=project_dir
    )
    current_branch = result.stdout.strip()
    print(f"  Current branch: {current_branch}")
    print()

    # Warn about uncommitted changes
    result = subprocess.run(
        ['git', 'status', '--porcelain'],
        capture_output=True, text=True, cwd=project_dir
    )
    if result.stdout.strip():
        print("  WARNING: You have uncommitted changes that will be discarded:")
        for line in result.stdout.strip().splitlines()[:10]:
            print(f"    {line}")
        if len(result.stdout.strip().splitlines()) > 10:
            print(f"    ... and {len(result.stdout.strip().splitlines()) - 10} more")
        print()

    try:
        response = input("  Reset working tree to pre-migration state? [y/N]: ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        response = 'n'

    if response != 'y':
        print("  Aborted.")
        return

    # Reset: restore all tracked files to the backup branch state
    # and remove any untracked files that were added by the migration
    try:
        # Restore tracked files from the backup branch
        subprocess.run(
            ['git', 'checkout', BACKUP_BRANCH, '--', '.'],
            cwd=project_dir, check=True
        )

        # Find files that exist now but don't exist in the backup branch
        # (i.e., files added by the migration)
        result = subprocess.run(
            ['git', 'diff', '--name-only', '--diff-filter=A', BACKUP_BRANCH, 'HEAD'],
            capture_output=True, text=True, cwd=project_dir
        )
        added_files = [f for f in result.stdout.strip().splitlines() if f]

        # Also check for untracked files that match known migration outputs
        migration_files = ['CMakePresets.json', 'vcpkg.json', 'VERSION.txt', 'project-ci.yml']
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            capture_output=True, text=True, cwd=project_dir
        )
        for line in result.stdout.strip().splitlines():
            status = line[:2].strip()
            filepath = line[3:]
            if status == '??' and filepath in migration_files:
                full_path = os.path.join(project_dir, filepath)
                if os.path.isfile(full_path):
                    os.remove(full_path)
                    print(f"  Removed: {filepath} (added by migration)")

        # Unstage everything so the working tree is clean but uncommitted
        subprocess.run(
            ['git', 'reset', 'HEAD', '.'],
            capture_output=True, cwd=project_dir, check=True
        )

        print()
        print("  Reset complete. Working tree restored to pre-migration state.")
        print(f"  The backup branch '{BACKUP_BRANCH}' is still available.")

    except subprocess.CalledProcessError as e:
        print(f"  ERROR: Reset failed: {e}")
        sys.exit(1)


# ── Main migration ─────────────────────────────────────────────────────────

def migrate(project_dir, dsqt_root, dry_run=False, no_backup=False):
    """Run the full migration."""
    project_dir = os.path.abspath(project_dir)
    dsqt_root = os.path.abspath(dsqt_root)

    cloner_source = os.path.join(dsqt_root, "Examples", "ClonerSource")

    print("=" * 60)
    print("DsQt Migration: Subdirectory -> Library")
    print("=" * 60)
    print(f"  Project:      {project_dir}")
    print(f"  DsQt root:    {dsqt_root}")
    print(f"  ClonerSource: {cloner_source}")
    print(f"  Dry run:      {dry_run}")
    print()

    # Validate paths
    cmake_path = os.path.join(project_dir, "CMakeLists.txt")
    if not os.path.isfile(cmake_path):
        print(f"ERROR: CMakeLists.txt not found in {project_dir}")
        sys.exit(1)

    if not os.path.isdir(cloner_source):
        print(f"ERROR: ClonerSource not found at {cloner_source}")
        sys.exit(1)

    # ── Step 1: Parse existing CMakeLists.txt ───────────────────────────
    print("Step 1: Analyzing existing CMakeLists.txt...")

    with open(cmake_path, 'r') as f:
        cmake_content = f.read()

    app_name = parse_cmake_set(cmake_content, 'DS_APP_NAME') or 'MyApp'
    exe_name = parse_cmake_set(cmake_content, 'DS_EXE_NAME') or f'app{app_name}'
    project_name = parse_cmake_set(cmake_content, 'DS_PROJECT_NAME') or app_name
    project_desc = parse_cmake_set(cmake_content, 'DS_PROJECT_DESC') or 'A DsQt Application'
    app_version = parse_cmake_set(cmake_content, 'DS_APP_VERSION') or '1.0.0.0'

    qt_components = parse_qt_components(cmake_content)
    qml_files = parse_qml_files_from_module(cmake_content)
    qml_deps = parse_qml_dependencies(cmake_content)
    link_libs = parse_link_libraries(cmake_content)
    local_subdirs = find_local_subdirectories(project_dir, cmake_content)
    env_var = detect_dsqt_subdirectory(cmake_content)
    has_dll_subdir = detect_dll_subdir(cmake_content)

    # Separate project-specific link libs from DsQt internal ones
    extra_link_libs = []
    qt_link_libs = set()
    for lib in link_libs:
        if lib in DSQT_INTERNAL_LINK_TARGETS:
            continue
        if lib.startswith('Qt6::'):
            qt_link_libs.add(lib)
            continue
        if lib in ('glm', 'glm::glm'):
            continue
        if lib in ('PkgConfig::tomlplusplus', 'tomlplusplus::tomlplusplus'):
            continue
        extra_link_libs.append(lib)

    # Separate project-specific QML deps from DsQt ones
    extra_qml_deps = []
    for dep in qml_deps:
        mapped = DSQT_QML_DEPENDENCY_MAP.get(dep)
        if mapped:
            continue  # will be added by the template
        if dep == 'QtQuick':
            continue  # will be added by the template
        extra_qml_deps.append(dep)

    # Check for tomlplusplus FetchContent
    has_toml_fc = has_fetchcontent(cmake_content, 'tomlplusplus')
    toml_fc_block = extract_fetchcontent_block(cmake_content, 'tomlplusplus') if has_toml_fc else None

    # Check for Fnt.qml singleton
    has_singleton_fnt = 'Fnt.qml' in cmake_content and 'SINGLETON' in cmake_content

    # Check for WebEngine initialization
    has_webengine = 'WebEngineQuick' in cmake_content

    # Check for extra .cpp files in qt_add_executable beyond main.cpp
    exec_blocks = extract_block(cmake_content, 'qt_add_executable')
    extra_cpp_files = []
    for block in exec_blocks:
        cpp_files = re.findall(r'\b(\w+\.(?:cpp|h))\b', block)
        for f in cpp_files:
            if f not in ('main.cpp', 'app.rc'):
                extra_cpp_files.append(f)

    # Extra Qt link libs that aren't in the standard set
    standard_qt_libs = {
        'Qt6::Concurrent', 'Qt6::Core', 'Qt6::Gui', 'Qt6::Multimedia',
        'Qt6::Quick', 'Qt6::QuickControls2', 'Qt6::ShaderTools',
        'Qt6::GuiPrivate', 'Qt6::Sql'
    }
    extra_qt_in_link = [lib for lib in qt_link_libs if lib not in standard_qt_libs]
    # Add extra Qt libs to the project-specific list
    extra_link_libs.extend(extra_qt_in_link)

    print(f"  App name:          {app_name}")
    print(f"  Exe name:          {exe_name}")
    print(f"  Project name:      {project_name}")
    print(f"  Version:           {app_version}")
    print(f"  Qt components:     {', '.join(qt_components)}")
    print(f"  QML files:         {len(qml_files)} files")
    print(f"  Local subdirs:     {', '.join(local_subdirs) if local_subdirs else '(none)'}")
    print(f"  Extra link libs:   {', '.join(extra_link_libs) if extra_link_libs else '(none)'}")
    print(f"  Extra QML deps:    {', '.join(extra_qml_deps) if extra_qml_deps else '(none)'}")
    print(f"  DsQt env var:      {env_var or '(not found)'}")
    print(f"  DLL subdir:        {'yes' if has_dll_subdir else 'no'}")
    print(f"  toml++ via FC:     {'yes' if has_toml_fc else 'no'}")
    print(f"  Fnt.qml singleton: {'yes' if has_singleton_fnt else 'no'}")
    print()

    if not env_var:
        print("WARNING: Could not detect DsQt subdirectory pattern.")
        print("         This project may already be using the library approach,")
        print("         or uses a different integration method.")
        if not dry_run:
            try:
                response = input("Continue anyway? [y/N]: ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                response = 'n'
            if response != 'y':
                print("Aborted.")
                sys.exit(0)

    # ── Step 2: Git backup ──────────────────────────────────────────────
    if not no_backup and not dry_run:
        print("Step 2: Git backup...")
        response = input("  Create a git backup branch before migrating? [Y/n]: ").strip().lower()
        if response != 'n':
            git_backup(project_dir)
        print()
    else:
        print("Step 2: Git backup... (skipped)")
        print()

    # ── Step 3: Generate new CMakeLists.txt ─────────────────────────────
    print("Step 3: Generating new CMakeLists.txt...")

    new_cmake = generate_cmakelists(
        app_name=app_name,
        exe_name=exe_name,
        project_name=project_name,
        project_desc=project_desc,
        qt_components=qt_components,
        qml_files=qml_files,
        local_subdirs=local_subdirs,
        extra_link_libs=extra_link_libs,
        extra_qml_deps=extra_qml_deps,
        has_toml_fetchcontent=has_toml_fc,
        toml_fetchcontent_block=toml_fc_block,
        has_singleton_fnt=has_singleton_fnt,
        has_webengine_init=has_webengine,
        extra_cpp_files=extra_cpp_files,
    )

    if dry_run:
        print("  [DRY RUN] Would write CMakeLists.txt")
    else:
        with open(cmake_path, 'w') as f:
            f.write(new_cmake)
        print("  Written: CMakeLists.txt")

    if has_dll_subdir:
        print()
        print("  NOTE: DS_QT_DLLS_SUBDIR feature has been removed for simplicity.")
        print("        The qt.manifest.in-based DLL subdirectory approach is no longer used.")
        print("        Qt DLLs will be installed alongside the executable as usual.")

    print()

    # ── Step 4: Generate or update CMakePresets.json ───────────────────────
    presets_path = os.path.join(project_dir, "CMakePresets.json")
    print("Step 4: CMakePresets.json...")

    # Detect installed Qt versions
    installed_qt = detect_qt_installations()
    print(f"  Detected {len(installed_qt)} Qt installation(s).")

    # Check what versions are already in presets (if file exists)
    existing_presets_content = None
    existing_qt_versions = set()
    if os.path.isfile(presets_path):
        with open(presets_path, 'r') as f:
            existing_presets_content = f.read()
        existing_qt_versions = extract_preset_qt_versions(existing_presets_content)

    # Prompt user to select Qt versions (unless dry run or non-interactive)
    if dry_run:
        # In dry run, just show what would happen
        selected_qt = installed_qt
        print(f"  [DRY RUN] Would prompt for Qt version selection from:")
        for ver, path in installed_qt:
            in_existing = ver in existing_qt_versions
            print(f"    {'[x]' if in_existing else '[ ]'} Qt {ver}  ({path}){'  (new)' if not in_existing else ''}")
        removed = existing_qt_versions - {v for v, _ in installed_qt}
        for ver in sorted(removed):
            print(f"    NOTE: Qt {ver} no longer installed - would be removed")
        print(f"  [DRY RUN] Would {'update' if existing_presets_content else 'create'} CMakePresets.json")
    else:
        selected_qt = prompt_qt_version_selection(installed_qt, existing_qt_versions)
        if not selected_qt:
            print("  No Qt versions selected, skipping CMakePresets.json generation.")
        else:
            presets_content = generate_cmake_presets(selected_qt)
            with open(presets_path, 'w') as f:
                f.write(presets_content)
            action = "Updated" if existing_presets_content else "Created"
            selected_names = ', '.join(v for v, _ in selected_qt)
            print(f"  {action}: CMakePresets.json with Qt {selected_names}")
    print()

    # ── Step 5: Generate vcpkg.json ─────────────────────────────────────
    vcpkg_path = os.path.join(project_dir, "vcpkg.json")
    print("Step 5: vcpkg.json...")
    if os.path.isfile(vcpkg_path):
        print("  Already exists, skipping. Ensure it includes 'glm' dependency.")
    else:
        vcpkg_content = generate_vcpkg_json(app_name)
        if dry_run:
            print("  [DRY RUN] Would create vcpkg.json")
        else:
            with open(vcpkg_path, 'w') as f:
                f.write(vcpkg_content)
            print("  Created: vcpkg.json")
    print()

    # ── Step 6: Create VERSION.txt ──────────────────────────────────────
    version_path = os.path.join(project_dir, "VERSION.txt")
    print("Step 6: VERSION.txt...")
    if os.path.isfile(version_path):
        print(f"  Already exists, skipping.")
    else:
        # Extract major.minor.patch from app_version (strip trailing .0 if it's x.y.z.0)
        version = app_version
        parts = version.split('.')
        if len(parts) == 4 and parts[3] == '0':
            version = '.'.join(parts[:3])
        if dry_run:
            print(f"  [DRY RUN] Would create VERSION.txt with: {version}")
        else:
            with open(version_path, 'w') as f:
                f.write(version + '\n')
            print(f"  Created: VERSION.txt ({version})")
    print()

    # ── Step 7: Update main.cpp ─────────────────────────────────────────
    main_cpp_path = os.path.join(project_dir, "main.cpp")
    print("Step 7: Updating main.cpp...")
    if os.path.isfile(main_cpp_path):
        with open(main_cpp_path, 'r') as f:
            main_content = f.read()
        new_main, main_changes = patch_main_cpp(main_content)
        if main_changes:
            if dry_run:
                print("  [DRY RUN] Would apply these changes:")
                for change in main_changes:
                    print(f"    - {change}")
            else:
                with open(main_cpp_path, 'w') as f:
                    f.write(new_main)
                for change in main_changes:
                    print(f"  - {change}")
        else:
            print("  No changes needed (already up to date).")
    else:
        print("  WARNING: main.cpp not found!")
    print()

    # ── Step 8: Rewrite QML imports ────────────────────────────────────
    print("Step 8: Rewriting QML imports...")
    qml_results = rewrite_qml_imports(project_dir, dry_run=dry_run)
    if qml_results:
        for rel_path, changes in qml_results:
            prefix = "[DRY RUN] " if dry_run else ""
            for change in changes:
                print(f"  {prefix}{rel_path}: {change}")
    else:
        print("  No QML import changes needed.")
    print()

    # ── Step 9: Copy scaffolding files ──────────────────────────────────
    print("Step 9: Scaffolding files from ClonerSource...")

    files_to_copy = [
        ("cmake/app.rc.in", "cmake/app.rc.in"),
        ("resource1.h", "resource1.h"),
    ]

    for src_rel, dst_rel in files_to_copy:
        src = os.path.join(cloner_source, src_rel)
        dst = os.path.join(project_dir, dst_rel)
        if os.path.isfile(dst):
            print(f"  {dst_rel}: already exists, skipping.")
        elif os.path.isfile(src):
            if dry_run:
                print(f"  [DRY RUN] Would copy {dst_rel}")
            else:
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(src, dst)
                print(f"  Copied: {dst_rel}")
        else:
            print(f"  WARNING: Source not found: {src}")

    # project-ci.yml
    ci_path = os.path.join(project_dir, "project-ci.yml")
    if not os.path.isfile(ci_path):
        ci_content = generate_project_ci(app_name)
        if dry_run:
            print(f"  [DRY RUN] Would create project-ci.yml")
        else:
            with open(ci_path, 'w') as f:
                f.write(ci_content)
            print(f"  Created: project-ci.yml")
    else:
        print(f"  project-ci.yml: already exists, skipping.")

    # .gitignore merge
    gitignore_path = os.path.join(project_dir, ".gitignore")
    ref_gitignore_path = os.path.join(cloner_source, ".gitignore")
    if os.path.isfile(gitignore_path) and os.path.isfile(ref_gitignore_path):
        with open(gitignore_path, 'r') as f:
            existing_gi = f.read()
        with open(ref_gitignore_path, 'r') as f:
            ref_gi = f.read()
        merged_gi, new_gi_lines = merge_gitignore(existing_gi, ref_gi)
        if new_gi_lines:
            if dry_run:
                print(f"  [DRY RUN] Would append {len(new_gi_lines)} lines to .gitignore")
            else:
                with open(gitignore_path, 'w') as f:
                    f.write(merged_gi)
                print(f"  .gitignore: appended {len(new_gi_lines)} new entries.")
        else:
            print(f"  .gitignore: already up to date.")
    elif not os.path.isfile(gitignore_path) and os.path.isfile(ref_gitignore_path):
        if dry_run:
            print(f"  [DRY RUN] Would copy .gitignore")
        else:
            shutil.copy2(ref_gitignore_path, gitignore_path)
            print(f"  Copied: .gitignore")

    print()

    # ── Summary ─────────────────────────────────────────────────────────
    print("=" * 60)
    if dry_run:
        print("DRY RUN COMPLETE - no files were modified.")
    else:
        print("MIGRATION COMPLETE")
    print("=" * 60)
    print()
    print("Next steps:")
    print("  1. Ensure DsQt Library is built and installed:")
    print("     Open DsQt/Library/CMakeLists.txt in Qt Creator, build, then install.")
    print(f"     (Installs to %USERPROFILE%\\Documents\\DsQt)")
    print()
    print("  2. Ensure VCPKG_ROOT environment variable is set.")
    print()
    print("  3. Delete your existing build/ directory and re-configure:")
    print(f"     cd {project_dir}")
    print("     cmake --preset vs2022-6.10.1")
    print()
    if env_var:
        print(f"  4. The {env_var} environment variable is no longer needed")
        print("     by this project. You can remove it once all projects are migrated.")
        print()
    print("  Review the generated CMakeLists.txt carefully for any project-specific")
    print("  customizations that may need manual adjustment.")
    print()


# ── CLI ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Migrate a DsQt project from subdirectory to library approach.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python migrate_to_library.py C:\\dev\\MyProject
  python migrate_to_library.py C:\\dev\\MyProject --dry-run
  python migrate_to_library.py C:\\dev\\MyProject --dsqt-root C:\\dev\\DsQt --no-backup
  python migrate_to_library.py C:\\dev\\MyProject --reset
        """
    )
    parser.add_argument(
        'project_dir',
        help='Path to the project directory to migrate'
    )
    parser.add_argument(
        '--dsqt-root',
        help='Path to DsQt repo root (default: derived from script location)',
        default=None
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would change without modifying files'
    )
    parser.add_argument(
        '--no-backup',
        action='store_true',
        help='Skip the git backup prompt'
    )
    parser.add_argument(
        '--reset',
        action='store_true',
        help='Reset the project to the pre-migration backup branch'
    )

    args = parser.parse_args()

    # Resolve DsQt root
    if args.dsqt_root:
        dsqt_root = args.dsqt_root
    else:
        # Derive from script location: Tools/migrate_to_library.py -> ../../
        script_dir = os.path.dirname(os.path.abspath(__file__))
        dsqt_root = os.path.dirname(script_dir)

    if not os.path.isdir(dsqt_root):
        print(f"ERROR: DsQt root not found at {dsqt_root}")
        sys.exit(1)

    if not os.path.isdir(args.project_dir):
        print(f"ERROR: Project directory not found: {args.project_dir}")
        sys.exit(1)

    if args.reset:
        reset_to_backup(args.project_dir)
    else:
        migrate(args.project_dir, dsqt_root, dry_run=args.dry_run, no_backup=args.no_backup)


if __name__ == '__main__':
    main()
