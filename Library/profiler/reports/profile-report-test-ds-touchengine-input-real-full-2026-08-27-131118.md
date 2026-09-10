# QML Performance Profile: TouchEngine Texture Input

## Profile metadata

- Profile mode: full
- Trace: `C:\dev\DsQt\Library\profiler\traces\qmlprofiler-trace-test-ds-touchengine-input-real-full-2026-08-27-122900.qtd`
- Wall-clock duration unavailable (no animation events captured).
- Sum of captured range-event durations (binding/JS/creating/etc.; **not** wall-clock time): 30.18 ms
- Captured events: 4,410
- Workload: Vulkan, 11,520×2,160 texture-input round trip, 2-second warm-up, 6-second measurement

## Event type summary

Raw event counts scale with run length and interaction pattern. The total duration and average event cost are more useful than count alone for this trace.

| Type | Count | Total |
|---|---:|---:|
| Binding | 2,146 | 14.20 ms |
| Compiling | 2 | 12.27 ms |
| JavaScript | 2,177 | 3.45 ms |
| Creating | 16 | 0.27 ms |

No animation frame events were captured, so per-frame QML timing and frame-time percentiles are unavailable. Scene-graph timing was captured by a separate rendering-only trace, but Qt 6.11 stores those packets in a format that the trace summarizer does not convert into range-event durations.

## Memory summary

**Verdict: the QML engine allocation footprint is small and bounded for this workload; the trace shows no JavaScript garbage collection during the captured interval.**

The trace contains 67 small-object allocations totaling 580.1 KiB, with 0% reclaimed before exit. One 632.5 KiB GC heap-page allocation supplied that object storage. Peak live GC heap was 632.5 KiB; small JavaScript objects accounted for 580.1 KiB live at exit. This is QML/JavaScript heap data only and does not include the large graphics resources owned by Qt, TouchEngine, or the graphics driver.

| Category | Allocations | Total allocated | Reclaimed | Peak live | Live at exit |
|---|---:|---:|---:|---:|---:|
| GC heap pages | 1 | 632.5 KiB | 0 B | 632.5 KiB | 632.5 KiB |
| Small JS objects | 67 | 580.1 KiB | 0 B | 580.1 KiB | 580.1 KiB |

GC heap pages are allocator-owned regions used to hold garbage-collected values. Small JS objects are the individual QML/JavaScript allocations placed into those pages. The Large JS objects category was zero and is omitted from the table.

No pixmap-cache events were captured.

## Top hotspots

| Rank | Total | Count | Average | Type | Source location | Details |
|---:|---:|---:|---:|---|---|---|
| 1 | 11.74 ms | 1 | 11.736 ms | Compiling | [TextureInputPattern.qml:0](../../Tests/TouchEngine/real/TextureInputPattern.qml#L1) | File compilation |
| 2 | 9.68 ms | 268 | 0.036 ms | Binding | [TextureInputPattern.qml:72](../../Tests/TouchEngine/real/TextureInputPattern.qml#L72) | Marker color binding |
| 3 | 2.03 ms | 268 | 0.008 ms | JavaScript | [TextureInputPattern.qml:72](../../Tests/TouchEngine/real/TextureInputPattern.qml#L72) | `expression for color` |
| 4 | 0.98 ms | 268 | 0.004 ms | Binding | [TextureInputPattern.qml:64](../../Tests/TouchEngine/real/TextureInputPattern.qml#L64) | Marker color binding |
| 5 | 0.75 ms | 268 | 0.003 ms | Binding | [TextureInputPattern.qml:56](../../Tests/TouchEngine/real/TextureInputPattern.qml#L56) | Marker color binding |
| 6 | 0.60 ms | 268 | 0.002 ms | Binding | [TextureInputPattern.qml:25](../../Tests/TouchEngine/real/TextureInputPattern.qml#L25) | Quadrant color binding |
| 7 | 0.58 ms | 268 | 0.002 ms | Binding | [TextureInputPattern.qml:32](../../Tests/TouchEngine/real/TextureInputPattern.qml#L32) | Quadrant color binding |
| 8 | 0.54 ms | 268 | 0.002 ms | Binding | [TextureInputPattern.qml:18](../../Tests/TouchEngine/real/TextureInputPattern.qml#L18) | Quadrant color binding |
| 9 | 0.54 ms | 268 | 0.002 ms | Binding | [TextureInputPattern.qml:48](../../Tests/TouchEngine/real/TextureInputPattern.qml#L48) | Marker color binding |
| 10 | 0.53 ms | 1 | 0.530 ms | Compiling | `[source unresolved]` | No filename in trace |
| 11 | 0.52 ms | 268 | 0.002 ms | Binding | [TextureInputPattern.qml:40](../../Tests/TouchEngine/real/TextureInputPattern.qml#L40) | Quadrant color binding |
| 12 | 0.29 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:56](../../Tests/TouchEngine/real/TextureInputPattern.qml#L56) | `expression for color` |
| 13 | 0.25 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:64](../../Tests/TouchEngine/real/TextureInputPattern.qml#L64) | `expression for color` |
| 14 | 0.23 ms | 2 | 0.115 ms | Creating | [TextureInputPattern.qml:3](../../Tests/TouchEngine/real/TextureInputPattern.qml#L3) | `QtQuick/Item` |
| 15 | 0.21 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:25](../../Tests/TouchEngine/real/TextureInputPattern.qml#L25) | `expression for color` |
| 16 | 0.17 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:48](../../Tests/TouchEngine/real/TextureInputPattern.qml#L48) | `expression for color` |
| 17 | 0.16 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:32](../../Tests/TouchEngine/real/TextureInputPattern.qml#L32) | `expression for color` |
| 18 | 0.16 ms | 268 | 0.001 ms | JavaScript | [TextureInputPattern.qml:18](../../Tests/TouchEngine/real/TextureInputPattern.qml#L18) | `expression for color` |
| 19 | 0.16 ms | 267 | 0.001 ms | JavaScript | [TextureInputPattern.qml:40](../../Tests/TouchEngine/real/TextureInputPattern.qml#L40) | `expression for color` |
| 20 | 0.01 ms | 2 | 0.007 ms | Creating | [TextureInputPattern.qml:15](../../Tests/TouchEngine/real/TextureInputPattern.qml#L15) | `QtQuick/Rectangle` |
| 21 | 0.01 ms | 1 | 0.012 ms | Binding | [TextureInputPattern.qml:11](../../Tests/TouchEngine/real/TextureInputPattern.qml#L11) | Marker-color array binding |
| 22 | 0.01 ms | 1 | 0.005 ms | JavaScript | [TextureInputPattern.qml:11](../../Tests/TouchEngine/real/TextureInputPattern.qml#L11) | `expression for markerColors` |
| 23 | <0.01 ms | 2 | 0.002 ms | Creating | [TextureInputPattern.qml:21](../../Tests/TouchEngine/real/TextureInputPattern.qml#L21) | `QtQuick/Rectangle` |
| 24 | <0.01 ms | 2 | 0.002 ms | JavaScript | [TextureInputPattern.qml:52](../../Tests/TouchEngine/real/TextureInputPattern.qml#L52) | `expression for x` |
| 25 | <0.01 ms | 2 | 0.002 ms | JavaScript | [TextureInputPattern.qml:17](../../Tests/TouchEngine/real/TextureInputPattern.qml#L17) | `expression for height` |
| 26 | <0.01 ms | 2 | 0.002 ms | Creating | [TextureInputPattern.qml:28](../../Tests/TouchEngine/real/TextureInputPattern.qml#L28) | `QtQuick/Rectangle` |
| 27 | <0.01 ms | 2 | 0.002 ms | Creating | [TextureInputPattern.qml:51](../../Tests/TouchEngine/real/TextureInputPattern.qml#L51) | `QtQuick/Rectangle` |
| 28 | <0.01 ms | 2 | 0.002 ms | JavaScript | [TextureInputPattern.qml:30](../../Tests/TouchEngine/real/TextureInputPattern.qml#L30) | `expression for width` |
| 29 | <0.01 ms | 1 | 0.003 ms | Creating | [TextureInputPattern.qml:59](../../Tests/TouchEngine/real/TextureInputPattern.qml#L59) | `QtQuick/Rectangle` |
| 30 | <0.01 ms | 2 | 0.002 ms | Creating | [TextureInputPattern.qml:43](../../Tests/TouchEngine/real/TextureInputPattern.qml#L43) | `QtQuick/Rectangle` |

## Detailed analysis

### 1. [TextureInputPattern.qml:3](../../Tests/TouchEngine/real/TextureInputPattern.qml#L3) — file compilation

```qml
Item {
    id: root
    // Eight small Rectangle children form the deterministic test pattern.
}
```

The 11.74 ms compilation event occurs once during component creation. It is startup work, not a recurring frame cost, and is not responsible for sustained TouchEngine slowdown. If startup latency becomes important, verify that the production build uses Qt's QML cache/AOT tooling; no source rewrite is justified by this trace.

### 2. [TextureInputPattern.qml:72](../../Tests/TouchEngine/real/TextureInputPattern.qml#L72) — bottom-right marker color

```qml
color: root.markerColors[(root.phase + 3) % 4]
```

This is the largest recurring QML hotspot: 9.68 ms of binding work plus 2.03 ms of JavaScript over 268 evaluations. The average combined evaluation is about 0.044 ms. It is measurable because every benchmark phase change reevaluates the expression, but it is still far below a 16.67 ms frame budget. Keep it as-is for test readability unless the synthetic pattern itself becomes a profiling target.

### 3. [TextureInputPattern.qml:72](../../Tests/TouchEngine/real/TextureInputPattern.qml#L72) — JavaScript expression execution

The profiler records binding bookkeeping and expression execution separately for the same line. The modulo/index expression is simple; the higher total at this marker is not evidence of a general JavaScript bottleneck. If the pattern is ever expanded to hundreds of markers, compute the four phase colors once on the root and bind children to those precomputed properties.

### 4. [TextureInputPattern.qml:64](../../Tests/TouchEngine/real/TextureInputPattern.qml#L64) — bottom-left marker color

```qml
color: root.markerColors[(root.phase + 2) % 4]
```

This binding totals 0.98 ms across 268 evaluations, or roughly 0.004 ms each. No optimization is warranted. Converting these four explicit markers to a `Repeater` could reduce duplicated source, but would make a deterministic rendering test less direct without producing a meaningful runtime gain.

### 5. [TextureInputPattern.qml:56](../../Tests/TouchEngine/real/TextureInputPattern.qml#L56) — top-right marker color

```qml
color: root.markerColors[(root.phase + 1) % 4]
```

This binding totals 0.75 ms across the trace. Like the other marker bindings, it confirms that phase animation is reaching the scene, but its cost is negligible. The sustained frame-rate limitation lies outside QML binding and JavaScript execution.

## Next steps

1. Keep the test-pattern QML unchanged; its recurring work is too small to explain frame loss.
2. Use backend synchronization and resource telemetry for sustained performance investigations. The trace shows only 30.18 ms of all captured QML range work over the entire run.
3. Preserve the large-texture benchmark's before/after visual signatures so graphics optimizations cannot silently swap or reuse the wrong texture.
4. If the pattern grows substantially, consolidate repeated color calculations and run a focused QML profile again.
5. For a broader structural check of this single hotspot cluster, run `qt-qml-review` on `Library/Tests/TouchEngine/real/TextureInputPattern.qml`.

> AI assistance has been used to create this output.
