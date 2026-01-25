#include "settings/dsQmlSettingsProxy.h"
#include "core/dsEnvironment.h"

#include <QDateTime>

namespace dsqt {
DsQmlSettingsProxy::DsQmlSettingsProxy(QObject* parent) : QObject{parent} {}

void DsQmlSettingsProxy::setTarget(const QString& val) {
	if (_target != val) {
		_target				 = val;
		auto [created, sets] = DsSettings::getSettingsOrCreate(_target.toStdString(), this);
		_settings			 = sets;
		emit targetChanged(_target);
	}
}

void DsQmlSettingsProxy::setPrefix(const QString& in_val) {
    auto val =  in_val.trimmed();
	if (_prefix != val) {
        if(!val.isEmpty()) val += ".";
		_prefix		= val;
		_prefix_str = val.toStdString();
		emit prefixChanged(_prefix);
	}
}

void DsQmlSettingsProxy::loadFromFile(const QString& filename) {
	if (_settings) {
        _settings = DsEnvironment::loadSettings(_target, filename);
	}
}

QVariant DsQmlSettingsProxy::getString(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QString>()) {
		result = _settings->getOr(keyPath, def.toString());
	} else {
		auto strOpt = _settings->get<QString>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getInt(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<int>()) {
		result = _settings->getOr(keyPath, def.toInt());
	} else {
		auto strOpt = _settings->get<int>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getFloat(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<float>()) {
		result = _settings->getOr(keyPath, def.toFloat());
	} else {
		auto strOpt = _settings->get<float>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getDate(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QDateTime>()) {
		result = _settings->getOr(keyPath, def.toDateTime());
	} else {
		auto strOpt = _settings->get<QDateTime>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getPoint(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QPointF>()) {
		result = _settings->getOr(keyPath, def.toPointF());
	} else {
		auto strOpt = _settings->get<QPointF>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getRect(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QRectF>()) {
		result = _settings->getOr(keyPath, def.toRectF());
	} else {
		auto strOpt = _settings->get<QRectF>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getSize(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QSizeF>()) {
		result = _settings->getOr(keyPath, def.toSizeF());
	} else {
		auto strOpt = _settings->get<QSizeF>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getColor(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<QColor>()) {
		result = _settings->getOr(keyPath, def.value<QColor>());
	} else {
		auto strOpt = _settings->get<QColor>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DsQmlSettingsProxy::getBool(const QString& key, QVariant def) {
	QVariant	result;
    std::string keyPath = _prefix_str + key.toStdString();
	if (def.canConvert<bool>()) {
		result = _settings->getOr(keyPath, def.toBool());
	} else {
		auto strOpt = _settings->get<bool>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariantList DsQmlSettingsProxy::getList(const QString& key, QVariant def) {
    QVariantList result;
    std::string keyPath = _prefix_str.empty() ? key.toStdString() : _prefix_str + key.toStdString();
    if (def.canConvert<QVariantList>()) {
        result = _settings->getOr(keyPath, def.toList());
    } else {
        auto strOpt = _settings->get<QVariantList>(keyPath);
        if (strOpt) {
            result = strOpt.value();
        }
    }
    return result;
}

QVariantMap DsQmlSettingsProxy::getObj(const QString& key, QVariant def) {
    QVariantMap result;
    std::string keyPath = _prefix_str.empty() ? key.toStdString() : _prefix_str + key.toStdString();
    if(def.canConvert<QVariantMap>()) {
        result = _settings->getOr(keyPath, def.toMap());
    } else {
        auto strOpt = _settings->get<QVariantMap>(keyPath);
        if(strOpt) {
            result = strOpt.value();
        }
    }
    return result;
}


}  // namespace dsqt
