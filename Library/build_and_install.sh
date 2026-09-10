#!/usr/bin/env bash
#
# build_and_install.sh — macOS / iOS counterpart to build_and_install.bat
#
# Configures, builds (Debug + Release) and installs the DsQt library using the
# macos / ios-device / ios-simulator presets from CMakePresets.json.
#
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------- colours ---
if [[ -t 1 ]]; then
    C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YEL=$'\033[33m'
    C_CYN=$'\033[36m'; C_OFF=$'\033[0m'
else
    C_RED=''; C_GRN=''; C_YEL=''; C_CYN=''; C_OFF=''
fi
header() { printf '%s=== %s ===%s\n' "$C_CYN" "$1" "$C_OFF"; }
info()   { printf '%s%s%s\n' "$C_YEL" "$1" "$C_OFF"; }
ok()     { printf '%s%s%s\n' "$C_GRN" "$1" "$C_OFF"; }
die()    { printf '%s%s%s\n' "$C_RED" "$1" "$C_OFF" >&2; exit "${2:-1}"; }

usage() {
cat <<'USAGE'

Usage: ./build_and_install.sh [preset] [-test] [-tools] [-no-configure] [-no-install] [-clean]
                              [-hard-clean] [-rebuild] [-hard-rebuild] [-qt <ver|path>]
                              [-team <id>] [-no-private-reinject]

  preset               : CMake configure preset (default: macos)
                         one of: macos | ios-device | ios-simulator
  -test                : Enable and run unit tests after the Debug build (macos only)
  -tools               : Build and install ProjectCloner + ClonerSource template (macos only)
  -no-configure        : Skip the CMake configure step (for fast incremental builds)
  -no-install          : Skip the install steps entirely
  -clean               : Run cmake --build clean targets and exit
  -hard-clean          : Delete the entire build folder and exit
  -rebuild             : Clean then build (cmake clean + full build)
  -hard-rebuild        : Delete build folder then configure + build + install from scratch
  -qt <ver or path>    : Qt version (e.g. 6.10.2) or full path to the host/target Qt dir.
                         A bare version resolves to <QtRoot>/<ver>/macos for the macos preset
                         and <QtRoot>/<ver>/ios for the iOS presets, probing ~/Qt, /opt/Qt
                         and /Applications/Qt.
  -team <id>           : Apple Development Team ID, for iOS code signing
  -no-private-reinject : Disable private-header touch re-injection (falls back to
                         QCoreApplication::sendEvent; private reinject is ON by default)
  -h / --help          : Print usage info and exit

USAGE
}

# ------------------------------------------------------------------ args ---
PRESET=""
RUN_TESTS=0
BUILD_TOOLS=0
SKIP_CONFIGURE=0
SKIP_INSTALL=0
DO_CLEAN=0
DO_HARD_CLEAN=0
DO_REBUILD=0
DO_HARD_REBUILD=0
NO_PRIVATE_REINJECT=0
QT_ARG=""
TEAM_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -test|--test)                 RUN_TESTS=1; shift ;;
        -tools|--tools)               BUILD_TOOLS=1; shift ;;
        -no-configure|--no-configure) SKIP_CONFIGURE=1; shift ;;
        -no-install|--no-install)     SKIP_INSTALL=1; shift ;;
        -clean|--clean)               DO_CLEAN=1; shift ;;
        -hard-clean|--hard-clean)     DO_HARD_CLEAN=1; shift ;;
        -rebuild|--rebuild)           DO_REBUILD=1; shift ;;
        -hard-rebuild|--hard-rebuild) DO_HARD_REBUILD=1; shift ;;
        -no-private-reinject|--no-private-reinject) NO_PRIVATE_REINJECT=1; shift ;;
        -qt|--qt)
            [[ $# -ge 2 ]] || die "-qt requires an argument"
            QT_ARG="$2"; shift 2 ;;
        -team|--team)
            [[ $# -ge 2 ]] || die "-team requires an argument"
            TEAM_ID="$2"; shift 2 ;;
        -h|--help)                    usage; exit 0 ;;
        -*)                           usage; die "Unknown option: $1" ;;
        *)
            [[ -z "$PRESET" ]] || die "Multiple presets given: '$PRESET' and '$1'"
            PRESET="$1"; shift ;;
    esac
done

PRESET="${PRESET:-macos}"
case "$PRESET" in
    macos|ios-device|ios-simulator) ;;
    *) die "Unknown preset '$PRESET'. Expected: macos | ios-device | ios-simulator" ;;
esac

IS_IOS=0
if [[ "$PRESET" == ios-* ]]; then IS_IOS=1; fi

BUILD_DIR="build/$PRESET"

# --------------------------------------------------------- sanity checks ---
[[ "$(uname -s)" == "Darwin" ]] \
    || die "This script targets macOS/iOS and must run on macOS. Use build_and_install.bat on Windows."

if (( IS_IOS )); then
    if (( RUN_TESTS )); then die "-test is not supported for the iOS presets: ctest cannot run the suite without a simulator or device harness, which this repo does not provide."; fi
    if (( BUILD_TOOLS )); then die "-tools is not supported for the iOS presets: ProjectCloner is a desktop application."; fi
fi

command -v cmake >/dev/null 2>&1 || die "cmake not found on PATH."

header "Checking Xcode toolchain"
command -v xcode-select >/dev/null 2>&1 || die "xcode-select not found. Install Xcode from the App Store."
XCODE_PATH="$(xcode-select -print-path 2>/dev/null || true)"
[[ -n "$XCODE_PATH" && -d "$XCODE_PATH" ]] \
    || die "No Xcode toolchain selected. Run: sudo xcode-select --switch /Applications/Xcode.app"
if [[ "$XCODE_PATH" == */CommandLineTools ]]; then
    die "Only the Command Line Tools are selected, which cannot build iOS bundles. Run: sudo xcode-select --switch /Applications/Xcode.app"
fi
info "    Xcode: $XCODE_PATH"

# ------------------------------------------------------------ vcpkg prep ---
[[ -n "${VCPKG_ROOT:-}" ]] || die "VCPKG_ROOT is not set. Please make sure vcpkg is properly installed."
[[ -x "$VCPKG_ROOT/vcpkg" ]] || die "vcpkg executable not found in VCPKG_ROOT: $VCPKG_ROOT"

header "Updating vcpkg"
VCPKG_NEEDS_BOOTSTRAP=0
if git -C "$VCPKG_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
    HEAD_BEFORE="$(git -C "$VCPKG_ROOT" rev-parse HEAD 2>/dev/null || echo none)"
    if git -C "$VCPKG_ROOT" fetch -q 2>/dev/null && git -C "$VCPKG_ROOT" pull --ff-only -q 2>/dev/null; then
        ok "vcpkg is up to date."
    else
        info "WARNING: git update failed for vcpkg - build may fail if the baseline is missing."
    fi
    HEAD_AFTER="$(git -C "$VCPKG_ROOT" rev-parse HEAD 2>/dev/null || echo none)"
    if [[ "$HEAD_BEFORE" != "$HEAD_AFTER" ]]; then VCPKG_NEEDS_BOOTSTRAP=1; fi
else
    info "WARNING: $VCPKG_ROOT is not a git checkout - skipping update."
fi

# A pull can bring in a scripts/vcpkg-tools.json newer than the installed binary
# understands ("document schema version N is not supported"), which then cascades
# into a missing Ninja / CMAKE_MAKE_PROGRAM.
if [[ "$VCPKG_ROOT/scripts/vcpkg-tools.json" -nt "$VCPKG_ROOT/vcpkg" ]]; then
    VCPKG_NEEDS_BOOTSTRAP=1
fi
if (( VCPKG_NEEDS_BOOTSTRAP )); then
    info "vcpkg sources are newer than the vcpkg binary - re-bootstrapping..."
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics || die "bootstrap-vcpkg.sh failed - cannot continue."
    ok "vcpkg re-bootstrapped."
fi

# --------------------------------------------------------------- Qt path ---
# The iOS presets read $QT_IOS_ROOT to locate qt.toolchain.cmake; the macos
# preset takes CMAKE_PREFIX_PATH on the command line, as the .bat does.
QT_PATH=""
if [[ -n "$QT_ARG" ]]; then
    if [[ "$QT_ARG" == */* ]]; then
        QT_PATH="$QT_ARG"
    else
        QT_SUBDIR="macos"
        if (( IS_IOS )); then QT_SUBDIR="ios"; fi
        for root in "$HOME/Qt" "/opt/Qt" "/Applications/Qt"; do
            if [[ -d "$root/$QT_ARG/$QT_SUBDIR" ]]; then
                QT_PATH="$root/$QT_ARG/$QT_SUBDIR"
                break
            fi
        done
        [[ -n "$QT_PATH" ]] || die "Qt $QT_ARG ($QT_SUBDIR) not found under ~/Qt, /opt/Qt or /Applications/Qt. Pass a full path instead: -qt /path/to/Qt/$QT_ARG/$QT_SUBDIR"
    fi
    [[ -d "$QT_PATH" ]] || die "Qt path not found: $QT_PATH"
    info "    Qt: $QT_PATH"
fi

if (( IS_IOS )); then
    if [[ -n "$QT_PATH" ]]; then
        export QT_IOS_ROOT="$QT_PATH"
    elif [[ -z "${QT_IOS_ROOT:-}" ]]; then
        die "The $PRESET preset needs a Qt for iOS location. Pass -qt <version|path>, or export QT_IOS_ROOT=<Qt-dir>/<version>/ios"
    fi
    [[ -f "$QT_IOS_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" ]] \
        || die "qt.toolchain.cmake not found under QT_IOS_ROOT ($QT_IOS_ROOT). Expected: \$QT_IOS_ROOT/lib/cmake/Qt6/qt.toolchain.cmake"
    info "    QT_IOS_ROOT: $QT_IOS_ROOT"
fi

# ------------------------------------------------------- clean / rebuild ---
if (( DO_HARD_REBUILD || DO_HARD_CLEAN )); then
    header "Removing $BUILD_DIR"
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        ok "$BUILD_DIR removed."
    else
        info "$BUILD_DIR does not exist."
    fi
    if (( DO_HARD_CLEAN )); then exit 0; fi
fi

if (( DO_REBUILD || DO_CLEAN )); then
    header "Cleaning $PRESET"
    cmake --build --preset "$PRESET-debug"   --target clean || true
    cmake --build --preset "$PRESET-release" --target clean || true
    ok "Clean complete."
    if (( DO_CLEAN )); then exit 0; fi
fi

# ------------------------------------------------------------------ time ---
now() { date +%s; }
fmt() { printf '%dm %ds' $(( $1 / 60 )) $(( $1 % 60 )); }
OVERALL_START=$(now)
CONFIGURE_SECS=0
BUILD_DEBUG_SECS=0
BUILD_RELEASE_SECS=0
INSTALL_SECS=0
TEST_SECS=0

# ------------------------------------------------------------- configure ---
CONFIGURE_ARGS=()
if (( RUN_TESTS )); then CONFIGURE_ARGS+=(-DDSQT_BUILD_TESTS=ON); fi
if (( BUILD_TOOLS )); then CONFIGURE_ARGS+=(-DDSQT_BUILD_TOOLS=ON); fi
if (( NO_PRIVATE_REINJECT )); then CONFIGURE_ARGS+=(-DDSQT_TOUCH_NO_PRIVATE_REINJECT=ON); fi
if [[ -n "$QT_PATH" ]] && (( ! IS_IOS )); then
    CONFIGURE_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PATH" -DQt6_DIR="$QT_PATH/lib/cmake/Qt6")
fi
if [[ -n "$TEAM_ID" ]]; then
    CONFIGURE_ARGS+=(-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID")
fi

if (( SKIP_CONFIGURE )); then
    header "Skipping Configure (-no-configure)"
else
    header "Configuring DsQt Library ($PRESET)"
    if (( RUN_TESTS )); then info "    Tests:            ENABLED"; fi
    if (( BUILD_TOOLS )); then info "    Tools:            ENABLED"; fi
    if (( NO_PRIVATE_REINJECT )); then info "    Private reinject: DISABLED"; fi
    if [[ -n "$TEAM_ID" ]]; then info "    Development team: $TEAM_ID"; fi
    t0=$(now)
    cmake --preset "$PRESET" ${CONFIGURE_ARGS[@]+"${CONFIGURE_ARGS[@]}"} || die "Configuration failed."
    CONFIGURE_SECS=$(( $(now) - t0 ))
fi

# ----------------------------------------------------------------- build ---
MARKER="$BUILD_DIR/.build_marker"
changed_since_marker() {
    # Any static archive newer than the marker means the build produced something.
    [[ -n "$(find "$BUILD_DIR" -name '*.a' -newer "$MARKER" -print -quit 2>/dev/null)" ]]
}

echo
header "Building Debug"
mkdir -p "$BUILD_DIR"
: > "$MARKER"
t0=$(now)
cmake --build --preset "$PRESET-debug" || die "Debug build failed."
BUILD_DEBUG_SECS=$(( $(now) - t0 ))
DEBUG_NOOP=0
changed_since_marker || DEBUG_NOOP=1

if (( RUN_TESTS )); then
    echo
    header "Running Tests"
    t0=$(now)
    set +e
    ctest --test-dir "$BUILD_DIR" --build-config Debug -V \
          --output-junit "$BUILD_DIR/test-results.xml"
    TEST_EXIT=$?
    set -e
    TEST_SECS=$(( $(now) - t0 ))
    (( TEST_EXIT == 0 )) || info "WARNING: Some tests failed (exit code $TEST_EXIT). Continuing with build..."
fi

echo
header "Building Release"
: > "$MARKER"
t0=$(now)
cmake --build --preset "$PRESET-release" || die "Release build failed."
BUILD_RELEASE_SECS=$(( $(now) - t0 ))
RELEASE_NOOP=0
changed_since_marker || RELEASE_NOOP=1

# --------------------------------------------------------------- install ---
if (( SKIP_INSTALL )); then
    echo
    header "Skipping Install (-no-install)"
elif (( DEBUG_NOOP && RELEASE_NOOP )); then
    echo
    info "No changes detected - skipping install."
else
    t0=$(now)
    echo
    header "Installing Debug"
    cmake --install "$BUILD_DIR" --config Debug || die "Debug install failed."
    echo
    header "Installing Release"
    cmake --install "$BUILD_DIR" --config Release || die "Release install failed."
    INSTALL_SECS=$(( $(now) - t0 ))

    if (( BUILD_TOOLS )); then
        TOOLS_DIR="$SCRIPT_DIR/../Tools/ProjectCloner"
        CLONER_SRC="$SCRIPT_DIR/../Examples/ClonerSource"
        TOOLS_BUILD="$TOOLS_DIR/build/$PRESET"
        INSTALL_PREFIX="$HOME/Documents/DsQt"

        echo
        header "Configuring ProjectCloner"
        TOOLS_ARGS=(-S "$TOOLS_DIR" -B "$TOOLS_BUILD" -G Ninja
                    -DCMAKE_BUILD_TYPE=Release
                    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX/Tools/ProjectCloner")
        if [[ -n "$QT_PATH" ]]; then TOOLS_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PATH"); fi
        cmake "${TOOLS_ARGS[@]}" || die "ProjectCloner configure failed."

        echo
        header "Building ProjectCloner"
        cmake --build "$TOOLS_BUILD" --config Release || die "ProjectCloner build failed."

        echo
        header "Installing ProjectCloner"
        cmake --install "$TOOLS_BUILD" --config Release || die "ProjectCloner install failed."

        echo
        header "Copying ClonerSource template"
        rm -rf "$INSTALL_PREFIX/Tools/ClonerSource"
        mkdir -p "$INSTALL_PREFIX/Tools/ClonerSource"
        cp -R "$CLONER_SRC/." "$INSTALL_PREFIX/Tools/ClonerSource/" || die "ClonerSource copy failed."
        ok "Tools installed to: $INSTALL_PREFIX/Tools"
    fi
fi

# --------------------------------------------------------------- summary ---
OVERALL_SECS=$(( $(now) - OVERALL_START ))
echo
header "Timing Summary"
(( SKIP_CONFIGURE )) || printf ' Configure time      : %s\n' "$(fmt $CONFIGURE_SECS)"
printf ' Debug build time    : %s\n' "$(fmt $BUILD_DEBUG_SECS)"
printf ' Release build time  : %s\n' "$(fmt $BUILD_RELEASE_SECS)"
printf ' Total build time    : %s\n' "$(fmt $(( BUILD_DEBUG_SECS + BUILD_RELEASE_SECS )))"
if (( RUN_TESTS )); then printf ' Test time           : %s\n' "$(fmt $TEST_SECS)"; fi
if (( INSTALL_SECS )); then printf ' Total install time  : %s\n' "$(fmt $INSTALL_SECS)"; fi
printf ' Overall time        : %s\n' "$(fmt $OVERALL_SECS)"
echo
ok "Done."
