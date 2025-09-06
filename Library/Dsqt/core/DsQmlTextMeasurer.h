// DsQmlTightBoundingBox.h
#ifndef DSQMLTEXTMEASURER_H
#define DSQMLTEXTMEASURER_H

#include <QFont>
#include <QFontMetricsF>
#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRectF>
#include <QTextLayout>

/// Allows measurement of text,
class DsQmlTextMeasurer : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTextMeasurer)

  public:
    explicit DsQmlTextMeasurer(QObject* parent = nullptr)
        : QObject(parent) {}

    Q_INVOKABLE QRectF rect(QQuickItem* textItem) const {
        if (!textItem || !textItem->property("text").isValid() || !textItem->property("font").isValid() ||
            !textItem->property("width").isValid() || !textItem->property("lineHeight").isValid()) {
            qWarning() << "Invalid Text item or missing required properties";
            return QRectF();
        }

        const QString text       = textItem->property("text").toString();
        const QFont   font       = textItem->property("font").value<QFont>();
        const qreal   width      = textItem->property("width").toReal();
        const qreal   lineHeight = textItem->property("lineHeight").toReal();

        QFontMetricsF fm(font);
        if (width <= 0) {
            return fm.tightBoundingRect(text).translated(0, fm.ascent());
        } else {
            QTextLayout layout(text, font);
            layout.beginLayout();

            QRectF result;
            qreal  offset = 0;
            while (true) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) break; // No more lines.
                line.setLineWidth(width);   // Set the width for word wrapping.

                QString str(text.begin() + line.textStart(), line.textLength());
                QRectF  bounds = fm.tightBoundingRect(str).translated(0, offset + fm.ascent());

                result = result.isNull() ? bounds : result.united(bounds);
                offset += lineHeight * line.height();
            }

            layout.endLayout();
            return result;
        }
    }

    /// Adjusts the text item's height and padding to fit the text snugly.
    Q_INVOKABLE void fit(QQuickItem* textItem) const {
        QRectF bounds = rect(textItem);
        if (!bounds.isNull()) {
            textItem->setProperty("height", bounds.height());
            textItem->setProperty("topPadding", -bounds.top());
            textItem->setProperty("leftPadding", -bounds.left());
        }
    }
};

#endif
