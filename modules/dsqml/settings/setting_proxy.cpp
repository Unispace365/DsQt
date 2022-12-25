#include "setting_proxy.h"
#include "dsenvironment.h"

namespace dsqt {
DSSettingProxy::DSSettingProxy(QObject* parent) : QObject{parent} {}

void DSSettingProxy::setTarget(const QString& val) {
	if (_target != val) {
		_target				 = val;
		auto [created, sets] = DSSettings::getSettingsOrCreate(_target.toStdString(), this);
		_settings			 = sets;
		emit targetChanged(_target);
	}
}

void DSSettingProxy::setPrefix(const QString& val) {
	if (_prefix != val) {
		_prefix		= val;
		_prefix_str = val.toStdString();
		emit prefixChanged(_prefix);
	}
}

void DSSettingProxy::loadFromFile(const QString& filename) {
	if (_settings) {
		_settings = DSEnvironment::loadSettings(_target.toStdString(), filename.toStdString());
	}
}

QVariant DSSettingProxy::getString(const QString& key, QVariant def) {
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

QVariant DSSettingProxy::getInt(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getFloat(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getDate(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getPoint(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getRect(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getSize(const QString& key, QVariant def) {
	QVariant	result;
	std::string keyPath = _prefix_str + "." + key.toStdString();
	return result;
}

QVariant DSSettingProxy::getColor(const QString& key, QVariant def) {
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
