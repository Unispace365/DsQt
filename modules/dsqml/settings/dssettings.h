#ifndef DSSETTINGS_H
#define DSSETTINGS_H


#include <qqml.h>
#include <toml++/toml.h>
#include <QLoggingCategory>
#include <QQmlPropertyMap>
#include <QtGui/QVector4D>
#include <glm/glm.hpp>
#include "QtGui/qcolor.h"
#include "overload.h"

#include <type_traits>
#include <typeindex>
/**
 * @brief The DSSettings class
 * Reads a setting file
 */
Q_DECLARE_LOGGING_CATEGORY(settingsParser)
Q_DECLARE_LOGGING_CATEGORY(settingsParserWarn)
namespace dsqt {
struct GeomElements {
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
const GeomElements getGeomElementsFromTable(toml::node_view<toml::node> node);

class DSSettings;
using DSSettingsRef	   = std::shared_ptr<DSSettings>;
using parse_resultWRef = std::weak_ptr<toml::parse_result>;
using NodeWMeta		   = std::tuple<toml::node_view<toml::node>, toml::table*, std::string>;
template <typename T>
using ValueWMeta = std::tuple<T, toml::table*, std::string>;
using tomlType	 = std::variant<ValueWMeta<toml::value<bool>>, ValueWMeta<toml::value<int64_t>>, ValueWMeta<toml::value<double>>,
								ValueWMeta<toml::value<std::string>>, ValueWMeta<toml::table>, ValueWMeta<toml::array>,
								ValueWMeta<toml::value<toml::date>>, ValueWMeta<toml::value<toml::time>>,
								ValueWMeta<toml::value<toml::date_time>>>;

class DSSettings : public QObject {
	Q_OBJECT
  public:
	// struct to hold setting file
	struct SettingFile {
		std::string		   filepath;
		toml::parse_result data;
		bool			   valid = false;
	};

  protected:
	DSSettings(std::string name = "", QObject* parent = nullptr);

  public:
	DSSettings(QObject* parent = nullptr) = delete;
	~DSSettings();

	template <typename T>
	static constexpr bool is_valid_setting_type = std::is_same_v<T, toml::node> || //
												  // std::is_same_v<T, QMap<QString,QVariant>> || //
												  // std::is_same_v<T, QVector<QVariant>> || //
												  std::is_same_v<T, std::string> ||	 //
												  std::is_same_v<T, QString> ||		 //
												  std::is_same_v<T, int64_t> ||		 //
												  std::is_same_v<T, int32_t> ||		 //
												  std::is_same_v<T, double> ||		 //
												  std::is_same_v<T, float> ||		 //
												  std::is_same_v<T, bool> ||		 //
												  std::is_same_v<T, QDate> ||		 //
												  std::is_same_v<T, QTime> ||		 //
												  std::is_same_v<T, QDateTime> ||	 //
												  std::is_same_v<T, QRectF> ||		 //
												  std::is_same_v<T, QSizeF> ||		 //
												  std::is_same_v<T, QPointF> ||		 //
												  std::is_same_v<T, QVector2D> ||	 //
												  std::is_same_v<T, QVector3D> ||	 //
												  std::is_same_v<T, QVector4D> ||	 //
												  std::is_same_v<T, glm::vec2> ||	 //
												  std::is_same_v<T, glm::vec3> ||	 //
												  std::is_same_v<T, glm::vec4> ||	 //
												  std::is_same_v<T, QColor>;		 //

	/// Get a DSSettingsRef by name.
	/// Get the DSSettingRef (a std::shared_ptr<DSSettings> typedef) to a setting collection associated with \b name.
	/// If the setting collection does not exist it will create it and return a DSSettingsRef to it.
	/// \returns a tuple of [bool \b exists,DSSettingsRef \b settings]
	static std::tuple<bool, DSSettingsRef> getSettingsOrCreate(const std::string& name, QObject* parent = nullptr);

	/// Get a setting collection by name.
	/// Get the DSSettingsRef (a std::shared_ptr<DSSettings> typedef)to a setting collection associated with \b name.
	/// If the setting collection does not exist it will return an empty DSSettingsRef.
	/// \returns DSSettingsRef \b settings
	static DSSettingsRef getSettings(const std::string& name);

	/// remove a setting file from the list of known setting files.
	/// \return a bool that indicates that a setting object was removed (true)
	/// or that no setting file existed (false)
	static bool forgetSettings(const std::string& name);


	/// load a setting file into this setting object.
	/// \return a bool the indecates the success
	/// or failure of loading the file.
	bool loadSettingFile(const std::string& file);


	/// Set the date format;
	void setDateFormat(Qt::DateFormat format, bool clearCustom = true) {
		mDateFormat = format;
		if (clearCustom && !mCustomDateFormat.isEmpty()) {
			qWarning(settingsParser) << "setDateFormat is clearing a custom date format for " << QString::fromStdString(mName)
									 << " settings.";
			mCustomDateFormat = "";
		}
	}
	void setCustomDateFormat(QString format) {
		mCustomDateFormat = format;
	}

	/// Get a setting from the collection.
	template <class T>
	T getOr(const std::string& key, const T& def) {
		static_assert(is_valid_setting_type<T>, "The type is not directly gettable from a settings file");
		std::optional<T> value = get<T>(key);
		return value.value_or(def);
	};

	template <class T>
	std::optional<T> get(const std::string& key) {
		static_assert(is_valid_setting_type<T>, "The type is not directly gettable from a settings file");
		auto retval = getWithMeta<T>(key);
		if (retval) {
			auto [value, x, y] = retval.value();
			return value;
		}
		return std::optional<T>();
	}

	/// Get a setting from the collection with the meta data.
	/// This is used for debuging and internal management.
	std::optional<NodeWMeta> getNodeViewWithMeta(const std::string& key);

	/// Get a setting from the collection with the meta data.
	/// This is used for debuging and internal management.

	template <class T>
	std::optional<ValueWMeta<T>> getWithMeta(const std::string& key) {
		auto val = getNodeViewWithMeta(key);
		if (!val.has_value()) {
			qDebug(settingsParser) << "Failed to find value at key " << key.c_str();
			return std::nullopt;
		}
		// auto [node,meta,place] = val.value();

		return std::nullopt;
	}

	template <class T, class V>
	std::optional<ValueWMeta<T>> originalValueWithMeta(const std::string& key, V&& overload) {
		auto val = getNodeViewWithMeta(key);
		if (!val.has_value()) {
			qDebug(settingsParser) << "Failed to find value at key " << key.c_str();
			return std::nullopt;
		}

		auto [n, m, p] = val.value();
		auto node	   = n;
		auto meta	   = m;
		auto place	   = p;
		auto grabber   = DSOverloaded{[meta, place](toml::value<std::string> s) {
										  return ValueWMeta<toml::value<std::string>>(s, meta, place);
									  },
									  [meta, place](toml::value<bool> s) {
										  return ValueWMeta<toml::value<bool>>(s, meta, place);
									  },
									  [meta, place](toml::value<int64_t> s) {
										  return ValueWMeta<toml::value<int64_t>>(s, meta, place);
									  },
									  [meta, place](toml::value<double> s) {
										  return ValueWMeta<toml::value<double>>(s, meta, place);
									  },
									  [meta, place](toml::value<toml::date> s) {
										  return ValueWMeta<toml::value<toml::date>>(s, meta, place);
									  },
									  [meta, place](toml::value<toml::time> s) {
										  return ValueWMeta<toml::value<toml::time>>(s, meta, place);
									  },
									  [meta, place](toml::value<toml::date_time> s) {
										  return ValueWMeta<toml::value<toml::date_time>>(s, meta, place);
									  },
									  [meta, place](toml::array s) {
										  return ValueWMeta<toml::array>(s, meta, place);
									  },
									  [meta, place](toml::table s) {
										  return ValueWMeta<toml::table>(s, meta, place);
									  }};
		auto nodeval   = node.visit(grabber);
		return std::optional(nodeval);
	}

	template <class T>
	std::optional<ValueWMeta<T>> getWithProcess(
		const std::string&																				 key,
		std::function<std::optional<ValueWMeta<T>>(toml::node_view<toml::node> node, toml::table* meta)> process) {
		auto val = getNodeViewWithMeta(key);
		if (!val.has_value()) return process(std::optional<ValueWMeta<T>>());
		auto [node, meta, place] = val.value();
		return process(node, meta);
	}

	// template<typename T> std::optional<ValueWMeta<std::vector<T>>> getWithMeta(const std::string& key);

	// base types
	template <>
	std::optional<ValueWMeta<bool>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<int64_t>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<int32_t>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<double>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<float>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<std::string>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QString>> getWithMeta(const std::string& key);

	// Collections
	template <>
	std::optional<ValueWMeta<toml::node>> getWithMeta(const std::string& key);
	// template<> std::optional<ValueWMeta<QVector<QVariant>>> getWithMeta(const std::string& key);
	// template<> std::optional<ValueWMeta<QMap<QString,QVariant>>> getWithMeta(const std::string& key);

	// QColor
	template <>
	std::optional<ValueWMeta<QColor>> getWithMeta(const std::string& key);


	// QDate, QTime, QDateTime
	template <>
	std::optional<ValueWMeta<QDate>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QTime>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QDateTime>> getWithMeta(const std::string& key);

	// TODO Geometry
	template <>
	std::optional<ValueWMeta<QVector2D>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QVector3D>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QVector4D>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QRect>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QRectF>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QSize>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QSizeF>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QPoint>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<QPointF>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<glm::vec2>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<glm::vec3>> getWithMeta(const std::string& key);
	template <>
	std::optional<ValueWMeta<glm::vec4>> getWithMeta(const std::string& key);

	/*
	const std::string&				SETTING_TYPE_UNKNOWN = "unknown";
	const std::string&				SETTING_TYPE_BOOL = "bool";
	const std::string&				SETTING_TYPE_INT = "int";
	const std::string&				SETTING_TYPE_FLOAT = "float";
	const std::string&				SETTING_TYPE_DOUBLE = "double";
	const std::string&				SETTING_TYPE_STRING = "string";
	const std::string&				SETTING_TYPE_WSTRING = "wstring";
	const std::string&				SETTING_TYPE_COLOR = "color";
	const std::string&				SETTING_TYPE_COLORA = "colora";
	const std::string&				SETTING_TYPE_VEC2 = "vec2";
	const std::string&				SETTING_TYPE_VEC3 = "vec3";
	const std::string&				SETTING_TYPE_RECT = "rect";
	const std::string&				SETTING_TYPE_SECTION_HEADER = "section_header";
	const std::string&				SETTING_TYPE_TEXT_STYLE = "text_style";
*/
	// Set a value in the collection.
	template <class T>
	void set(std::string& key, T& value);


	// QQmlPropertyMap interface
  private:
	// meta data
	Qt::DateFormat										  mDateFormat		= Qt::ISODateWithMs;
	QString												  mCustomDateFormat = "";
	static std::string									  mConfigurationDirectory;
	static bool											  mLoadedConfiguration;
	static std::unordered_map<std::string, DSSettingsRef> sSettings;
	std::string											  mName = "";
	std::vector<SettingFile>							  mResultStack;
	SettingFile											  mRuntimeResult;
	static std::unordered_map<std::string,
							  std::function<void(QColor& resultcolor, float v1, float v2, float v3, float v4, float v5)>>
		sColorConversionFuncs;
	// static std::unordered_map<std::type_index,DSOverloaded> sOverloadMap;
};



}  // namespace dsqt
#endif	// DSSETTINGS_H
