#include <dssettings.h>
#include <algorithm>
#include <string>


/*
QVector2D
QVector3D
QVector4D
QRect
QRectF
QSize
QSizeF
QPoint
QPointF
*/

namespace dsqt {

enum class ArrayType { XYWH, P1P2 };

ArrayType getArrayTypeFromMeta(toml::table* metaData) {
	auto type = metaData->get("layout")->value_or<std::string>("xywh");
	if (type == "p1p2" || type == "P1P2") {
		return ArrayType::P1P2;
	}
	return ArrayType::XYWH;
};

struct GeomElements
{
	std::optional<double> x;
	std::optional<double> y;
	std::optional<double> z;
	std::optional<double> w;
	std::optional<double> h;
	std::optional<double> x1;
	std::optional<double> y1;
	std::optional<double> x2;
	std::optional<double> y2;
};

const GeomElements getGeomElementsFromTable(toml::node_view<toml::node> node){


	toml::table& nTable = *(node.as_table());
	GeomElements out;
	out.x = nTable["x"].value<double>();
	out.y = nTable["y"].value<double>();
	out.z = nTable["z"].value<double>();
	out.w = nTable["w"].value<double>();
	out.h = nTable["h"].value<double>();
	out.x1 = nTable["x1"].value<double>();
	out.y1 = nTable["y1"].value<double>();
	out.x2 = nTable["x2"].value<double>();
	out.y2 = nTable["y2"].value<double>();
	return out;
}

template <>
std::optional<ValueWMeta<QRectF>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(settingsParser) << "RUNNING float";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return std::optional<ValueWMeta<QRectF>>();
	auto [node, meta, place] = val.value();
	auto  type				 = getArrayTypeFromMeta(meta);
	auto nArray = node.value<toml::array>().value_or(toml::array());


	if (nArray.size() == 4) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();
		auto v4 = nArray[3].value<double>();
		if ((v1 && v2 && v3 && v4)) {
			if (type == ArrayType::XYWH) {
				return std::optional<ValueWMeta<QRectF>>(
					ValueWMeta<QRectF>(QRectF(v1.value(), v2.value(), v3.value(), v4.value()), meta, place));
			} else {
				return std::optional<ValueWMeta<QRectF>>(
					ValueWMeta<QRectF>(QRectF(QPointF(v1.value(), v2.value()), QPointF(v3.value(), v4.value())), meta, place));
			}
		}
	} else if (node.is_table()) {
		auto [_x, _y, z, w, h, x1, y1, x2, y2] = getGeomElementsFromTable(node);
		auto x								   = _x.value_or(0.0);
		auto y								   = _y.value_or(0.0);
		if (w && h) {
			return std::optional<ValueWMeta<QRectF>>(
				ValueWMeta<QRectF>(QRectF(x, y, w.value_or(0.0f), h.value_or(0.0f)), meta, place));
		} else if (x2 && y2) {
			return std::optional<ValueWMeta<QRectF>>(ValueWMeta<QRectF>(
				QRectF(QPointF(x1.value_or(x), y1.value_or(y)), QPointF(x2.value_or(0.0f), y2.value_or(0.0f))), meta, place));
		}
	}
	return {};
}

template <>
std::optional<ValueWMeta<QPointF>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(settingsParser) << "RUNNING float";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto nArray = node.value<toml::array>().value_or(toml::array());


	if (nArray.size() == 2) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		if ((v1 && v2 )) {

				return std::optional<ValueWMeta<QPointF>>(
					ValueWMeta<QPointF>(QPointF(v1.value(), v2.value()), meta, place));

		}
	} else if (node.is_table()) {
		auto [x, y, z, w, h, x1, y1, x2, y2] = getGeomElementsFromTable(node);

		if (x && y) {
			return std::optional<ValueWMeta<QPointF>>(
					ValueWMeta<QPointF>(QPointF(x.value_or(0.0), y.value_or(0.0)), meta, place));
		} else if (x1 && y1) {
			return std::optional<ValueWMeta<QPointF>>(
				ValueWMeta<QPointF>(QPointF(x1.value_or(0.0), y1.value_or(0.0)), meta, place));
		} else if (w && h) {
			return std::optional<ValueWMeta<QPointF>>(
				ValueWMeta<QPointF>(QPointF(w.value_or(0.0), h.value_or(0.0)), meta, place));
		}
	}
	return {};
}

template <>
std::optional<ValueWMeta<QSizeF>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(settingsParser) << "RUNNING float";
	auto val = getWithMeta<QPointF>(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();

	QSizeF outVal(node.x(),node.y());
	return std::optional<ValueWMeta<QSizeF>>(ValueWMeta<QSizeF>(outVal,meta,place));
}

}  // namespace dsqt
