#include "dsWhiteboardCanvas.h"
#include <QCanvasPath>
#include <QCanvasPainter>
#include <QCanvasOffscreenCanvas>
#include <QCanvasImage>
#include <QColor>
#include <QRectF>
#include <QPointF>
#include <QTransform>
#include <QMouseEvent>
#include <QTouchEvent>
#include <utility>
#include <cmath>
#include <algorithm>
#include <QQuickItemGrabResult>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QFileInfo>

Q_LOGGING_CATEGORY(ds_whiteboard_lc, "dsqt.whiteboard")
Q_LOGGING_CATEGORY(ds_whiteboard_lcv, "dsqt.whiteboard.verbose")

namespace {
constexpr float kHighlighterAlpha = 0.35f; //translucency of a highlighter stroke
}

DsWhiteboardCanvasRenderer::DsWhiteboardCanvasRenderer(QObject *parent, int undoBufferSize)
    : m_backingCanvas(nullptr)
    , m_undoHorizon(undoBufferSize)
{
    setSharedPainter(false);
    Q_UNUSED(parent) //QCanvasPainterItemRenderer is not a QObject; no parent to forward
}

DsWhiteboardCanvasRenderer::~DsWhiteboardCanvasRenderer()
{
    //m_backingCanvas only wraps a handle; the painter owns and frees the actual GPU
    //resources during its own destruction. Just release our handle wrapper.
    delete m_backingCanvas;
}

void DsWhiteboardCanvasRenderer::initializeResources(QCanvasPainter *painter)
{

}

void DsWhiteboardCanvasRenderer::prePaint(QCanvasPainter *painter)
{
    //Ensure the backing canvas exists and matches the item size. The backing canvas
    //holds strokes that scrolled past the undo horizon and can no longer be undone.
    const QSize size(qMax(1, int(width())), qMax(1, int(height())));
    if(m_backingCanvas == nullptr || m_backingSize != size){
        if(m_backingCanvas != nullptr){
            // synchronizeData() replays retained commands when the item size changes.
            painter->destroyCanvas(*m_backingCanvas);
            delete m_backingCanvas;
            m_backingCanvas = nullptr;
        }
        m_backingCanvas = new QCanvasOffscreenCanvas(
            painter->createCanvas(size, 1, QCanvasOffscreenCanvas::Flag::PreserveContents));
        m_backingSize = size;
        m_backingImage = painter->addImage(*m_backingCanvas);
    }

    //A clear() wipes the backing canvas back to transparent.
    if(m_clearBacking){
        beginCanvasPainting(*m_backingCanvas);
        painter->clearRect(0.0f, 0.0f, float(size.width()), float(size.height()));
        endCanvasPainting();
        m_backingImage = painter->addImage(*m_backingCanvas);
        m_clearBacking = false;
    }

    //Composite any strokes that fell beyond the undo horizon into the backing canvas.
    //This is the only place where painting into an offscreen canvas is allowed.
    if(!m_pendingBake.isEmpty()){
        beginCanvasPainting(*m_backingCanvas);
        for(const DsWhiteboardPath &toolPath : std::as_const(m_pendingBake)){
            //bake once with no caching; the stroke's own cache (if any) is freed below.
            drawToolPath(painter, toolPath, false);
        }
        endCanvasPainting();
        m_pendingBake.clear();
        //Refresh the registered image so paint() draws the updated contents.
        m_backingImage = painter->addImage(*m_backingCanvas);
    }

    //Free GPU cache buckets for strokes that have left the undo buffer.
    if(!m_pendingGroupRemoval.isEmpty()){
        for(int group : std::as_const(m_pendingGroupRemoval)){
            painter->removePathGroup(group);
        }
        m_pendingGroupRemoval.clear();
    }
}

void DsWhiteboardCanvasRenderer::paint(QCanvasPainter *painter)
{
    //Draw the permanent backing layer first...
    if(m_backingCanvas != nullptr && !m_backingImage.isNull()){
        painter->drawImage(m_backingImage, 0.0f, 0.0f);
    }

    //...then the undoable window of strokes on top, up to the undo cursor.
    const int count = qMin(m_undoCursor, int(m_toolPaths.size()));
    for(int i = 0; i < count; ++i){
        drawToolPath(painter, m_toolPaths.at(i));
    }
}

void DsWhiteboardCanvasRenderer::synchronizeData(QCanvasPainterItem *item)
{
    auto whiteboard = qobject_cast<DsWhiteboardCanvas*>(item);
    if(whiteboard == nullptr){
        return;
    }

    m_pathCachingEnabled = whiteboard->m_pathCaching;
    m_brushCenterAlpha = whiteboard->m_brushCenterAlpha;
    m_brushFalloff = whiteboard->m_brushFalloff;
    m_brushMinLayers = whiteboard->m_brushMinLayers;
    m_brushMaxLayers = whiteboard->m_brushMaxLayers;

    const QSizeF size(whiteboard->width(), whiteboard->height());
    if (m_revision != whiteboard->m_revision || m_itemSize != size) {
        for (const auto &path : std::as_const(m_toolPaths)) queueGroupRemoval(path);
        m_toolPaths.clear();
        m_activePaths.clear();
        m_pendingBake.clear();
        m_undoCursor = 0;
        m_eventCursor = 0;
        m_clearBacking = true;
        m_revision = whiteboard->m_revision;
        m_itemSize = size;
    }
    const auto &events = whiteboard->m_events;

    //run through the events and make paths.
    for (; m_eventCursor < events.size(); ++m_eventCursor) {
        auto event = events.at(m_eventCursor);
        switch(event.type){
        case DsWhiteboardTool::DOWN:
            syncToolDown(event,m_toolPaths);
            break;
        case DsWhiteboardTool::UP:
            syncToolUp(event,m_toolPaths);
            break;
        case DsWhiteboardTool::MOVE:
            syncToolMove(event,m_toolPaths);
            break;
        case DsWhiteboardTool::UNDO:
            if(m_undoCursor > 0){
                m_undoCursor--;
            }
            break;
        case DsWhiteboardTool::REDO:
            if(m_undoCursor < int(m_toolPaths.size())){
                m_undoCursor++;
            }
            break;
        case DsWhiteboardTool::CLEAR:
            //free every cached path group, drop all paths, and wipe the backing canvas.
            for(const DsWhiteboardPath &tp : std::as_const(m_toolPaths)){
                queueGroupRemoval(tp);
            }
            m_toolPaths.clear();
            m_activePaths.clear();
            m_pendingBake.clear();
            m_undoCursor = 0;
            m_clearBacking = true;
            break;
        }
        // Match GUI history: commit only when all concurrent strokes have ended.
        if (m_activePaths.isEmpty()) enforceUndoHorizon();
    }
}

void DsWhiteboardCanvasRenderer::syncToolDown(DsWhiteboardTool& event,DsWhiteboardPathList& buffer){
    //if there is no tool, do nothing.
    if(event.tool == DsWhiteboardTool::None) return;

    //starting a new stroke discards any redo history (strokes after the cursor).
    while(int(buffer.size()) > m_undoCursor){
        queueGroupRemoval(buffer.last()); //free the discarded stroke's GPU cache buckets.
        buffer.removeLast();
    }
    //drop tracking for any path that lived in the discarded redo tail.
    for(auto it = m_activePaths.begin(); it != m_activePaths.end(); ){
        if(it.value() >= m_undoCursor){
            it = m_activePaths.erase(it);
        } else {
            ++it;
        }
    }

    //add the brushsize and color from the event.
    DsWhiteboardPath nextPath;
    nextPath.color = event.color;
    nextPath.size = event.size;
    nextPath.tool = event.tool;
    nextPath.filled = event.filled;
    buffer.push_back(nextPath);

    //track the new path by its index in the buffer. inserting replaces any path
    //previously tracked for this pointer id. the new stroke is now the newest
    //applied stroke, so advance the undo cursor to include it.
    //TODO: indices are only stable while the buffer is append-only; the bake step in
    //enforceUndoHorizon() compensates by shifting tracked indices when it pops the front.
    const int index = int(buffer.size()) - 1;
    m_activePaths.insert(event.id,index);
    m_undoCursor = int(buffer.size());

    //edit through the buffer element, not the local copy that was pushed.
    DsWhiteboardPath& path = buffer[index];

    switch(event.tool){
    case DsWhiteboardTool::Pen:
    case DsWhiteboardTool::Brush:
    case DsWhiteboardTool::Highlighter:
    case DsWhiteboardTool::Eraser:
        //the eraser is a freehand stroke too; drawToolPath() composites it as a cutout.
        path.path.moveTo(event.currentPoint.x()-1,event.currentPoint.y());
        path.path.lineTo(event.currentPoint);
        path.points.append(event.currentPoint);
        break;
    case DsWhiteboardTool::Line:
    case DsWhiteboardTool::Arrow:
        //linear shapes are directional: keep firstPoint -> currentPoint (no normalize).
        buildShapePath(path.path, event.tool, QRectF(event.firstPoint, event.currentPoint));
        path.path.squeeze();
        break;
    case DsWhiteboardTool::Circle:
    case DsWhiteboardTool::Triangle:
    case DsWhiteboardTool::Square:
    case DsWhiteboardTool::Star: {
        //box shapes are defined by a bounding rect (opposite corners), optionally rotated
        //about the rect centre.
        const QRectF rect = QRectF(event.firstPoint, event.currentPoint).normalized();
        QCanvasPath shape;
        buildShapePath(shape, event.tool, rect);
        QTransform t;
        if(!qFuzzyIsNull(event.angle)){
            const QPointF c = rect.center();
            t.translate(c.x(), c.y());
            t.rotate(event.angle);
            t.translate(-c.x(), -c.y());
        }
        path.path.addPath(shape, t);
        path.path.squeeze();
        break;
    }
    case DsWhiteboardTool::None:
        break;
    }
}

void DsWhiteboardCanvasRenderer::syncToolUp(DsWhiteboardTool& event,DsWhiteboardPathList& buffer){
    if(!m_activePaths.contains(event.id)){
        qCWarning(ds_whiteboard_lc)<<"Up event but was not tracking a path for the pointer id";
        return;
    }
    const int index = m_activePaths.value(event.id,-1);
    //the stroke is finished; stop tracking this pointer id.
    m_activePaths.remove(event.id);
    if(index < 0 || index >= buffer.size()){
        return;
    }

    //if there is no tool, do nothing.
    if(event.tool == DsWhiteboardTool::None) return;

    DsWhiteboardPath& currentPath = buffer[index];

    switch(event.tool){
    case DsWhiteboardTool::Pen:
    case DsWhiteboardTool::Brush:
    case DsWhiteboardTool::Highlighter:
    case DsWhiteboardTool::Eraser:
        currentPath.path.lineTo(event.currentPoint);
        currentPath.points.append(event.currentPoint);
        currentPath.path.squeeze();
        break;
    case DsWhiteboardTool::Circle:
    case DsWhiteboardTool::Triangle:
    case DsWhiteboardTool::Square:
    case DsWhiteboardTool::Line:
    case DsWhiteboardTool::Star:
    case DsWhiteboardTool::Arrow:
    case DsWhiteboardTool::None:
        break;
    }

    //the stroke is final now. Brush/highlighter build their filled regions once and cache
    //them (one GPU bucket per region); everything else caches its single stroke path.
    if(event.tool == DsWhiteboardTool::Brush || event.tool == DsWhiteboardTool::Highlighter){
        buildFillRegions(currentPath);
        for(int i = 0; i < currentPath.fillRegions.size(); ++i){
            currentPath.fillGroups.append(m_nextPathGroup++);
        }
    } else {
        currentPath.pathGroup = m_nextPathGroup++;
    }
}

void DsWhiteboardCanvasRenderer::syncToolMove(DsWhiteboardTool& event,DsWhiteboardPathList& buffer){
    if(!m_activePaths.contains(event.id)){
        qCWarning(ds_whiteboard_lc)<<"Move event but was not tracking a path for the pointer id";
        return;
    }
    const int index = m_activePaths.value(event.id,-1);
    if(index < 0 || index >= buffer.size()){
        return;
    }

    //if there is no tool, do nothing.
    if(event.tool == DsWhiteboardTool::None) return;

    DsWhiteboardPath& currentPath = buffer[index];

    switch(event.tool){
    case DsWhiteboardTool::Pen:
    case DsWhiteboardTool::Brush:
    case DsWhiteboardTool::Highlighter:
    case DsWhiteboardTool::Eraser:
        currentPath.path.lineTo(event.currentPoint);
        currentPath.points.append(event.currentPoint);
        break;
    case DsWhiteboardTool::Circle:
    case DsWhiteboardTool::Triangle:
    case DsWhiteboardTool::Square:
    case DsWhiteboardTool::Line:
    case DsWhiteboardTool::Star:
    case DsWhiteboardTool::Arrow:
    case DsWhiteboardTool::None:
        break;
    }
}

void DsWhiteboardCanvasRenderer::applyToolStyle(QCanvasPainter *painter, const DsWhiteboardPath &toolPath)
{
    //color is applied by drawToolPath (stroke vs fill); here we set geometry style only.
    painter->setLineWidth(static_cast<float>(toolPath.size));
    painter->setLineCap(QCanvasPainter::LineCap::Round);
    painter->setLineJoin(QCanvasPainter::LineJoin::Round);

    switch(toolPath.tool){
    case DsWhiteboardTool::Pen:
        //crisp line: minimal antialiasing, fully opaque.
        painter->setAntialias(2.5f);
        painter->setGlobalAlpha(1.0f);
        break;
    case DsWhiteboardTool::Brush:
        //soft line: heavy antialiasing feathers the edges.
        painter->setAntialias(2.5f);
        painter->setGlobalAlpha(1.0f);
        break;
    case DsWhiteboardTool::Highlighter:
        //like the pen but translucent and chisel-tipped so overlaps build up.
        painter->setAntialias(2.5f);
        painter->setLineCap(QCanvasPainter::LineCap::Square);
        painter->setGlobalAlpha(0.35f);
        break;
    default:
        painter->setAntialias(1.0f);
        painter->setGlobalAlpha(1.0f);
        break;
    }
}

void DsWhiteboardCanvasRenderer::drawToolPath(QCanvasPainter *painter, const DsWhiteboardPath &toolPath, bool allowCache)
{
    if(toolPath.tool == DsWhiteboardTool::None || toolPath.path.isEmpty()){
        return;
    }
    painter->save();
    applyToolStyle(painter, toolPath);
    //The eraser is composited as a cutout: DestinationOut removes existing content where
    //the stroke overlaps, revealing whatever sits behind the (transparent) whiteboard.
    if(toolPath.tool == DsWhiteboardTool::Eraser){
        painter->setGlobalCompositeOperation(QCanvasPainter::CompositeOperation::DestinationOut);
        painter->setGlobalAlpha(1.0f);
        painter->setStrokeStyle(QColor(Qt::black)); //opaque source -> full erase
        //caching the cutout geometry is fine; it still composites with DestinationOut.
        const int eraseGroup = (allowCache && m_pathCachingEnabled && toolPath.pathGroup >= 0)
                                   ? toolPath.pathGroup : -1;
        painter->stroke(toolPath.path, eraseGroup);
        painter->restore();
        return;
    }
    //Brush & highlighter render as filled, self-unioned regions so translucency stays
    //uniform (no "doubling" where the stroke overlaps itself). Committed strokes reuse
    //their cached regions/GPU buffers; only the in-progress stroke rebuilds each frame.
    if(toolPath.tool == DsWhiteboardTool::Highlighter || toolPath.tool == DsWhiteboardTool::Brush){
        painter->setGlobalCompositeOperation(QCanvasPainter::CompositeOperation::SourceOver);
        painter->setFillStyle(QColor(toolPath.color));
        painter->setAntialias(1.5f);
        if(toolPath.regionsBuilt){
            for(int i = 0; i < toolPath.fillRegions.size(); ++i){
                const int g = (allowCache && m_pathCachingEnabled && i < toolPath.fillGroups.size())
                                  ? toolPath.fillGroups.at(i) : -1;
                painter->setGlobalAlpha(float(toolPath.fillAlphas.at(i)));
                painter->fill(toolPath.fillRegions.at(i), g);
            }
        } else if(toolPath.tool == DsWhiteboardTool::Highlighter){
            QCanvasPath region;
            buildThickPath(region, toolPath.points, float(toolPath.size) / 2.0f);
            if(!region.isEmpty()){
                painter->setGlobalAlpha(kHighlighterAlpha);
                painter->fill(region, -1);
            }
        } else { //in-progress brush
            QList<QCanvasPath> regions;
            QList<double> regionAlphas;
            buildBrushRegions(toolPath.points, float(toolPath.size), regions, regionAlphas);
            for(int i = 0; i < regions.size(); ++i){
                painter->setGlobalAlpha(float(regionAlphas.at(i)));
                painter->fill(regions.at(i), -1);
            }
        }
        painter->restore();
        return;
    }
    painter->setGlobalCompositeOperation(QCanvasPainter::CompositeOperation::SourceOver);
    //pathGroup >= 0 caches the path's geometry in a GPU buffer; -1 renders dynamically
    //(the shared dynamic buffer). Active strokes carry pathGroup -1 and so never cache.
    const int group = (allowCache && m_pathCachingEnabled && toolPath.pathGroup >= 0)
                          ? toolPath.pathGroup
                          : -1;
    if(toolPath.filled){
        painter->setFillStyle(QColor(toolPath.color));
        painter->fill(toolPath.path, group);
    } else {
        painter->setStrokeStyle(QColor(toolPath.color));
        painter->stroke(toolPath.path, group);
    }
    painter->restore();
}

void DsWhiteboardCanvasRenderer::buildThickPath(QCanvasPath &out, const QList<QPointF> &points, float halfWidth)
{
    if(points.isEmpty() || halfWidth <= 0.0f){
        return;
    }
    //Drop near-coincident samples so we don't get zero-length segments.
    QList<QPointF> p;
    p.reserve(points.size());
    for(const QPointF &q : points){
        if(p.isEmpty() || std::hypot(q.x() - p.last().x(), q.y() - p.last().y()) > 0.5)
            p.append(q);
    }
    //A single point is just a round dab.
    if(p.size() == 1){
        out.beginSolidSubPath();
        out.circle(float(p[0].x()), float(p[0].y()), halfWidth);
        return;
    }

    const int n = p.size();
    const auto unit = [](double x, double y) -> QPointF {
        const double l = std::hypot(x, y);
        return (l > 1e-6) ? QPointF(x / l, y / l) : QPointF(0.0, 0.0);
    };

    //Per-point outward normal = perpendicular of the averaged (bisector) tangent. This
    //keeps the two offset sides smooth through curves without per-vertex discs.
    QList<QPointF> nrm;
    nrm.reserve(n);
    for(int i = 0; i < n; ++i){
        const QPointF dPrev = (i > 0) ? unit(p[i].x() - p[i - 1].x(), p[i].y() - p[i - 1].y()) : QPointF(0, 0);
        const QPointF dNext = (i < n - 1) ? unit(p[i + 1].x() - p[i].x(), p[i + 1].y() - p[i].y()) : QPointF(0, 0);
        QPointF tang = unit(dPrev.x() + dNext.x(), dPrev.y() + dNext.y());
        if(tang.isNull())
            tang = (i < n - 1) ? dNext : dPrev;
        nrm.append(QPointF(-tang.y(), tang.x()));
    }

    //Build ONE outline polygon: forward along one offset side, back along the other.
    //A single non-overlapping subpath avoids the antialiasing seams that a union of many
    //quads/discs produced (the "bumps").
    out.beginSolidSubPath();
    out.moveTo(float(p[0].x() + nrm[0].x() * halfWidth), float(p[0].y() + nrm[0].y() * halfWidth));
    for(int i = 1; i < n; ++i)
        out.lineTo(float(p[i].x() + nrm[i].x() * halfWidth), float(p[i].y() + nrm[i].y() * halfWidth));
    for(int i = n - 1; i >= 0; --i)
        out.lineTo(float(p[i].x() - nrm[i].x() * halfWidth), float(p[i].y() - nrm[i].y() * halfWidth));
    out.closePath();

    //Round caps at the two ends only (two small discs -> negligible seams at the tips).
    out.beginSolidSubPath();
    out.circle(float(p[0].x()), float(p[0].y()), halfWidth);
    out.beginSolidSubPath();
    out.circle(float(p[n - 1].x()), float(p[n - 1].y()), halfWidth);
}

void DsWhiteboardCanvasRenderer::buildBrushRegions(const QList<QPointF> &points, float size,
                                              QList<QCanvasPath> &regions, QList<double> &alphas) const
{
    //Soft brush: concentric filled regions from full width (outer) to a small core (centre),
    //drawn outer-first. Each layer gets its OWN alpha so the cumulative (SourceOver) coverage
    //follows a target profile: ~0 at the edge, m_brushCenterAlpha at the centre. m_brushFalloff
    //shapes that profile (>1 = softer/wider faded skirt, 1 = linear, <1 = harder edge), which
    //decouples centre opacity from edge softness (centre can be fully opaque with soft edges).
    regions.clear();
    alphas.clear();
    const int minLayers = qMax(1, m_brushMinLayers);
    const int maxLayers = qMax(minLayers, m_brushMaxLayers);
    const int layers = std::clamp(int(size / 4.0f) + 2, minLayers, maxLayers);
    double prevCum = 0.0; //cumulative coverage built up by the layers drawn so far
    for(int i = 0; i < layers; ++i){
        const double f = double(i + 1) / double(layers);          //(0,1], 1 at the centre
        const double targetCum = m_brushCenterAlpha * std::pow(f, m_brushFalloff);
        double a = (1.0 - prevCum > 1e-6) ? (targetCum - prevCum) / (1.0 - prevCum) : 0.0;
        a = std::clamp(a, 0.0, 1.0);
        prevCum = targetCum;
        const float w = size * float(1.0 - double(i) / double(layers)); //widest -> narrowest
        if(w < 0.75f || a <= 0.003) continue;
        QCanvasPath r;
        buildThickPath(r, points, w / 2.0f);
        if(r.isEmpty()) continue;
        r.squeeze();
        regions.append(r);
        alphas.append(a);
    }
}

void DsWhiteboardCanvasRenderer::buildFillRegions(DsWhiteboardPath &tp)
{
    tp.fillRegions.clear();
    tp.fillAlphas.clear();
    if(tp.tool == DsWhiteboardTool::Highlighter){
        QCanvasPath r;
        buildThickPath(r, tp.points, float(tp.size) / 2.0f);
        if(!r.isEmpty()){
            r.squeeze();
            tp.fillRegions.append(r);
            tp.fillAlphas.append(kHighlighterAlpha);
        }
    } else if(tp.tool == DsWhiteboardTool::Brush){
        buildBrushRegions(tp.points, float(tp.size), tp.fillRegions, tp.fillAlphas);
    }
    tp.regionsBuilt = true;
}

void DsWhiteboardCanvasRenderer::queueGroupRemoval(const DsWhiteboardPath &tp)
{
    if(tp.pathGroup >= 0){
        m_pendingGroupRemoval.append(tp.pathGroup);
    }
    for(int g : tp.fillGroups){
        if(g >= 0){
            m_pendingGroupRemoval.append(g);
        }
    }
}

void DsWhiteboardCanvasRenderer::buildShapePath(QCanvasPath &path, DsWhiteboardTool::Tool tool, const QRectF &r)
{
    constexpr float kPi = 3.14159265358979f;
    const float x = float(r.x()), y = float(r.y());
    const float w = float(r.width()), h = float(r.height());
    const float cx = x + w / 2.0f, cy = y + h / 2.0f;

    switch(tool){
    case DsWhiteboardTool::Square:
        path.rect(x, y, w, h);
        break;
    case DsWhiteboardTool::Circle:
        path.ellipse(QRectF(x, y, w, h));
        break;
    case DsWhiteboardTool::Triangle:
        path.moveTo(cx, y);
        path.lineTo(x + w, y + h);
        path.lineTo(x, y + h);
        path.closePath();
        break;
    case DsWhiteboardTool::Line:
        //directional: (x, y) is the start, (x + w, y + h) the end (w/h may be negative).
        path.moveTo(x, y);
        path.lineTo(x + w, y + h);
        break;
    case DsWhiteboardTool::Star: {
        const float rOuter = std::min(w, h) / 2.0f;
        const float rInner = rOuter * 0.45f;
        for(int i = 0; i < 10; ++i){
            const float ang = -kPi / 2.0f + i * kPi / 5.0f;
            const float rr = (i % 2 == 0) ? rOuter : rInner;
            const float px = cx + rr * std::cos(ang);
            const float py = cy + rr * std::sin(ang);
            if(i == 0) path.moveTo(px, py); else path.lineTo(px, py);
        }
        path.closePath();
        break;
    }
    case DsWhiteboardTool::Arrow: {
        //directional: shaft from start (x, y) to end (x + w, y + h), head at the end.
        const float ex = x + w, ey = y + h;
        const float len = std::hypot(w, h);
        const float head = std::min(len * 0.3f, 36.0f);
        const float ang = std::atan2(h, w);
        const float a1 = ang - 0.5f, a2 = ang + 0.5f; //~28 degrees off the shaft
        path.moveTo(x, y);
        path.lineTo(ex, ey);
        path.moveTo(ex - head * std::cos(a1), ey - head * std::sin(a1));
        path.lineTo(ex, ey);
        path.lineTo(ex - head * std::cos(a2), ey - head * std::sin(a2));
        break;
    }
    default:
        break;
    }
}

void DsWhiteboardCanvasRenderer::enforceUndoHorizon()
{
    //Strokes older than the horizon can no longer be undone; queue them to be baked into
    //the backing canvas (done in prePaint(), the only place we can paint into it).
    while(m_undoCursor > m_undoHorizon && !m_toolPaths.isEmpty()){
        //Never bake a stroke that is still being drawn (front index is tracked/active).
        bool frontActive = false;
        for(int idx : std::as_const(m_activePaths)){
            if(idx == 0){
                frontActive = true;
                break;
            }
        }
        if(frontActive){
            break;
        }

        DsWhiteboardPath baked = m_toolPaths.takeFirst();
        //free the baked stroke's GPU cache buckets once it has been composited.
        queueGroupRemoval(baked);
        m_pendingBake.push_back(baked);
        m_undoCursor--;
        //Removing the front shifts every tracked index down by one.
        for(auto it = m_activePaths.begin(); it != m_activePaths.end(); ++it){
            it.value() -= 1;
        }
    }
}

DsWhiteboardCanvas::DsWhiteboardCanvas(QQuickItem *parent)
    : QCanvasPainterItem(parent)
{
    setAlphaBlending(true);
    setFillColor(Qt::transparent);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
}

DsWhiteboardTool::Tool DsWhiteboardCanvas::tool() const
{
    return m_tool;
}

void DsWhiteboardCanvas::setTool(DsWhiteboardTool::Tool newTool)
{
    if (m_tool == newTool)
        return;
    m_tool = newTool;
    emit toolChanged();
}

double DsWhiteboardCanvas::size() const
{
    return m_size;
}

void DsWhiteboardCanvas::setSize(double newSize)
{
    if (!std::isfinite(newSize) || newSize <= 0 || qFuzzyCompare(m_size, newSize))
        return;
    m_size = newSize;
    emit sizeChanged();
}

QColor DsWhiteboardCanvas::color() const
{
    return m_color;
}

void DsWhiteboardCanvas::setColor(const QColor &newColor)
{
    if (!newColor.isValid() || m_color == newColor)
        return;
    m_color = newColor;
    emit colorChanged();
}

bool DsWhiteboardCanvas::pathCaching() const
{
    return m_pathCaching;
}

void DsWhiteboardCanvas::setPathCaching(bool enabled)
{
    if (m_pathCaching == enabled)
        return;
    m_pathCaching = enabled;
    emit pathCachingChanged();
    //repaint so existing strokes switch between cached and dynamic rendering.
    update();
}

qreal DsWhiteboardCanvas::brushCenterAlpha() const { return m_brushCenterAlpha; }
void DsWhiteboardCanvas::setBrushCenterAlpha(qreal a)
{
    if (!std::isfinite(a)) return;
    a = std::clamp(a, qreal(0), qreal(1));
    if (qFuzzyCompare(m_brushCenterAlpha, a)) return;
    m_brushCenterAlpha = a;
    emit brushRampChanged();
    update();
}

qreal DsWhiteboardCanvas::brushFalloff() const { return m_brushFalloff; }
void DsWhiteboardCanvas::setBrushFalloff(qreal f)
{
    if (!std::isfinite(f) || f <= 0) return;
    if (qFuzzyCompare(m_brushFalloff, f)) return;
    m_brushFalloff = f;
    emit brushRampChanged();
    update();
}

int DsWhiteboardCanvas::brushMinLayers() const { return m_brushMinLayers; }
void DsWhiteboardCanvas::setBrushMinLayers(int n)
{
    n = std::clamp(n, 1, 64);
    if (m_brushMinLayers == n) return;
    m_brushMinLayers = n;
    emit brushRampChanged();
    update();
}

int DsWhiteboardCanvas::brushMaxLayers() const { return m_brushMaxLayers; }
void DsWhiteboardCanvas::setBrushMaxLayers(int n)
{
    n = std::clamp(n, 1, 64);
    if (m_brushMaxLayers == n) return;
    m_brushMaxLayers = n;
    emit brushRampChanged();
    update();
}

void DsWhiteboardCanvas::undo()
{
    if (!canUndo()) return;
    --m_historyCursor;
    emit historyChanged();
    DsWhiteboardTool te;
    te.type = DsWhiteboardTool::UNDO;
    te.valid = true;
    m_events.append(te);
    update();
}

void DsWhiteboardCanvas::redo()
{
    if (!canRedo()) return;
    ++m_historyCursor;
    emit historyChanged();
    DsWhiteboardTool te;
    te.type = DsWhiteboardTool::REDO;
    te.valid = true;
    m_events.append(te);
    update();
}

void DsWhiteboardCanvas::clear()
{
    m_pointers.clear();
    m_events.clear();
    ++m_revision;
    m_historyCursor = m_historySize = m_historyFloor = 0;
    emit historyChanged();
    DsWhiteboardTool te;
    te.type = DsWhiteboardTool::CLEAR;
    te.valid = true;
    m_events.append(te);
    update();
}

void DsWhiteboardCanvas::addShape(int tool, qreal x, qreal y, qreal w, qreal h,
                             const QColor &color, bool filled, qreal strokeWidth, qreal angle)
{
    if (tool < DsWhiteboardTool::Circle || tool > DsWhiteboardTool::Arrow
        || !color.isValid() || !std::isfinite(strokeWidth) || strokeWidth <= 0
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w)
        || !std::isfinite(h) || !std::isfinite(angle)) return;
    ++m_historyCursor;
    m_historySize = m_historyCursor;
    commitHistory();
    //A finalized shape is committed as a single path: a DOWN builds the geometry from
    //the two corners, the UP finalizes it (assigns a cache group) like a normal stroke.
    DsWhiteboardTool down;
    down.type = DsWhiteboardTool::DOWN;
    down.tool = static_cast<DsWhiteboardTool::Tool>(tool);
    down.firstPoint = QPointF(x, y);
    down.currentPoint = QPointF(x + w, y + h);
    down.color = color.name(QColor::HexArgb);
    down.size = strokeWidth;
    down.filled = filled;
    down.angle = angle;
    down.id = ShapeId;
    down.valid = true;
    m_events.append(down);

    DsWhiteboardTool up = down;
    up.type = DsWhiteboardTool::UP;
    m_events.append(up);

    update();
}

QCanvasPainterItemRenderer *DsWhiteboardCanvas::createItemRenderer() const
{
    //Keep the live (un-baked) window modest: strokes beyond this bake into the backing
    //image and cost nothing per frame. This bounds per-frame work (and is the undo depth).
    return new DsWhiteboardCanvasRenderer(nullptr, 64);
}

void DsWhiteboardCanvas::commitHistory()
{
    if (!drawing()) m_historyFloor = qMax(m_historyFloor, m_historyCursor - 64);
    emit historyChanged();
}

void DsWhiteboardCanvas::appendToolEvent(DsWhiteboardTool::Type type, int id,
                                        const QPointF &point, bool first)
{
    if (first) {
        if (m_tool == DsWhiteboardTool::None || m_pointers.contains(id)) return;
        DsWhiteboardTool event;
        event.tool = m_tool;
        event.size = m_size;
        event.color = m_color.name(QColor::HexArgb);
        event.firstPoint = point;
        event.id = id;
        event.valid = true;
        m_pointers.insert(id, event);
        ++m_historyCursor;
        m_historySize = m_historyCursor;
    }
    if (!m_pointers.contains(id)) return;
    auto &event = m_pointers[id];
    event.type = type;
    event.currentPoint = point;
    m_events.append(event);
    if (type == DsWhiteboardTool::UP) m_pointers.remove(id);
    commitHistory();
    update();
}

void DsWhiteboardCanvas::finishStrokes()
{
    const auto pointers = m_pointers;
    for (auto it = pointers.cbegin(); it != pointers.cend(); ++it)
        appendToolEvent(DsWhiteboardTool::UP, it.key(), it->currentPoint, false);
}

void DsWhiteboardCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_tool == DsWhiteboardTool::None) { event->ignore(); return; }
    appendToolEvent(DsWhiteboardTool::DOWN, MouseId, event->position(), true);
    event->accept();
}

void DsWhiteboardCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pointers.contains(MouseId)) { event->ignore(); return; }
    appendToolEvent(DsWhiteboardTool::MOVE, MouseId, event->position(), false);
    event->accept();
}

void DsWhiteboardCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_pointers.contains(MouseId)) { event->ignore(); return; }
    appendToolEvent(DsWhiteboardTool::UP, MouseId, event->position(), false);
    event->accept();
}

void DsWhiteboardCanvas::mouseUngrabEvent()
{
    if (m_pointers.contains(MouseId))
        appendToolEvent(DsWhiteboardTool::UP, MouseId, m_pointers[MouseId].currentPoint, false);
}

void DsWhiteboardCanvas::touchUngrabEvent() { finishStrokes(); }

void DsWhiteboardCanvas::touchEvent(QTouchEvent *event)
{
    if (event->type() == QEvent::TouchCancel) {
        finishStrokes();
        event->accept();
        return;
    }
    bool handled = false;
    for (const auto &point : event->points()) {
        DsWhiteboardTool::Type type;
        switch (point.state()) {
        case QEventPoint::Pressed:
            if (m_tool == DsWhiteboardTool::None) continue;
            type = DsWhiteboardTool::DOWN;
            break;
        case QEventPoint::Updated: type = DsWhiteboardTool::MOVE; break;
        case QEventPoint::Released: type = DsWhiteboardTool::UP; break;
        default: continue;
        }
        if (type != DsWhiteboardTool::DOWN && !m_pointers.contains(point.id())) continue;
        appendToolEvent(type, point.id(), point.position(), type == DsWhiteboardTool::DOWN);
        handled = true;
    }
    event->setAccepted(handled);
}

bool DsWhiteboardCanvas::save(const QUrl &file, bool solidBackground, const QColor &background)
{
    if (m_saving || drawing() || !isVisible() || !window() || !file.isLocalFile()) {
        emit saveFailed(file, tr("The drawing is not ready to save, or the destination is not a local file."));
        return false;
    }
    if (QFileInfo(file.toLocalFile()).suffix().compare("png", Qt::CaseInsensitive) != 0) {
        emit saveFailed(file, tr("Choose a PNG file for the whiteboard image."));
        return false;
    }
    const QSize outputSize(qRound(width()), qRound(height()));
    auto result = grabToImage();
    if (!result) {
        emit saveFailed(file, tr("Could not capture the drawing."));
        return false;
    }
    m_saving = true;
    emit savingChanged();
    connect(result.data(), &QQuickItemGrabResult::ready, this,
            [this, result, file, solidBackground, background, outputSize] {
        QImage image = result->image();
        if (image.size() != outputSize)
            image = image.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        image.setDevicePixelRatio(1.0);
        if (solidBackground && !image.isNull()) {
            QImage composite(image.size(), QImage::Format_ARGB32_Premultiplied);
            composite.fill(background);
            QPainter painter(&composite);
            painter.drawImage(0, 0, image);
            painter.end();
            image = composite;
        }
        QSaveFile output(file.toLocalFile());
        const bool success = !image.isNull() && output.open(QIODevice::WriteOnly)
                             && image.save(&output, "PNG") && output.commit();
        m_saving = false;
        emit savingChanged();
        if (success) emit saved(file);
        else emit saveFailed(file, tr("Could not save the PNG image. Check the destination and available space."));
    }, Qt::SingleShotConnection);
    return true;
}
