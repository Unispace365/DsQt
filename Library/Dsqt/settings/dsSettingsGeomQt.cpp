#include "settings/dsSettings.h"

#include <string>


/*
QVector2D ✔
QVector3D ✔
QVector4D ✔
QRect  ✔
QRectF ✔
QSize ✔
QSizeF ✔
QPoint ✔
QPointF ✔
*/

namespace dsqt {

enum class ArrayType { XYWH, P1P2 };

ArrayType getArrayTypeFromMeta(toml::table* metaData) {
	if(metaData){
		auto type = metaData->get("layout")->value_or<std::string>("xywh");
		if (type == "p1p2" || type == "P1P2") {
			return ArrayType::P1P2;
		}
	}
	return ArrayType::XYWH;
};



template <>
MaybeQVector4DMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QVector4D";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	const auto& nArray				 = node.is_array()?*node.as_array():toml::array();


	if (nArray.size() >= 4) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();
		auto v4 = nArray[3].value<double>();
		if (v1 && v2 && v3 && v4) {
            return MaybeQVector4DMeta( {QVector4D(v1.value(), v2.value(), v3.value(), v4.value()), meta, place} );
		}
	}
	return {};
}

template <>
MaybeQVector3DMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QVector3D";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	const auto& nArray				 = node.is_array()?*node.as_array():toml::array();


	if (nArray.size() >= 3) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();

		if (v1 && v2 && v3) {
			return MaybeQVector3DMeta( { QVector3D(v1.value(), v2.value(), v3.value()), meta, place} );
		}
	}
	return {};
}

template <>
MaybeQVector2DMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QVector2D";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	const auto& nArray				 = node.is_array()?*node.as_array():toml::array();

	if (nArray.size() >= 2) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();

		if (v1 && v2 ) {
			return MaybeQVector2DMeta( { QVector2D(v1.value(), v2.value()), meta, place} );
		}
	}
	return {};
}

template <>
MaybeQRectFMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QRectF";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto type				 = getArrayTypeFromMeta(meta);
	const auto& nArray				 = node.is_array()?*node.as_array():toml::array();


	if (nArray.size() == 4) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();
		auto v4 = nArray[3].value<double>();
		if ((v1 && v2 && v3 && v4)) {
			if (type == ArrayType::XYWH) {
				return MaybeQRectFMeta( {QRectF(v1.value(), v2.value(), v3.value(), v4.value()), meta, place} );
			} else {
				return MaybeQRectFMeta( {QRectF(QPointF(v1.value(), v2.value()), QPointF(v3.value(), v4.value())), meta, place} );
			}
		}
	} else if (node.is_table()) {
		auto [_x, _y, z, w, h, x1, y1, x2, y2] = getGeomElementsFromTable(node);
		auto x								   = _x.value_or(0.0);
		auto y								   = _y.value_or(0.0);
		if (w && h) {
			return MaybeQRectFMeta( {QRectF(x, y, w.value_or(0.0f), h.value_or(0.0f)), meta, place} );
		} else if (x2 && y2) {
			return MaybeQRectFMeta( {QRectF(QPointF(x1.value_or(x), y1.value_or(y)), QPointF(x2.value_or(0.0f), y2.value_or(0.0f))), meta, place} );
		}
	}
	return {};
}

template <>
MaybeQRectMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QRect";
    MaybeQRectFMeta val = getWithMeta<QRectF>(key);
    if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();

	QRect outVal(node.x(), node.y(),node.width(),node.height());
	return MaybeQRectMeta( {outVal, meta, place} );
}

template <>
MaybeQPointFMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QPointF";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	const auto& nArray				 = node.is_array()?*node.as_array():toml::array();


	if (nArray.size() == 2) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		if ((v1 && v2)) {

			return MaybeQPointFMeta( {QPointF(v1.value(), v2.value()), meta, place} );
		}
	} else if (node.is_table()) {
		auto [x, y, z, w, h, x1, y1, x2, y2] = getGeomElementsFromTable(node);

		if (x && y) {
			return MaybeQPointFMeta( {QPointF(x.value_or(0.0), y.value_or(0.0)), meta, place} );
		} else if (x1 && y1) {
			return MaybeQPointFMeta( {QPointF(x1.value_or(0.0), y1.value_or(0.0)), meta, place} );
		} else if (w && h) {
			return MaybeQPointFMeta( {QPointF(w.value_or(0.0), h.value_or(0.0)), meta, place} );
		}
	}
	return {};
}

template <>
MaybeQPointMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QPoint";
    MaybeQPointFMeta val = getWithMeta<QPointF>(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();

	QPoint outVal(node.x(), node.y());
	return MaybeQPointMeta( {outVal, meta, place} );
}

template <>
MaybeQSizeFMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QSizeF";
    MaybeQPointFMeta val = getWithMeta<QPointF>(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();

	QSizeF outVal(node.x(), node.y());
	return MaybeQSizeFMeta({outVal, meta, place} );
}

template <>
MaybeQSizeMeta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING QSize";
	auto val = getWithMeta<QPointF>(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();

	QSize outVal(node.x(), node.y());
	return MaybeQSizeMeta({outVal, meta, place} );
}

}  // namespace dsqt
