#ifndef DSMENUITEM_H
#define DSMENUITEM_H

#include <QImage>
#include <QObject>
#include "qqmlintegration.h"
namespace dsqt::ui {
    class DSMenuItem : public QObject {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged FINAL)
        Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)
      public:
        explicit DSMenuItem(QObject *parent = nullptr);

        QImage image() const;
        void   setImage(const QImage& newImage);

        QString text() const;
        void	setText(const QString& newText);

      signals:
        void imageChanged();
        void textChanged();

      private:
        QImage	m_image;
        QString m_text;
    };
}
#endif	// DSMENUITEM_H
