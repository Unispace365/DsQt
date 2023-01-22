#include "dssettings_proxy.h"
#include "core/dsenvironment.h"

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
	return result;
}

QVariant DSSettingsProxy::getFloat(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingsProxy::getDate(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingsProxy::getPoint(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingsProxy::getRect(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingsProxy::getSize(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
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


}  // namespace dsqt
