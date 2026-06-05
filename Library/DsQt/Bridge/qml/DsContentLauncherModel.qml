pragma ComponentBehavior: Bound

import QtQuick
import Dsqt.Bridge

// Content Launcher data adapter — reads the running bridge and produces the generic item shape
// the Waffles `DsContentLauncher` panel consumes:
//
//     { uid, title, kind, thumbnail, media, hasChildren, recordHasChildren, record }
//
// `media` carries the full bridge media descriptor (filepath, width, height, type, crop) when
// the record has one, or null for folders/playlists/non-media. Consumers use it as-is for
// `viewerProps.media` so the viewer can size to the real media aspect.
//
// **Drillable vs openable** — two independent concepts:
//   - `recordHasChildren` is a *data fact* — does the bridge record actually carry child
//     records? Useful for consumers like a presentation viewer that wants to walk the children
//     once an item is opened.
//   - `hasChildren` is a *UX hint* — should the launcher drill into this item on tap, or open
//     it via `openRequested`? Derived purely from `kind` being in `drillableKinds`.
// So a Presentation (record has children → openable in a presentation viewer) is `kind` =
// `"presentation"` (not in drillableKinds) → tap opens, no drill. An empty folder is still
// drillable. A Bridge "Assets" container that holds media → map to `"folder"` (default does
// this for the demo schema) → drillable.
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
    // The platform's default AMBIENT (attract-loop) playlist link field.
    property string platformAmbientPlaylistField: "default_ambient_playlist"

    // Field appKeys on the interactive event record.
    property string eventContentField:  "content"
    property string eventPlaylistField: "interactive_playlist"
    // The event's AMBIENT playlist link field.
    property string eventAmbientPlaylistField: "ambient_playlist"

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
        "4fysJWac8KpK": "folder",      // demo "Assets" is a container of media → drillable
        "YhPdk0XdFnRD": "folder",
        "Q1zLmsTO9Ux9": "folder",
        "bweKU2WOurPH": "playlist",
        "TybD4YgiVHB3": "playlist"
    })
    // type_name → kind. Fallback when type_uid isn't mapped — useful when the type names happen
    // to be portable across schemas (e.g. "Media" / "Folder") but the uids aren't.
    property var typeKindByName: ({
        "Media": "media",
        "Assets": "folder",            // see above — demo Assets is a container
        "Assets Folder": "folder",
        "Playlist Folder": "folder",
        "Interactive Playlist": "playlist",
        "Ambient Playlist": "playlist"
    })

    // Which kinds the launcher should drill into on tap (chevron shown). Anything not listed
    // here is openable (tap → openRequested), even if its record actually has children. This is
    // how a Presentation (`kind: "presentation"`) stays openable despite having slide children:
    // give it a custom kind and don't list that kind here.
    property var drillableKinds: ["folder", "playlist"]

    // The "media descriptor" field appKey on Media records — a hash with `.type` (image/video/pdf/
    // weblink) and `.filepath`. It refines `kind` for the `media` bucket and provides the primary
    // thumbnail.
    property string mediaField: "media"
    // Fallback thumbnail field appKeys, checked in order, for records that don't have a primary
    // media descriptor (e.g. folders, playlists with a cover image).
    property var thumbnailFields: ["thumbnail", "image", "icon", "preview", "header_image", "main_media"]
    // Title field appKeys, checked in order. The first non-empty wins; falls back to "(untitled)".
    property var titleFields: ["record_name", "headline", "title"]

    // --- Playlist / slide config (Phase 0: presentations + ambient playlists) ---
    // A playlist record's slides are its child template records (resolved via _resolveChildren).
    // Ambient auto-advance reads a per-slide hold time (seconds): the slide's own
    // `ambientHoldTimeField` → else the platform's → else `defaultHoldTimeSeconds`. The schema
    // carries `ambient_hold_time` via a reusable "Hold Time / Transition" trait, so the same appKey
    // works on a slide and (if the trait is applied) the platform.
    property string ambientHoldTimeField:  "ambient_hold_time"
    // Per-slide "skip the transition" flag appKey (CHECKBOX). Read by the playlist viewer.
    property string disableTransitionField: "disable_transition"
    // Per-slide transition NAME appKey (e.g. "fade" / "slideLeft" / a custom name). Empty/absent →
    // the playlist's default transition. (No transition-type field exists in the demo schema yet —
    // this resolves to "" until one is added; disable_transition still gives a per-slide "none".)
    property string transitionField: "transition"
    // Fallback hold time (seconds) when neither slide nor platform specify one. Apps can drive this
    // from a setting (e.g. [waffles.playlist] defaultHoldTime) by assigning it.
    property real   defaultHoldTimeSeconds: 8

    // -----------------------------------------------------------------------------------------
    // Outputs (UI binds to these).
    // -----------------------------------------------------------------------------------------

    // The CURRENT (interactive) PLAYLIST row, or null.
    property var currentPlaylist: null
    // The CURRENT AMBIENT (attract-loop) playlist row, or null. Used by the app to auto-start the
    // attract loop on boot and return to it on idle.
    property var currentAmbientPlaylist: null
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

    // --- Search (Phase 2c) ---
    // Inputs (the launcher's search pane writes these):
    //   - searchQuery : free-text; case-insensitive substring match against item titles.
    //   - searchKinds : kind filter; empty = all kinds. e.g. ["image"], or ["folder","playlist"].
    // Output:
    //   - searchResults : flat list of matching items (same item shape as displayedItems),
    //     gathered by recursively walking the LOADED menu content (currentPlaylist + library and
    //     their descendants). Bridge-only content not placed in a menu is not searched.
    // Empty query AND empty kind filter => empty results (nothing to show yet); a kind filter with
    // no text lists everything of that kind.
    property string searchQuery: ""
    property var    searchKinds: []
    property var    searchResults: []

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
            // Refine the kind from the media descriptor's subtype. Applies to "media" and also
            // "unknown" (a web/media record whose content type isn't mapped still classifies by
            // its descriptor), but never to container kinds (folder/playlist may carry a cover
            // image without becoming an image leaf).
            if (kind === "media" || kind === "unknown") {
                const sub = ("" + (mediaDescriptor.type || "")).toLowerCase();
                if (sub === "image" || sub === "video" || sub === "pdf") kind = sub;
                // The bridge serialises a WEB media descriptor's type as "web" (DsResource
                // getTypeName → WEB_NAME_SZ); youtube is also web-rendered. Accept url/link/
                // weblink aliases too for other schemas.
                else if (sub === "web" || sub === "weblink" || sub === "url"
                         || sub === "link" || sub === "youtube") kind = "weblink";
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

        // recordHasChildren = data fact (record actually carries children).
        // hasChildren       = UX choice (launcher drills into this kind). Independent.
        const recordHasChildren =
            (rec.children && rec.children.length > 0) ||
            (rec.child_uid && rec.child_uid.length > 0);
        const hasChildren = (adapter.drillableKinds || []).indexOf(kind) !== -1;
        return {
            "uid":               rec.uid || "",
            "title":             title,
            "kind":              kind,
            "thumbnail":         thumb,
            "media":             mediaOut,
            "hasChildren":       hasChildren,
            "recordHasChildren": recordHasChildren,
            "record":            rec
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
        const arr = adapter._uidsFromField(record.child_uid);
        if (arr.length) {
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

    // --- Playlist slides (Phase 0) ---
    // Resolve a playlist record into an ordered list of slide descriptors the playlist viewer
    // consumes. Each slide reuses the launcher item shape (so templates can read `media`/`thumbnail`
    // and the raw `record` for template-specific fields) plus:
    //   - typeUid  : the slide's template type_uid → maps to a QML template component (registry)
    //   - holdTime : resolved ambient auto-advance time in seconds (slide → platform → default)
    //   - disableTransition : per-slide flag to skip the transition
    // Order follows _resolveChildren (the bridge's child order). NOTE: the playlist slots declare
    // `reverseOrdering: true` — if slides come out reversed on device, reverse here.
    function slidesFor(playlistRecord) {
        if (!playlistRecord) return [];
        const kids = adapter._resolveChildren(playlistRecord);
        const out = [];
        for (let i = 0; i < kids.length; ++i) {
            const it  = kids[i];
            const rec = it.record;
            out.push({
                "uid":               it.uid,
                "title":             it.title,
                "kind":              it.kind,
                "typeUid":           (rec && rec.type_uid) ? rec.type_uid : "",
                "media":             it.media,
                "thumbnail":         it.thumbnail,
                "holdTime":          adapter._holdTimeFor(rec),
                "disableTransition": !!(rec && rec[adapter.disableTransitionField]),
                "transition":        (rec && rec[adapter.transitionField]) ? ("" + rec[adapter.transitionField]) : "",
                "record":            rec
            });
        }
        return out;
    }

    // Resolve a slide's ambient auto-advance hold time (seconds): the slide's own field, else the
    // platform's, else the configured default. 0 / undefined fall through.
    function _holdTimeFor(slideRecord) {
        const f = adapter.ambientHoldTimeField;
        const st = slideRecord ? Number(slideRecord[f]) : 0;
        if (st && st > 0) return st;
        const platform = DsBridge.getPlatformRecord();
        const pt = platform ? Number(platform[f]) : 0;
        if (pt && pt > 0) return pt;
        return adapter.defaultHoldTimeSeconds;
    }

    // Resolve a LINK field (QStringList of uids) into a list of items.
    // Normalise a LINK field value into an array of uids. The bridge serialises a MULTI-value link
    // field as a single COMMA-SEPARATED STRING ("uid1,uid2,uid3"); a single link as a plain uid
    // string; and some fields as a list. Handle all three. (Splitting a single uid with no comma
    // yields [uid], so single-value fields are unaffected.) This is why an "additional content"
    // field with multiple items resolved to nothing before — "uid1,uid2" was used as one uid.
    function _uidsFromField(raw) {
        if (!raw) return [];
        // Normalise to a list of entries first (string / array / array-like).
        let entries;
        if (typeof raw === "string")        entries = [raw];
        else if (Array.isArray(raw))        entries = raw;
        else if (raw.length !== undefined)  entries = Array.from(raw);
        else                                return [];
        // Then comma-split EACH entry and flatten. The bridge returns a multi-value link as an
        // ARRAY whose (single) element is a comma-joined string of uids — e.g. ["uid1,uid2"] — so
        // splitting only a top-level string isn't enough; every entry must be split.
        const out = [];
        for (let i = 0; i < entries.length; ++i) {
            const e = entries[i];
            if (typeof e === "string") {
                const parts = e.split(",");
                for (let j = 0; j < parts.length; ++j) {
                    const u = parts[j].trim();
                    if (u.length > 0) out.push(u);
                }
            } else if (e) {
                out.push(e);
            }
        }
        return out;
    }

    function _resolveLinks(record, appKey) {
        if (!record || !appKey) return [];
        const uids = adapter._uidsFromField(record[appKey]);
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

    // Resolve a single-link field into one item (the first uid), or null. Uses _uidsFromField so a
    // comma-separated multi value takes its first entry rather than the whole string.
    function _resolveSingleLink(record, appKey) {
        if (!record || !appKey) return null;
        const uids = adapter._uidsFromField(record[appKey]);
        if (uids.length === 0) return null;
        const r = DsBridge.getRecordById(uids[0]);
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

    // --- Search ---
    // Recursively collect items under `item` (inclusive) whose title matches `q` (already
    // lower-cased; "" = match all) and whose kind passes `kinds` (empty = all). `visited` guards
    // against link cycles. Descends into any record that actually carries children.
    function _collectMatches(item, q, kinds, visited, out) {
        if (!item) return;
        if (item.uid) {
            if (visited[item.uid]) return;
            visited[item.uid] = true;
        }
        const titleOk = !q || (("" + item.title).toLowerCase().indexOf(q) !== -1);
        const kindOk  = (kinds.length === 0) || (kinds.indexOf(item.kind) !== -1);
        if (titleOk && kindOk) out.push(item);
        if (item.recordHasChildren) {
            const kids = adapter._resolveChildren(item.record);
            for (let i = 0; i < kids.length; ++i)
                adapter._collectMatches(kids[i], q, kinds, visited, out);
        }
    }

    // Recompute searchResults from the current query/kinds against the loaded menu tree.
    function _runSearch() {
        const q = ("" + adapter.searchQuery).trim().toLowerCase();
        const kinds = adapter.searchKinds || [];
        if (!q && kinds.length === 0) { adapter.searchResults = []; return; }
        const out = [];
        const visited = ({});
        const roots = [];
        if (adapter.currentPlaylist) roots.push(adapter.currentPlaylist);
        for (let i = 0; i < adapter.library.length; ++i) roots.push(adapter.library[i]);
        for (let i = 0; i < roots.length; ++i)
            adapter._collectMatches(roots[i], q, kinds, visited, out);
        adapter.searchResults = out;
    }

    onSearchQueryChanged: adapter._runSearch()
    onSearchKindsChanged:  adapter._runSearch()

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
        // Remember where the user was so we can replay the breadcrumb against the freshly
        // rebuilt tree. Records can get recreated by a bridge sync, so we replay by uid
        // (stable identity) — that also picks up fresh `record` handles and child lists.
        const savedPath = adapter._navStack.map(function (e) { return e.item.uid; });

        const platform = DsBridge.getPlatformRecord();
        const eventRec = adapter._currentInteractiveEventRecord();

        // Resolve appKeys honouring per-type overrides (falls back to the global defaults).
        const platContent  = adapter._fieldFor(platform, "contentField",  adapter.platformContentField,  adapter.platformFieldOverrides);
        const platPlaylist = adapter._fieldFor(platform, "playlistField", adapter.platformPlaylistField, adapter.platformFieldOverrides);
        const evtContent   = adapter._fieldFor(eventRec, "contentField",  adapter.eventContentField,     adapter.eventFieldOverrides);
        const evtPlaylist  = adapter._fieldFor(eventRec, "playlistField", adapter.eventPlaylistField,    adapter.eventFieldOverrides);

        // Current (interactive) playlist: event wins if present, else platform default.
        let cp = adapter._resolveSingleLink(eventRec, evtPlaylist);
        if (!cp) cp = adapter._resolveSingleLink(platform, platPlaylist);

        // Current AMBIENT playlist (attract loop): event wins if present, else platform default.
        let ap = adapter._resolveSingleLink(eventRec, adapter.eventAmbientPlaylistField);
        if (!ap) ap = adapter._resolveSingleLink(platform, adapter.platformAmbientPlaylistField);

        // Library: platform content first, then event content.
        const lib = []
            .concat(adapter._resolveLinks(platform, platContent))
            .concat(adapter._resolveLinks(eventRec, evtContent));

        adapter.currentPlaylist = cp;
        adapter.currentAmbientPlaylist = ap;
        adapter.library = lib;

        // Replay the saved navigation path by uid. Stop at the first missing level — the
        // user falls back to the deepest ancestor that still exists (potentially root).
        const newStack = [];
        let displayed = lib;
        for (let i = 0; i < savedPath.length; ++i) {
            const uid = savedPath[i];
            let found = null;
            for (let j = 0; j < displayed.length; ++j) {
                if (displayed[j].uid === uid) { found = displayed[j]; break; }
            }
            if (!found) break;
            const kids = adapter._resolveChildren(found.record);
            newStack.push({ item: found, children: kids });
            displayed = kids;
        }
        adapter._navStack = newStack;
        adapter.displayedItems = displayed;
        adapter.ready = (platform !== null);

        // Re-run any active search against the freshly rebuilt tree.
        adapter._runSearch();

        if (adapter._verbose) adapter.dump();
    }

    // --- Console dump (verification helper) ---
    // Off by default — flip to true (or call dump()/dumpAllChildren() from the console) when you
    // need to inspect what the bridge is yielding. With it on, every content change spams a full
    // dump since each change triggers a rebuild().
    property bool _verbose: false

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
            console.log("[" + top.kind + "] " + top.title + " (" + top.uid + ")" +
                        (top.hasChildren ? " [drillable]" : "") +
                        (top.recordHasChildren ? " [has children]" : "") + ":");
            if (!top.recordHasChildren) {
                console.log("  (no children)");
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
    // Bridge signals are wired via a typed `property Connections` (not `property var`) so the
    // QML engine keeps the Connections object alive for the adapter's lifetime. Explicit
    // `signal.connect()` calls from Component.onCompleted weren't establishing a durable
    // subscription against the DsBridge singleton — the typed property form is the canonical
    // QML pattern and gets cleaned up automatically when the adapter goes away.
    property Connections _bridgeConn: Connections {
        target: DsBridge
        function onContentChanged()  { adapter.rebuild(); }
        function onBridgeUpdated()   { adapter.rebuild(); }
        function onDatabaseChanged() { adapter.rebuild(); }
    }

    Component.onCompleted: {
        adapter._ensureSchedules();
        // First rebuild — content may not be ready yet; signals will retrigger.
        adapter.rebuild();
    }
}
