#ifndef DSWHITEBOARDCANVAS_H
#define DSWHITEBOARDCANVAS_H

#include <QQuickItem>
#include <QCanvasPainterItem>
#include <QCanvasPainterItemRenderer>
#include <QCanvasOffscreenCanvas>
#include <QCanvasImage>
#include <QLoggingCategory>
#include <QCanvasPath>
#include <QColor>
#include <QUrl>
#include <QMap>
#include <QtQml/qqmlregistration.h>

Q_DECLARE_LOGGING_CATEGORY(ds_whiteboard_lc)
Q_DECLARE_LOGGING_CATEGORY(ds_whiteboard_lcv)

struct DsWhiteboardTool {
    Q_GADGET
public:
    enum Type {
        UP,
        DOWN,
        MOVE,
        UNDO,
        REDO,
        CLEAR
    };
    enum Tool {
        Pen,
        Brush,
        Highlighter,
        Circle,
        Triangle,
        Square,
        Line,
        Star,
        Arrow,
        Eraser,
        None
    };
    Q_ENUM(Tool)
    Q_ENUM(Type)
    Type type = CLEAR;
    Tool tool = DsWhiteboardTool::None;
    QPointF firstPoint = {0,0};
    QPointF currentPoint = {0,0};
    double size = 10.0;
    QString color = "#000000";
    bool filled = false;
    double angle = 0.0; //rotation in degrees about the shape's centre (box shapes only)
    bool valid = false;
    int id = 0;
};

// Expose DsWhiteboardTool's enums to QML as a namespace, e.g. DsWhiteboardTool.Pen, DsWhiteboardTool.None.
namespace DsWhiteboardToolForeign {
    Q_NAMESPACE
    QML_NAMED_ELEMENT(DsWhiteboardTool)
    QML_FOREIGN_NAMESPACE(DsWhiteboardTool)
}

struct DsWhiteboardPath {
    QCanvasPath path;
    QList<QPointF> points; //raw freehand points (used to build filled brush/highlighter regions)
    DsWhiteboardTool::Tool tool = DsWhiteboardTool::None;
    double size = 10.0;
    QString color = "#000000";
    bool filled = false; //shapes: fill the path instead of stroking its outline
    int pathGroup = -1; //GPU cache bucket assigned on commit; -1 means uncached/dynamic
    //brush/highlighter only: filled regions built once on commit and GPU-cached so they
    //don't have to be rebuilt/re-tessellated every frame.
    QList<QCanvasPath> fillRegions;
    QList<int> fillGroups;       //GPU cache bucket per fill region
    QList<double> fillAlphas;    //alpha for each region (outer->inner ramp)
    bool regionsBuilt = false;   //true once the cached regions exist
};

typedef QList<DsWhiteboardPath> DsWhiteboardPathList;

class DsWhiteboardCanvasRenderer : public QCanvasPainterItemRenderer
{
public:
    DsWhiteboardCanvasRenderer(QObject *parent=nullptr,int undoBufferSize=1000);
    ~DsWhiteboardCanvasRenderer() override;
    // QCanvasPainterItemRenderer interface

protected:
    void initializeResources(QCanvasPainter *painter) override;
    void prePaint(QCanvasPainter *painter) override;
    void paint(QCanvasPainter *painter) override;
    void synchronizeData(QCanvasPainterItem *item) override;
private:
    void syncToolDown(DsWhiteboardTool &event,DsWhiteboardPathList &buffer);
    void syncToolUp(DsWhiteboardTool &event,DsWhiteboardPathList &buffer);
    void syncToolMove(DsWhiteboardTool &event,DsWhiteboardPathList &buffer);
    /* draw a single tool path with its tool-specific style: strokes brushes and open
       shapes, fills closed shapes when DsWhiteboardPath::filled is set. allowCache lets the
       caller force dynamic (uncached) rendering, e.g. when baking into the backing canvas. */
    void drawToolPath(QCanvasPainter *painter, const DsWhiteboardPath &toolPath, bool allowCache = true);
    void applyToolStyle(QCanvasPainter *painter, const DsWhiteboardPath &toolPath);
    /* build the geometry for a shape tool into path, fitted to rect. */
    static void buildShapePath(QCanvasPath &path, DsWhiteboardTool::Tool tool, const QRectF &rect);
    /* build a filled, self-unioned "thick line" region (capsules + round caps) of the given
       half width through points, so it can be filled once without translucency doubling. */
    static void buildThickPath(QCanvasPath &out, const QList<QPointF> &points, float halfWidth);
    /* build the soft-brush's layered fill regions (widest->narrowest) and the matching
       per-layer alphas that form the transparency ramp shaped by the brush members. */
    void buildBrushRegions(const QList<QPointF> &points, float size,
                           QList<QCanvasPath> &regions, QList<double> &alphas) const;
    /* build + cache the filled regions for a committed brush/highlighter stroke. */
    void buildFillRegions(DsWhiteboardPath &tp);
    /* queue a tool path's GPU cache buckets (stroke + fill regions) for removal. */
    void queueGroupRemoval(const DsWhiteboardPath &tp);
    /* move strokes older than the undo horizon into m_pendingBake. */
    void enforceUndoHorizon();
    /* the backing canvas holds the image of drawing that are beyond the undo buffer.*/
    QCanvasOffscreenCanvas* m_backingCanvas = nullptr; //this is the backing canvas
    QCanvasImage m_backingImage; //registered image used to draw m_backingCanvas in paint()
    QSize m_backingSize; //current backing canvas size; recreated when the item size changes
    DsWhiteboardPathList m_pendingBake; //finished strokes awaiting compositing into m_backingCanvas
    DsWhiteboardPathList m_toolPaths; //this is the undo buffer
    QMap<int,int> m_activePaths; //pointer id -> index into m_toolPaths
    QList<int> m_pendingGroupRemoval; //GPU cache buckets to free in prePaint()
    bool m_clearBacking = false; //request to wipe the backing canvas in prePaint()
    bool m_pathCachingEnabled = true; //mirrors DsWhiteboardCanvas::pathCaching
    //soft-brush transparency ramp (mirrors DsWhiteboardCanvas; sourced from WhiteboardTheme).
    double m_brushCenterAlpha = 1.0; //opacity at the centre of a brush stroke
    double m_brushFalloff = 0.9;     //ramp shape: >1 softer, 1 linear, <1 harder
    int m_brushMinLayers = 4;
    int m_brushMaxLayers = 12;
    int m_nextPathGroup = 0; //next GPU cache bucket id to hand out
    int m_undoHorizon = 100;
    int m_undoCursor = 0;
    qsizetype m_eventCursor = 0;
    quint64 m_revision = 0;
    QSizeF m_itemSize;

};


class DsWhiteboardCanvas : public QCanvasPainterItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged FINAL)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged FINAL)
    Q_PROPERTY(bool hasDrawing READ hasDrawing NOTIFY historyChanged FINAL)
    Q_PROPERTY(bool drawing READ drawing NOTIFY historyChanged FINAL)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged FINAL)
    Q_PROPERTY(DsWhiteboardTool::Tool tool READ tool WRITE setTool NOTIFY toolChanged FINAL)
    Q_PROPERTY(double size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(bool pathCaching READ pathCaching WRITE setPathCaching NOTIFY pathCachingChanged FINAL)
    //soft-brush transparency ramp (drive these from WhiteboardTheme).
    Q_PROPERTY(qreal brushCenterAlpha READ brushCenterAlpha WRITE setBrushCenterAlpha NOTIFY brushRampChanged FINAL)
    Q_PROPERTY(qreal brushFalloff READ brushFalloff WRITE setBrushFalloff NOTIFY brushRampChanged FINAL)
    Q_PROPERTY(int brushMinLayers READ brushMinLayers WRITE setBrushMinLayers NOTIFY brushRampChanged FINAL)
    Q_PROPERTY(int brushMaxLayers READ brushMaxLayers WRITE setBrushMaxLayers NOTIFY brushRampChanged FINAL)

public:
    explicit DsWhiteboardCanvas(QQuickItem *parent = nullptr);
    bool canUndo() const { return !drawing() && m_historyCursor > m_historyFloor; }
    bool canRedo() const { return !drawing() && m_historyCursor < m_historySize; }
    bool hasDrawing() const { return m_historyCursor > 0; }
    bool drawing() const { return !m_pointers.isEmpty(); }
    bool saving() const { return m_saving; }
    Q_INVOKABLE void finishStrokes();
    Q_INVOKABLE bool save(const QUrl &file, bool solidBackground = true,
                          const QColor &background = Qt::white);

    DsWhiteboardTool::Tool tool() const;
    void setTool(DsWhiteboardTool::Tool newTool);

    double size() const;
    void setSize(double newSize);

    QColor color() const;
    void setColor(const QColor &newColor);

    bool pathCaching() const;
    void setPathCaching(bool enabled);

    qreal brushCenterAlpha() const;
    void setBrushCenterAlpha(qreal a);
    qreal brushFalloff() const;
    void setBrushFalloff(qreal f);
    int brushMinLayers() const;
    void setBrushMinLayers(int n);
    int brushMaxLayers() const;
    void setBrushMaxLayers(int n);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    /* Remove all paths and clear the backing canvas. Not undoable. */
    Q_INVOKABLE void clear();
    /* Commit a finalized shape into the canvas (built and styled by the renderer). */
    Q_INVOKABLE void addShape(int tool, qreal x, qreal y, qreal w, qreal h,
                              const QColor &color, bool filled, qreal strokeWidth, qreal angle = 0.0);

signals:

    // QCanvasPainterItem interface
    void historyChanged();
    void savingChanged();
    void saved(const QUrl &file);
    void saveFailed(const QUrl &file, const QString &message);
    void toolChanged();
    void sizeChanged();
    void colorChanged();
    void pathCachingChanged();
    void brushRampChanged();

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;

private:
    friend class DsWhiteboardCanvasRenderer;
    static constexpr int MouseId = -1; // pointer id for mouse strokes; touch ids are >= 0
    static constexpr int ShapeId = -2; // pointer id for committed shapes
    void appendToolEvent(DsWhiteboardTool::Type type, int id, const QPointF &point, bool first);
    DsWhiteboardTool::Tool m_tool = DsWhiteboardTool::Pen;
    double m_size = 10.0;
    QColor m_color = QColor(Qt::black);
    bool m_pathCaching = true;
    double m_brushCenterAlpha = 1.0;
    double m_brushFalloff = 0.9;
    int m_brushMinLayers = 4;
    int m_brushMaxLayers = 12;
    // Retained CPU commands allow replay after resize or scene-graph recreation.
    // Only the new suffix is consumed during normal drawing. clear() releases it.
    QVector<DsWhiteboardTool> m_events;
    quint64 m_revision = 0;
    QMap<int, DsWhiteboardTool> m_pointers;
    int m_historyCursor = 0;
    int m_historySize = 0;
    int m_historyFloor = 0;
    bool m_saving = false;
    void commitHistory();


    // QQuickItem interface
protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void touchEvent(QTouchEvent *event) override;
    void mouseUngrabEvent() override;
    void touchUngrabEvent() override;
};



#endif // DSWHITEBOARDCANVAS_H
