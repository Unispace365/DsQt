#include "settings/dsSettings.h"

#include <string>

namespace dsqt {

template <>
MaybeVec4Meta DsSettings::getWithMeta(const std::string& key) {
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
            return MaybeVec4Meta( {glm::vec4(v1.value(), v2.value(), v3.value(), v4.value()), meta, place} );
		}
	}
	return {};
}

template <>
MaybeVec3Meta DsSettings::getWithMeta(const std::string& key) {
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
			return MaybeVec3Meta( {glm::vec3(v1.value(), v2.value(), v3.value()), meta, place} );
		}
	}
	return {};
}

template <>
MaybeVec2Meta DsSettings::getWithMeta(const std::string& key) {
	qCDebug(lgSPVerbose) << "RUNNING glm::vec2";
	auto val = getNodeViewWithMeta(key);
	if (!val.has_value()) return {};
	auto [node, meta, place] = val.value();
	auto nArray				 = node;


	if (nArray.as_array()->size() >= 2) {
		auto v1 = nArray[0].value<double>();
		auto v2 = nArray[1].value<double>();


		if ((v1 && v2)) {
			return MaybeVec2Meta( {glm::vec2(v1.value(), v2.value()), meta, place} );
		}
	}
	return {};
}

}  // namespace dsqt
