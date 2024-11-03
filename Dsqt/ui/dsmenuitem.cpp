#include "dsmenuitem.h"

DSMenuItem::DSMenuItem(QObject* parent) : QObject{parent} {}

QImage DSMenuItem::image() const {
	return m_image;
}

void DSMenuItem::setImage(const QImage& newImage) {
	if (m_image == newImage) return;
	m_image = newImage;
	emit imageChanged();
}

QString DSMenuItem::text() const {
	return m_text;
}

void DSMenuItem::setText(const QString& newText) {
	if (m_text == newText) return;
	m_text = newText;
	emit textChanged();
}
