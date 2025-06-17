#include "dssettings_proxy.h"
#include "core/dsenvironment.h"
#include <QDateTime>
namespace dsqt {
DSSettingsProxy::DSSettingsProxy(QObject* parent) : QObject{parent} {}

void DSSettingsProxy::setTarget(const QString& val) {
	if (_target != val) {
		_target				 = val;
		auto [created, sets] = DSSettings::getSettingsOrCreate(_target.toStdString(), this);
		_settings			 = sets;
		emit targetChanged(_target);
	}
}

void DSSettingsProxy::setPrefix(const QString& val) {
	if (_prefix != val) {
		_prefix		= val;
		_prefix_str = val.toStdString();
		emit prefixChanged(_prefix);
	}
}

void DSSettingsProxy::loadFromFile(const QString& filename) {
	if (_settings) {
		_settings = DSEnvironment::loadSettings(_target.toStdString(), filename.toStdString());
	}
}

QVariant DSSettingsProxy::getString(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getInt(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getFloat(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getDate(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	if (def.canConvert<QDateTime>()) {
		result = _settings->getOr(keyPath, def.toString());
	} else {
		auto strOpt = _settings->get<QDateTime>(keyPath);
		if (strOpt) {
			result = strOpt.value();
		}
	}
	return result;
}

QVariant DSSettingsProxy::getPoint(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getRect(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getSize(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getColor(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariant DSSettingsProxy::getBool(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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

QVariantList DSSettingsProxy::getList(const QString& key, QVariant def) {
    QVariantList result;
    std::string keyPath = _prefix_str.empty() ? key.toStdString() : _prefix_str + "." + key.toStdString();
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

QVariantMap DSSettingsProxy::getObj(const QString& key, QVariant def) {
    QVariantMap result;
    std::string keyPath = _prefix_str.empty() ? key.toStdString() : _prefix_str + "." + key.toStdString();
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
