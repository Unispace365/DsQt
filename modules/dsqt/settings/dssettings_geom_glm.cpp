#include "dssettings.h"
#include <algorithm>
#include <string>


/*
vec2 ✔
vec3 ✔
vec4 ✔
*/

namespace dsqt {

template <>
std::optional<ValueWMeta<glm::vec4>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING glm::vec4";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto nArray				 = node;


	if (nArray.as_array()->size() >= 4) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();
		auto v4 = nArray[3].value<double>();
		if ((v1 && v2 && v3 && v4)) {
			return std::optional<ValueWMeta<glm::vec4>>(
				ValueWMeta<glm::vec4>(glm::vec4(v1.value(), v2.value(), v3.value(), v4.value()), meta, place));
		}
	}
	return {};
}

template <>
std::optional<ValueWMeta<glm::vec3>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING glm::vec3";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto nArray				 = node;


	if (nArray.as_array()->size() >= 3) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();
		auto v3 = nArray[2].value<double>();

		if ((v1 && v2 && v3)) {
			return std::optional<ValueWMeta<glm::vec3>>(
				ValueWMeta<glm::vec3>(glm::vec3(v1.value(), v2.value(), v3.value()), meta, place));
		}
	}
	return {};
}

template <>
std::optional<ValueWMeta<glm::vec2>> DSSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING glm::vec2";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto nArray				 = node;


	if (nArray.as_array()->size() >= 2) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();


		if ((v1 && v2)) {
			return std::optional<ValueWMeta<glm::vec2>>(
				ValueWMeta<glm::vec2>(glm::vec2(v1.value(), v2.value()), meta, place));
		}
	}
	return {};
}

}  // namespace dsqt
