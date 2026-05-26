pragma ComponentBehavior: Bound

import QtQuick
import Dsqt.Bridge

// Content Launcher data adapter — reads the running bridge and produces the generic item shape
// the Waffles `DsContentLauncher` panel consumes:
//
//     { uid, title, kind, thumbnail, media, hasChildren, record }
//
// `media` carries the full bridge media descriptor (filepath, width, height, type, crop) when
// the record has one, or null for folders/playlists/non-media. Consumers use it as-is for
// `viewerProps.media` so the viewer can size to the real media aspect.
//
// Lives in `Dsqt.Bridge` so the Waffles UI module stays bridge-agnostic: Waffles depends on
// nothing here; this depends on Bridge; apps that want a bridge-driven launcher import both.
//
// Data sources (per the conventional "platform + interactive event" pattern):
//   - The current Platform record's menu-content link field (default appKey: `default_content`).
//   - The currently-active Interactive Event record's menu-content link field (default: `content`).
//   - Plus a "Current Playlist" row sourced from the event's interactive-playlist link if active,
//     otherwise the platform's default.
//
// Schema-configurable: every value specific to a particular schema (field appKeys, event type
// names, type-uid → kind mapping, thumbnail/title field candidates, per-type field overrides)
// is exposed as a writable property. To use this adapter against a different schema, override
// just the properties — no JS/imperative code required.
QtObject {
    id: adapter

    // -----------------------------------------------------------------------------------------
    // Schema configuration — override these per app/schema. Defaults match schema-downstreamdemo.
    // -----------------------------------------------------------------------------------------

    // Field appKeys on the platform record.
    property string platformContentField:  "default_content"
    property string platformPlaylistField: "default_interactive_playlist"

    // Field appKeys on the interactive event record.
    property string eventContentField:  "content"
    property string eventPlaylistField: "interactive_playlist"

    // Optional per-type appKey overrides — used only when multiple event types (or platform
    // types) in the same schema carry the menu-content trait under DIFFERENT appKeys. Keyed by
    // `record.type_name`; each entry is an object with one or both of:
    //   - `contentField`  : appKey for that type's menu-content link field
    //   - `playlistField` : appKey for that type's current-playlist link field
    // Any key not present falls through to the global *ContentField / *PlaylistField above.
    // Leave empty (the default `{}`) for uniform schemas — zero behavior change.
    //
    // Example:
    //   eventFieldOverrides: ({
    //       "Touch Module Event": { contentField: "module_content", playlistField: "module_playlist" }
    //   })
    property var eventFieldOverrides:    ({})
    property var platformFieldOverrides: ({})

    // Event type names (matched against record.type_name) that carry the menu-content trait, in
    // priority order. The first one whose DsEventSchedule has a `current` event wins.
    property var interactiveEventTypes: ["Connect Wall Event", "Explore Room Event"]

    // type_uid → kind. Most specific source — checked first because uids are stable.
    property var typeKindByUid: ({
        "JfDgLbj9vxT8": "media",
        "4fysJWac8KpK": "asset",
        "YhPdk0XdFnRD": "folder",
        "Q1zLmsTO9Ux9": "folder",
        "bweKU2WOurPH": "playlist",
        "TybD4YgiVHB3": "playlist"
    })
    // type_name → kind. Fallback when type_uid isn't mapped — useful when the type names happen
    // to be portable across schemas (e.g. "Media" / "Folder") but the uids aren't.
    property var typeKindByName: ({
        "Media": "media",
        "Assets": "asset",
        "Assets Folder": "folder",
        "Playlist Folder": "folder",
        "Interactive Playlist": "playlist",
        "Ambient Playlist": "playlist"
    })

    // The "media descriptor" field appKey on Media records — a hash with `.type` (image/video/pdf/
    // weblink) and `.filepath`. It refines `kind` for the `media` bucket and provides the primary
    // thumbnail.
    property string mediaField: "media"
    // Fallback thumbnail field appKeys, checked in order, for records that don't have a primary
    // media descriptor (e.g. folders, playlists with a cover image).
    property var thumbnailFields: ["thumbnail", "image", "icon", "preview", "header_image", "main_media"]
    // Title field appKeys, checked in order. The first non-empty wins; falls back to "(untitled)".
    property var titleFields: ["record_name", "headline", "title"]

    // -----------------------------------------------------------------------------------------
    // Outputs (UI binds to these).
    // -----------------------------------------------------------------------------------------

    // The CURRENT PLAYLIST row, or null.
    property var currentPlaylist: null
    // The CONTENT LIBRARY items at the ROOT level — platformContentField + eventContentField.
    property var library: []
    // Items currently displayed by the launcher — equals `library` at the root, or the children
    // of the deepest entry in `_navStack` when drilled in.
    property var displayedItems: []
    // Breadcrumb: the chain of items the user has entered (root → current).
    readonly property var path: {
        const p = [];
        for (let i = 0; i < adapter._navStack.length; ++i) p.push(adapter._navStack[i].item);
        return p;
    }
    // True when nothing is on the navigation stack (i.e. the root view).
    readonly property bool atRoot: adapter._navStack.length === 0
    // True when the bridge has yielded a platform record.
    property bool ready: false

    // -----------------------------------------------------------------------------------------
    // Internals.
    // -----------------------------------------------------------------------------------------

    // Map a content record to a launcher item.
    function _itemFor(rec) {
        if (!rec) return null;
        const typeUid  = rec.type_uid  || "";
        const typeName = rec.type_name || "";
        let kind = adapter.typeKindByUid[typeUid] || adapter.typeKindByName[typeName] || "unknown";

        // Refine "media" kind from the media descriptor's subtype (image/video/pdf/weblink) when
        // present, and grab its filepath as the primary thumbnail. Keep the descriptor itself on
        // the item so consumers can size to the real aspect ratio (and honour crop, etc.).
        let thumb = "";
        let mediaOut = null;
        const mediaDescriptor = rec[adapter.mediaField];
        if (mediaDescriptor && typeof mediaDescriptor === "object") {
            mediaOut = mediaDescriptor;
            if (mediaDescriptor.filepath) thumb = mediaDescriptor.filepath;
            if (kind === "media") {
                const sub = ("" + (mediaDescriptor.type || "")).toLowerCase();
                if (sub === "image" || sub === "video" || sub === "pdf") kind = sub;
                else if (sub === "weblink" || sub === "url" || sub === "link") kind = "weblink";
            }
        }
        // Fall back to a header/preview-style thumbnail field if no media descriptor.
        if (!thumb) {
            const fields = adapter.thumbnailFields || [];
            for (let i = 0; i < fields.length; ++i) {
                const v = rec[fields[i]];
                if (v && typeof v === "object" && v.filepath) { thumb = v.filepath; break; }
            }
        }

        // Title from the first non-empty configured field.
        let title = "";
        const tfields = adapter.titleFields || [];
        for (let i = 0; i < tfields.length; ++i) {
            const v = rec[tfields[i]];
            if (v) { title = "" + v; break; }
        }
        if (!title) title = "(untitled)";

        const hasChildren = (kind === "folder" || kind === "playlist");
        return {
            "uid":         rec.uid || "",
            "title":       title,
            "kind":        kind,
            "thumbnail":   thumb,
            "media":       mediaOut,
            "hasChildren": hasChildren,
            "record":      rec
        };
    }

    // Resolve a folder/playlist record's children into launcher items.
    //
    // Bridge auto-reparents linked records that have a single parent, so a folder's children are
    // normally accessible via the `children` property (a list of ContentModel records). For
    // playlists and any LINK-only fields we fall back to the `child_uid` list resolved via
    // getRecordById.
    function _resolveChildren(record) {
        if (!record) return [];
        const direct = record.children;
        if (direct && direct.length) {
            const out = [];
            for (let i = 0; i < direct.length; ++i) {
                const it = adapter._itemFor(direct[i]);
                if (it) out.push(it);
            }
            if (out.length) return out;
        }
        const uids = record.child_uid;
        if (uids && uids.length) {
            const arr = Array.isArray(uids) ? uids : Array.from(uids);
            const out = [];
            for (let i = 0; i < arr.length; ++i) {
                const r = DsBridge.getRecordById(arr[i]);
                const it = adapter._itemFor(r);
                if (it) out.push(it);
            }
            return out;
        }
        return [];
    }

    // Resolve a LINK field (QStringList of uids) into a list of items.
    function _resolveLinks(record, appKey) {
        if (!record || !appKey) return [];
        const raw = record[appKey];
        if (!raw) return [];
        // Single-link fields can arrive as a string, multi as an array.
        const uids = (typeof raw === "string") ? [raw]
                   : (Array.isArray(raw) ? raw : (raw.length !== undefined ? Array.from(raw) : []));
        const out = [];
        for (let i = 0; i < uids.length; ++i) {
            const r = DsBridge.getRecordById(uids[i]);
            const item = adapter._itemFor(r);
            if (item) out.push(item);
        }
        return out;
    }

    // Look up an appKey for a given record + role, honouring per-type overrides. Falls back to
    // the supplied default field name when no override applies.
    //   role  : "contentField" | "playlistField"
    //   defaultField : adapter.eventContentField / platformContentField / etc.
    //   overrides    : adapter.eventFieldOverrides / platformFieldOverrides
    function _fieldFor(record, role, defaultField, overrides) {
        const t = record && record.type_name;
        const o = (t && overrides) ? overrides[t] : null;
        return (o && o[role]) || defaultField;
    }

    // Resolve a single-link field (a uid or [uid]) into one item, or null.
    function _resolveSingleLink(record, appKey) {
        if (!record || !appKey) return null;
        const raw = record[appKey];
        const uid = Array.isArray(raw) ? raw[0] : raw;
        if (!uid) return null;
        const r = DsBridge.getRecordById(uid);
        return r ? adapter._itemFor(r) : null;
    }

    // Find the currently-active event record whose type carries the menu-content trait, by trying
    // each configured type name in priority order and returning the first `.current`.
    function _currentInteractiveEventRecord() {
        for (let i = 0; i < adapter.interactiveEventTypes.length; ++i) {
            const s = _scheduleByTypeName[adapter.interactiveEventTypes[i]];
            if (s && s.current && s.current.model)
                return s.current.model;
        }
        return null;
    }

    // --- Navigation ---
    // Each entry: { item: <launcher item the user entered>, children: <resolved item list> }.
    // Reassigned (never mutated) so QML property bindings re-evaluate.
    property var _navStack: []

    // Enter a folder/playlist. Resolves its children, pushes it onto the nav stack, and
    // updates displayedItems. No-op for leaf items.
    function enter(item) {
        if (!item || !item.hasChildren) return;
        const kids = adapter._resolveChildren(item.record);
        adapter._navStack = adapter._navStack.concat([{ item: item, children: kids }]);
        adapter.displayedItems = kids;
    }

    // Pop the deepest entry from the nav stack and update displayedItems. No-op at root.
    function back() {
        if (adapter._navStack.length === 0) return;
        const ns = adapter._navStack.slice(0, -1);
        adapter._navStack = ns;
        adapter.displayedItems = (ns.length === 0) ? adapter.library
                                                   : ns[ns.length - 1].children;
    }

    // --- Schedules: one per configured interactive event type. Built once, kept in sync. ---
    property var _scheduleByTypeName: ({})
    property var _schedules: []

    function _ensureSchedules() {
        const want = adapter.interactiveEventTypes || [];
        // Tear down old (rare; the list is normally static).
        for (let i = 0; i < adapter._schedules.length; ++i) {
            if (adapter._schedules[i]) adapter._schedules[i].destroy();
        }
        adapter._schedules = [];
        adapter._scheduleByTypeName = {};
        for (let i = 0; i < want.length; ++i) {
            const s = _scheduleComponent.createObject(adapter, { "type": want[i] });
            if (s) {
                s.eventsChanged.connect(adapter.rebuild);
                adapter._schedules.push(s);
                adapter._scheduleByTypeName[want[i]] = s;
            }
        }
    }

    // Reconfigure schedules + rebuild when the interactive-event types list is changed.
    onInteractiveEventTypesChanged: {
        adapter._ensureSchedules();
        adapter.rebuild();
    }

    property Component _scheduleComponent: Component { DsEventSchedule { } }

    // --- Rebuild ---
    function rebuild() {
        const platform = DsBridge.getPlatformRecord();
        const eventRec = adapter._currentInteractiveEventRecord();

        // Resolve appKeys honouring per-type overrides (falls back to the global defaults).
        const platContent  = adapter._fieldFor(platform, "contentField",  adapter.platformContentField,  adapter.platformFieldOverrides);
        const platPlaylist = adapter._fieldFor(platform, "playlistField", adapter.platformPlaylistField, adapter.platformFieldOverrides);
        const evtContent   = adapter._fieldFor(eventRec, "contentField",  adapter.eventContentField,     adapter.eventFieldOverrides);
        const evtPlaylist  = adapter._fieldFor(eventRec, "playlistField", adapter.eventPlaylistField,    adapter.eventFieldOverrides);

        // Current playlist: event wins if present, else platform default.
        let cp = adapter._resolveSingleLink(eventRec, evtPlaylist);
        if (!cp) cp = adapter._resolveSingleLink(platform, platPlaylist);

        // Library: platform content first, then event content.
        const lib = []
            .concat(adapter._resolveLinks(platform, platContent))
            .concat(adapter._resolveLinks(eventRec, evtContent));

        adapter.currentPlaylist = cp;
        adapter.library = lib;
        // Reset navigation on rebuild — records may have been recreated by a bridge sync.
        adapter._navStack = [];
        adapter.displayedItems = lib;
        adapter.ready = (platform !== null);

        if (adapter._verbose) adapter.dump();
    }

    // --- Console dump (verification helper) ---
    property bool _verbose: true

    function dump() {
        const platform = DsBridge.getPlatformRecord();
        const eventRec = adapter._currentInteractiveEventRecord();
        console.log("==== DsContentLauncherModel dump ====");
        console.log("Platform record:",
                    platform ? (platform.record_name + " [" + platform.uid + ", type=" + platform.type_name + "]")
                             : "(none — check app_settings.platform.id and the bridge db)");
        if (platform) {
            const pc = adapter._fieldFor(platform, "contentField",  adapter.platformContentField,  adapter.platformFieldOverrides);
            const pp = adapter._fieldFor(platform, "playlistField", adapter.platformPlaylistField, adapter.platformFieldOverrides);
            const dc = platform[pc];
            console.log("  platform." + pc + " uids:",
                        Array.isArray(dc) ? ("[" + dc.length + "] " + JSON.stringify(dc)) : JSON.stringify(dc));
            console.log("  platform." + pp + ":",
                        JSON.stringify(platform[pp]));
        }
        console.log("Current interactive event:",
                    eventRec ? (eventRec.record_name + " [" + eventRec.uid + ", type=" + eventRec.type_name + "]")
                             : "(none active in schedule for types " + JSON.stringify(adapter.interactiveEventTypes) + ")");
        if (eventRec) {
            const ec = adapter._fieldFor(eventRec, "contentField",  adapter.eventContentField,  adapter.eventFieldOverrides);
            const ep = adapter._fieldFor(eventRec, "playlistField", adapter.eventPlaylistField, adapter.eventFieldOverrides);
            const ecv = eventRec[ec];
            console.log("  event." + ec + " uids:",
                        Array.isArray(ecv) ? ("[" + ecv.length + "] " + JSON.stringify(ecv)) : JSON.stringify(ecv));
            console.log("  event." + ep + ":",
                        JSON.stringify(eventRec[ep]));
        }
        console.log("currentPlaylist:",
                    adapter.currentPlaylist
                        ? (adapter.currentPlaylist.title + " (" + adapter.currentPlaylist.kind + ", " + adapter.currentPlaylist.uid + ")")
                        : "(none)");
        console.log("library: " + adapter.library.length + " item(s)");
        for (let i = 0; i < adapter.library.length; ++i) {
            const it = adapter.library[i];
            console.log("  [" + i + "] " + it.kind.padEnd(8, " ") +
                        " " + it.title +
                        "  (" + it.uid + ")" +
                        (it.thumbnail ? "  thumb=" + it.thumbnail : "") +
                        (it.hasChildren ? "  [hasChildren]" : ""));
        }
        console.log("======================================");
    }

    // Verification helper: for every CONTENT LIBRARY item that has children, resolve and print
    // one level of children. Lets you see the shape of the actual content tree.
    function dumpAllChildren() {
        console.log("==== DsContentLauncherModel children dump ====");
        if (adapter.library.length === 0) {
            console.log("(empty library — run dump() first to see why)");
        }
        for (let i = 0; i < adapter.library.length; ++i) {
            const top = adapter.library[i];
            console.log("[" + top.kind + "] " + top.title + " (" + top.uid + "):");
            if (!top.hasChildren) {
                console.log("  (leaf — no children expected)");
                continue;
            }
            const kids = adapter._resolveChildren(top.record);
            if (kids.length === 0) {
                console.log("  (empty — no children resolved)");
            } else {
                for (let j = 0; j < kids.length; ++j) {
                    const it = kids[j];
                    console.log("  - " + it.kind.padEnd(8, " ") + " " + it.title +
                                "  (" + it.uid + ")" +
                                (it.thumbnail ? "  thumb=" + it.thumbnail : "") +
                                (it.hasChildren ? "  [hasChildren]" : ""));
                }
            }
        }
        console.log("==============================================");
    }

    // --- Lifecycle ---
    property var _bridge: DsBridge
    property var _conn: Connections {
        target: adapter._bridge
        function onContentChanged() { adapter.rebuild(); }
        function onBridgeUpdated()  { adapter.rebuild(); }
    }

    Component.onCompleted: {
        adapter._ensureSchedules();
        // First rebuild — content may not be ready yet; signals will retrigger.
        adapter.rebuild();
    }
}
