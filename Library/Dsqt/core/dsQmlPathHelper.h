#ifndef DSQMLPATHHELPER_H
#define DSQMLPATHHELPER_H

#include <QObject>
#include <QPointF>

namespace dsqt {

class DsQmlPathHelper : public QObject {
    Q_OBJECT

  public:
    explicit DsQmlPathHelper(QObject* parent = nullptr)
        : QObject(parent) {}

    Q_INVOKABLE QString circle(float cx, float cy, float r) {
        return QString("M%1 %2m%3 0a%3 %3 0 1 0 -%4 0a%3 %3 0 1 0 %4 0").arg(cx).arg(cy).arg(r).arg(2 * r);
    }
    Q_INVOKABLE QString ellipse(float cx, float cy, float rx, float ry) {
        return QString("M%1 %2m%3 0a%3 %4 0 1 0 -%5 0a%3 %4 0 1 0 %5 0").arg(cx).arg(cy).arg(rx).arg(ry).arg(2 * rx);
    }
    Q_INVOKABLE QString line(float x0, float y0, float x1, float y1) {
        return QString("M%1 %2L%3 %4").arg(x0).arg(y0).arg(x1).arg(y1);
    }
    Q_INVOKABLE QString rectangle(float x, float y, float w, float h) {
        return QString("M%1 %2L%3 %2L%3 %4L%1 %4Z").arg(x).arg(y).arg(x + w).arg(y + h);
    }
    /// Generates a rounded rectangle with corner radius r.
    Q_INVOKABLE QString rectangle(float x, float y, float w, float h, float r) {
        return QString("M %3 %2 L %7 %2 A %9 %9 0 0 1 %5 %4 L %5 %8 A %9 %9 0 0 1 %7 %6 L %3 %6 A %9 %9 0 0 1 %1 %8 L "
                       "%1 %4 A %9 %9 0 0 1 %3 %2 Z")
            .arg(x)
            .arg(y)
            .arg(x + r)
            .arg(y + r)
            .arg(x + w)
            .arg(y + h)
            .arg(x + w - r)
            .arg(y + h - r)
            .arg(r);
    }
    /// Generates a rounded rectangle with separate corner radii for x and y.
    Q_INVOKABLE QString rectangle(float x, float y, float w, float h, float rx, float ry) { return QString(); }
    /// Generates a rounded rectangle with different radii for each corner,
    /// starting top-left and going clockwise.
    Q_INVOKABLE QString rectangle(float x, float y, float w, float h, float r0, float r1, float r2, float r3) {
        return QString(
                   "M %2 %7 L %3 %7 A %14 %14 0 0 1 %4 %8 L %4 %9 A %15 %15 0 0 1 %5 %10 L %6 %10 A %16 %16 0 0 1 %1 "
                   "%11 L %1 %12 A %13 %13 0 0 1 %2 %7 Z")
            .arg(x)
            .arg(x + r0)
            .arg(x + w - r1)
            .arg(x + w)
            .arg(x + w - r2)
            .arg(x + r3)
            .arg(y)
            .arg(y + r1)
            .arg(y + h - r2)
            .arg(y + h)
            .arg(y + h - r3)
            .arg(y + r0)
            .arg(r0)
            .arg(r1)
            .arg(r2)
            .arg(r3);
    }
    /// Generates a star with the specified outer and inner radii and the number of points.
    Q_INVOKABLE QString star(float cx, float cy, float ro, float ri, int points = 5) {
        QString path = QString("M %1 %2").arg(cx).arg(cy - ro);

        for (int i = 1; i < points * 2; ++i) {
            float angle  = M_PI * i / points;
            float radius = (i % 2 == 0) ? ro : ri;
            float x      = cx + radius * qSin(angle);
            float y      = cy - radius * qCos(angle);
            path += QString("L %1 %2").arg(x).arg(y);
        }

        path += "Z";
        return path;
    }

    Q_INVOKABLE QString arrow(float x0, float y0, float x1, float y1, float thickness, float width = 4,
                              float length = 4, float concavity = 0) {
        // Convert input points to QPointF
        QPointF p0(x0, y0);
        QPointF p1(x1, y1);

        // Calculate distance and direction
        float distance = qSqrt(qPow(p1.x() - p0.x(), 2) + qPow(p1.y() - p0.y(), 2));
        if (distance < 1e-6) return QString("M %1 %2 Z").arg(x0).arg(y0); // Degenerate case

        QPointF direction((p1.x() - p0.x()) / distance, (p1.y() - p0.y()) / distance);

        // Calculate normal vector
        QPointF normal(0.5f * thickness * direction.y(), -0.5f * thickness * direction.x());

        // Calculate base point
        QPointF base = p0 + direction * qMax(0.0f, distance - thickness * length);

        // Precompute points
        QPointF pt1            = p0 - normal; // Left side of shaft
        QPointF concave_offset = direction * (thickness * length * concavity);
        QPointF pt2            = base - normal + concave_offset; // Left concave point
        QPointF width_offset   = normal * width;
        QPointF pt3            = base - width_offset;            // Left base of arrowhead
        QPointF pt4            = p1;                             // Arrowhead tip
        QPointF pt5            = base + width_offset;            // Right base of arrowhead
        QPointF pt6            = base + normal + concave_offset; // Right concave point
        QPointF pt7            = p0 + normal;                    // Right side of shaft

        // Build SVG path
        return QString("M %1 %2 L %3 %4 L %5 %6 L %7 %8 L %9 %10 L %11 %12 L %13 %14 Z")
            .arg(pt1.x())
            .arg(pt1.y()) // Point 1
            .arg(pt2.x())
            .arg(pt2.y()) // Point 2
            .arg(pt3.x())
            .arg(pt3.y()) // Point 3
            .arg(pt4.x())
            .arg(pt4.y()) // Point 4
            .arg(pt5.x())
            .arg(pt5.y()) // Point 5
            .arg(pt6.x())
            .arg(pt6.y()) // Point 6
            .arg(pt7.x())
            .arg(pt7.y()); // Point 7
    }
};

} // namespace dsqt

#endif
