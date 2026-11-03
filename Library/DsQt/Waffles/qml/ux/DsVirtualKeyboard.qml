pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings
import Dsqt.Core

// Thin wrapper around the Qt Virtual Keyboard InputPanel.
//
// Styling: the dark "waffles" style is bootstrapped app-side via the QT_VIRTUALKEYBOARD_STYLE
// env var (set in each app's main.cpp) and can be overridden at runtime from
// [waffles.keyboard] style (applied below via VirtualKeyboardSettings.styleName). The style ships
// in Dsqt.Core's module resources at qrc:/qt/qml/Dsqt/Core/res/QtQuick/VirtualKeyboard/Styles/
// waffles/; DsQmlApplicationEngine adds qrc:/qt/qml/Dsqt/Core/res to the QML import path so VK
// resolves it. (Bundled as individual files, not a .qrc — a .qrc's init is dead-stripped from the
// static Core lib.)
//
// Routing: requires QT_IM_MODULE=qtvirtualkeyboard (also set in main.cpp) so focused text inputs
// route through the virtual keyboard.
//
// Sizing: fills its parent's width; the InputPanel derives its own height from that width and the
// keyboard's design aspect. `panelActive` is true while a focused control has requested input —
// hosts use it to allocate/animate the docked space. Place one instance where the keyboard should
// dock. NOTE: all InputPanels in a window share one InputContext, so keep at most one active at a
// time (host this on a Loader gated by the relevant mode).
Item {
    id: kbd

    // True while the virtual keyboard requests to be shown (a control has active input focus).
    readonly property alias panelActive: inputPanel.active
    // The intrinsic height the panel wants at the current width.
    readonly property alias panelHeight: inputPanel.height

    implicitHeight: inputPanel.height

    // Active keyboard locales (multilingual). Defaults to a Latin set; override via
    //   [waffles.keyboard]
    //   locales = ["en_US", "es_ES", ...]
    // in waffles_settings.toml. Broaden by adding CJK/Arabic locales (e.g. "zh_CN", "ja_JP",
    // "ko_KR", "ar_AR") or list every installed layout. The globe key cycles the active set.
    // activeLocales is a global VirtualKeyboardSettings singleton; applying it from any instance
    // is idempotent.
    property DsSettingsProxy _kbdSettings: DsSettingsProxy { target: "app_settings"; prefix: "waffles.keyboard" }
    readonly property var _defaultLocales: ["en_US", "es_ES", "fr_FR", "de_DE", "it_IT"]

    Component.onCompleted: {
        const raw = kbd._kbdSettings.getList("locales", []);
        const locales = (raw && raw.length > 0) ? raw : kbd._defaultLocales;
        VirtualKeyboardSettings.activeLocales = locales;
        // Default the initial layout to the first active locale (e.g. en_US) rather than Qt VK's
        // en_GB fallback.
        if (locales.length > 0)
            VirtualKeyboardSettings.locale = locales[0];
        // Keyboard style — overrides the QT_VIRTUALKEYBOARD_STYLE env-var bootstrap (set in
        // main.cpp) at runtime, so swapping styles is a one-line [waffles.keyboard] style edit with
        // no recompile. The named style must resolve on a QML import path (Core adds
        // qrc:/qt/qml/Dsqt/Core/res, where the bundled "waffles" style lives).
        const style = "" + kbd._kbdSettings.getString("style", "waffles");
        if (style.length > 0)
            VirtualKeyboardSettings.styleName = style;
    }

    InputPanel {
        id: inputPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
