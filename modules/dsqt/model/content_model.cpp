

#include "content_model.h"

#include <dsqmlapplicationengine.h>
#include <utility/string_util.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "qdebug.h"
Q_LOGGING_CATEGORY(lgContentModel, "model.contentmodel")
Q_LOGGING_CATEGORY(lgContentModelVerbose, "model.contentmodel.verbose")
namespace dsqt::model {

namespace {
const int											  EMPTY_INT = 0;
const QString										  EMPTY_STRING;
const QUrl											  EMPTY_URL;
const std::vector<ContentModelRef>					  EMPTY_DATAMODELREF_VECTOR;
const ContentModelRef								  EMPTY_DATAMODEL;
const ContentProperty								  EMPTY_PROPERTY;
const Resource										  EMPTY_RESOURCE;
const std::vector<ContentProperty>					  EMPTY_PROPERTY_LIST;
const std::map<QString, ContentProperty>			  EMPTY_PROPERTY_MAP;
const std::map<QString, std::vector<ContentProperty>> EMPTY_PROPERTY_LIST_MAP;
const std::unordered_map<QString, ContentModelRef>	  EMPTY_REFERENCE;

const std::vector<bool>			EMPTY_BOOL_LIST;
const std::vector<int>			EMPTY_INT_LIST;
const std::vector<float>		EMPTY_FLOAT_LIST;
const std::vector<double>		EMPTY_DOUBLE_LIST;
const std::vector<QString>		EMPTY_STRING_LIST;
const std::vector<std::wstring> EMPTY_WSTRING_LIST;
const std::vector<QColor>		EMPTY_COLOR_LIST;
const std::vector<QColor>		EMPTY_COLORA_LIST;
const std::vector<glm::vec2>	EMPTY_VEC2_LIST;
const std::vector<glm::vec3>	EMPTY_VEC3_LIST;
const std::vector<QRectF>		EMPTY_RECTF_LIST;

}  // namespace


ContentProperty::ContentProperty() : mName(""), mValue(""), mIntValue(0), mDoubleValue(0) {}

ContentProperty::ContentProperty(const QString& name, const QString& value) {
	setValue(value);
	setName(name);
}

ContentProperty::ContentProperty(const QString& name, const QString& value, const int& valueInt, const double& valueDouble) {
	mName		 = name;
	mValue		 = value;
	mIntValue	 = valueInt;
	mDoubleValue = valueDouble;
}

const QString& ContentProperty::getName() const {
	return mName;
}

void ContentProperty::setName(const QString& name) {
	mName = name;
}

const QString& ContentProperty::getValue() const {
	return mValue;
}

void ContentProperty::setValue(const QString& value) {
	mValue		 = value;
	mIntValue	 = mValue.toInt();
	mDoubleValue = mValue.toDouble();
}

// void ContentProperty::setValue(const std::wstring& value) {
// 	mValue		 = ds::utf8_from_wstr(value);
// 	mIntValue	 = ds::wstring_to_int(value);
// 	mDoubleValue = ds::wstring_to_double(value);
// }

void ContentProperty::setValue(const int& value) {
	mValue		 = mValue.setNum(value);
	mIntValue	 = value;
	mDoubleValue = (double)mIntValue;
}

void ContentProperty::setValue(const double& value) {
	mValue		 = mValue.setNum(value);
	mIntValue	 = (int)round(value);
	mDoubleValue = value;
}

void ContentProperty::setValue(const float& value) {
	mValue		 = mValue.setNum(value);
	mIntValue	 = (int)roundf(value);
	mDoubleValue = (double)(value);
}

void ContentProperty::setValue(const QColor& value) {
	mValue		 = value.name(QColor::HexArgb);
	mIntValue	 = 0;
	mDoubleValue = 0.0;
}

// Start here with creating utility.h/.cpp file for conversions.
void ContentProperty::setValue(const glm::vec2& value) {
	mValue		 = QString::fromStdString(dsqt::unparseVector(value));
	mIntValue	 = 0;
	mDoubleValue = 0.0;
}

void ContentProperty::setValue(const glm::vec3& value) {
	auto v		 = dsqt::unparseVector(value);
	mValue		 = QString::fromStdString(v);
	mIntValue	 = 0;
	mDoubleValue = 0.0;
}

void ContentProperty::setValue(const QRectF& value) {
	auto v		 = dsqt::unparseRect(value);
	mValue		 = QString::fromStdString(v);
	mIntValue	 = 0;
	mDoubleValue = 0.0;
}

Resource ContentProperty::getResource() const {
	if (mResource) return *mResource;
	return EMPTY_RESOURCE;
}

void ContentProperty::setResource(const dsqt::Resource& resource) {
	dsqt::Resource reccy = resource;
	mResource			 = std::make_shared<dsqt::Resource>(reccy);
}
void ContentProperty::setValue(const QUrl& value) {
	mValue = value.toString(QUrl::FormattingOptions(QUrl::FullyEncoded));
}

QUrl ContentProperty::getUrl() const {
	QUrl url(mValue);
	if (url.isValid()) {
		return url;
	} else {
		return EMPTY_URL;
	}
}


bool ContentProperty::operator==(const ContentProperty& b) const {
	// If both resource shared pointers == null, match
	// If neither are null, do a full resource compare
	// bool resourcesComparable = (mResource.get() != nullptr && b.mResource.get() != nullptr);
	// bool sameResource = (mResource.get() == b.mResource.get()) || (resourcesComparable && (*mResource == *b.mResource));

	return mName == b.mName && mValue == b.mValue;
}

bool ContentProperty::empty() const {
	return mName.isEmpty();
}

bool ContentProperty::getBool() const {
	return dsqt::parseBoolean(mValue.toStdString());
}

int ContentProperty::getInt() const {
	return mIntValue;
}

float ContentProperty::getFloat() const {
	return (float)mDoubleValue;
}

double ContentProperty::getDouble() const {
	return mDoubleValue;
}

QColor ContentProperty::getColor() const {
	return QColor::fromString(mValue);
}


const QString& ContentProperty::getString() const {
	return getValue();
}


glm::vec2 ContentProperty::getVec2() const {
	return glm::vec2(dsqt::parseVector(mValue.toStdString()));
}

glm::vec3 ContentProperty::getVec3() const {
	return dsqt::parseVector(mValue.toStdString());
}

QRectF ContentProperty::getRect() const {
	return dsqt::parseRect(mValue.toStdString());
}


Data::Data() : mName(EMPTY_STRING), mLabel(EMPTY_STRING), mUserData(nullptr), mId(EMPTY_STRING) {}
Data::Data(const Data& other)
  : QSharedData(other)
  , mName(other.mName)
  , mLabel(other.mLabel)
  , mUserData(other.mUserData)
  , mId(other.mId)
  , mProperties(other.mProperties)
  , mPropertyLists(other.mPropertyLists)
  , mChildren(other.mChildren)
  , mReferences(other.mReferences) {}
Data::~Data() {}


ContentModelRef::ContentModelRef() {}

ContentModelRef::ContentModelRef(const ContentModelRef& other) : mData(other.mData) {}

ContentModelRef::ContentModelRef(const QString& name, const QString& id, const QString& label) {
	setName(name);
	setId(id);
	setLabel(label);
}

ContentModelRef& ContentModelRef::operator=(const ContentModelRef& other) {
	mData = other.mData;
	return *this;
}

const QString& ContentModelRef::getId() const {
	if (!mData) return EMPTY_STRING;
	return mData->mId;
}

void ContentModelRef::setId(const QString& id) {
	createData();
	mData->mId = id;
}

const QString& ContentModelRef::getName() const {
	if (!mData) return EMPTY_STRING;
	return mData->mName;
}

void ContentModelRef::setName(const QString& name) {
	createData();
	mData->mName = name;
}

const QString& ContentModelRef::getLabel() const {
	if (!mData) return EMPTY_STRING;
	return mData->mLabel;
}

void ContentModelRef::setLabel(const QString& name) {
	createData();
	mData->mLabel = name;
}

void* ContentModelRef::getUserData() const {
	if (!mData) return nullptr;
	return mData->mUserData;
}

void ContentModelRef::setUserData(void* userData) {
	createData();
	mData->mUserData = userData;
}

bool ContentModelRef::empty() const {
	if (!mData) return true;
	if (mData->mId == EMPTY_STRING && mData->mName == EMPTY_STRING && mData->mLabel == EMPTY_STRING &&
		mData->mUserData == nullptr && mData->mChildren.empty() && mData->mProperties.empty() && mData->mReferences.empty() &&
		mData->mPropertyLists.empty()) {
		return true;
	}

	return false;
}

void ContentModelRef::clear() {
	mData.reset(new Data());
}

dsqt::model::ContentModelRef ContentModelRef::duplicate() const {
	if (empty()) {
		return dsqt::model::ContentModelRef();
	}

	dsqt::model::ContentModelRef newModel(getName(), getId(), getLabel());
	newModel.setUserData(getUserData());

	if (!mData) return newModel;

	std::map<QString, ContentProperty> props = mData->mProperties;
	newModel.setProperties(props);

	std::vector<ContentModelRef> newChildren;
	for (const auto& it : mData->mChildren) {
		newChildren.emplace_back(it.duplicate());
	}
	newModel.setChildren(newChildren);

	newModel.mData->mReferences	   = mData->mReferences;
	newModel.mData->mPropertyLists = mData->mPropertyLists;

	return newModel;
}

namespace {
	template <typename Map>
	bool map_compare(Map const& lhs, Map const& rhs) {
		// No predicate needed because there is operator== for pairs already.
		return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
	}
} // namespace

bool ContentModelRef::operator==(const ContentModelRef& b) const {
	std::vector<std::pair<const void*, const void*>> alreadyChecked;

	// if (weakEqual(b)) {
	try {
		return equalChildrenAndReferences(b, alreadyChecked);
	} catch (...) {
		return false;
	}
	//}
	// else {
	// return false;
	//}
	/*if(empty() && b.empty()) return true;

	if(!mData && !b.mData) return true;

	if(!mData && b.mData || mData && !b.mData) return false;

	if (mData.get() == b.mData.get()) return true;

	if(mData->mName == b.mData->mName
	   && mData->mId == b.mData->mId
	   && mData->mLabel == b.mData->mLabel
	   && mData->mUserData == b.mData->mUserData
	   && mData->mProperties.size() == b.mData->mProperties.size()
	   && mData->mChildren.size() == b.mData->mChildren.size()
	   ) {
		if(!map_compare(mData->mProperties, b.mData->mProperties)) {
			return false;
		}
		if (!map_compare(mData->mPropertyLists, b.mData->mPropertyLists)) {
			return false;
		}

		// These two recurse!
		if(!map_compare(mData->mChildren, b.mData->mChildren)) {
			return false;
		}
		// This fucker is leading to an infinite loop of rechecking :(
		if (!map_compare(mData->mReferences, b.mData->mReferences)) {
			return false;
		}
		return true;
	}

	return false;*/
}

bool ContentModelRef::weakEqual(const ContentModelRef& b) const {

	if (empty() && b.empty()) return true;

	if (!mData && !b.mData) return true;

	if ((!mData && b.mData) || (mData && !b.mData)) return false;

	if (mData.get() == b.mData.get()) return true;

	return (mData->mName == b.mData->mName && mData->mId == b.mData->mId && mData->mLabel == b.mData->mLabel &&
			mData->mUserData == b.mData->mUserData && mData->mProperties.size() == b.mData->mProperties.size() &&
			mData->mChildren.size() == b.mData->mChildren.size() &&
			mData->mReferences.size() == b.mData->mReferences.size() &&
			map_compare(mData->mProperties, b.mData->mProperties) &&
			map_compare(mData->mPropertyLists, b.mData->mPropertyLists));
}

// Predicate: this & b have equal size children and references
bool ContentModelRef::equalChildrenAndReferences(const ContentModelRef&							   b,
												 std::vector<std::pair<const void*, const void*>>& alreadyChecked) const {
	if (weakEqual(b)) {
		if (!mData) return true;

		auto* max = static_cast<const void*>(std::max(mData.get(), b.mData.get()));
		auto* min = static_cast<const void*>(std::min(mData.get(), b.mData.get()));
		if (std::find(alreadyChecked.begin(), alreadyChecked.end(), std::make_pair(min, max)) != alreadyChecked.end()) {
			return true;
		}

		for (size_t i = 0; i < mData->mChildren.size(); ++i) {
			if (!mData->mChildren.at(i).equalChildrenAndReferences(b.mData->mChildren.at(i), alreadyChecked))
				return false;
		}

		alreadyChecked.push_back(std::make_pair(min, max));

		auto aIt = mData->mReferences.begin();
		auto bIt = b.mData->mReferences.begin();
		while (aIt != mData->mReferences.end() && bIt != b.mData->mReferences.end()) {
			if (aIt->first == bIt->first && aIt->second.size() == bIt->second.size()) {

				auto aaIt = aIt->second.begin();
				auto bbIt = bIt->second.begin();
				while (aaIt != aIt->second.end() && bbIt != bIt->second.end()) {
					if (aaIt->first != bbIt->first ||
						!aaIt->second.equalChildrenAndReferences(bbIt->second, alreadyChecked)) {
						return false;
					}
					aaIt++;
					bbIt++;
				}

			} else {
				return false;
			}
			aIt++;
			bIt++;
		}

		return true;
	}

	return false;
}

bool ContentModelRef::operator!=(const ContentModelRef& b) const {
	if (*this == b) {
		return false;
	} else {
		return true;
	}
}

const std::map<QString, ContentProperty>& ContentModelRef::getProperties() const {
	if (!mData) return EMPTY_PROPERTY_MAP;
	return mData->mProperties;
}

void ContentModelRef::setProperties(const std::map<QString, ContentProperty>& newProperties) {
	createData();
	mData->mProperties = newProperties;
}

dsqt::model::ContentProperty ContentModelRef::getProperty(const QString& propertyName) const {
	if (!mData) return EMPTY_PROPERTY;
	auto findy = mData->mProperties.find(propertyName);
	if (findy != mData->mProperties.end()) {
		return findy->second;
	}

	return EMPTY_PROPERTY;
}

QString ContentModelRef::getPropertyValue(const QString& propertyName) const {
	return getProperty(propertyName).getValue();
}

bool ContentModelRef::getPropertyBool(const QString& propertyName) const {
	return getProperty(propertyName).getBool();
}

int ContentModelRef::getPropertyInt(const QString& propertyName) const {
	return getProperty(propertyName).getInt();
}

float ContentModelRef::getPropertyFloat(const QString& propertyName) const {
	return getProperty(propertyName).getFloat();
}

double ContentModelRef::getPropertyDouble(const QString& propertyName) const {
	return getProperty(propertyName).getDouble();
}

QColor ContentModelRef::getPropertyColor(const QString& propertyName) const {
	return getProperty(propertyName).getColor();
}

QString ContentModelRef::getPropertyString(const QString& propertyName) const {
	return getProperty(propertyName).getString();
}

glm::vec2 ContentModelRef::getPropertyVec2(const QString& propertyName) const {
	return getProperty(propertyName).getVec2();
}

glm::vec3 ContentModelRef::getPropertyVec3(const QString& propertyName) const {
	return getProperty(propertyName).getVec3();
}

QRectF ContentModelRef::getPropertyRect(const QString& propertyName) const {
	return getProperty(propertyName).getRect();
}

QUrl ContentModelRef::getPropertyUrl(const QString& propertyName) const {
	return getProperty(propertyName).getUrl();
}

void ContentModelRef::setProperty(const QString& propertyName, ContentProperty& datamodel) {
	createData();

	mData->mProperties[propertyName] = datamodel;
}

void ContentModelRef::setProperty(const QString& propertyName, const QString& propertyValue) {
	createData();
	mData->mProperties[propertyName] = ContentProperty(propertyName, propertyValue);
}

void ContentModelRef::setProperty(const QString& propertyName, const int& value) {
	ContentProperty dp(propertyName, QString::number(value), value, (double)value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const double& value) {
	ContentProperty dp(propertyName, QString::number(value), (int)round(value), value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const float& value) {
	ContentProperty dp(propertyName, QString::number(value), (int)round(value), (double)value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const QColor& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setValue(value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const glm::vec2& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setValue(value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const glm::vec3& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setValue(value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setProperty(const QString& propertyName, const QRectF& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setValue(value);
	setProperty(propertyName, dp);
}


void ContentModelRef::setProperty(const QString& propertyName, char* value) {
	setProperty(propertyName, QString(value));
}

void ContentModelRef::setProperty(const QString& propertyName, const QUrl& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setValue(value);
	setProperty(propertyName, dp);
}

void ContentModelRef::setPropertyResource(const QString& propertyName, const Resource& value) {
	ContentProperty dp;
	dp.setName(propertyName);
	dp.setResource(value);
	setProperty(propertyName, dp);
}

const std::map<QString, std::vector<ContentProperty>>& ContentModelRef::getAllPropertyLists() const {
	if (!mData) return EMPTY_PROPERTY_LIST_MAP;
	return mData->mPropertyLists;
}

const std::vector<dsqt::model::ContentProperty>& ContentModelRef::getPropertyList(const QString& propertyName) const {
	if (!mData) return EMPTY_PROPERTY_LIST;
	auto findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		return findy->second;
	}

	return EMPTY_PROPERTY_LIST;
}

std::vector<bool> ContentModelRef::getPropertyListBool(const QString& propertyName) const {
	if (!mData) return EMPTY_BOOL_LIST;

	std::vector<bool> returnList;
	auto			  findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getBool());
		}
	}

	return returnList;
}

std::vector<int> ContentModelRef::getPropertyListInt(const QString& propertyName) const {
	if (!mData) return EMPTY_INT_LIST;

	std::vector<int> returnList;
	auto			 findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getInt());
		}
	}

	return returnList;
}

std::vector<float> ContentModelRef::getPropertyListFloat(const QString& propertyName) const {
	if (!mData) return EMPTY_FLOAT_LIST;

	std::vector<float> returnList;
	auto			   findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getFloat());
		}
	}

	return returnList;
}

std::vector<double> ContentModelRef::getPropertyListDouble(const QString& propertyName) const {
	if (!mData) return EMPTY_DOUBLE_LIST;

	std::vector<double> returnList;
	auto				findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getDouble());
		}
	}

	return returnList;
}

std::vector<QColor> ContentModelRef::getPropertyListColor(const QString& propertyName) const {
	if (!mData) return EMPTY_COLOR_LIST;

	std::vector<QColor>	   returnList;
	auto				   findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getColor());
		}
	}

	return returnList;
}

std::vector<QString> ContentModelRef::getPropertyListString(const QString& propertyName) const {
	if (!mData) return EMPTY_STRING_LIST;

	std::vector<QString>	 returnList;
	auto					 findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getString());
		}
	}

	return returnList;
}


std::vector<glm::vec2> ContentModelRef::getPropertyListVec2(const QString& propertyName) const {
	if (!mData) return EMPTY_VEC2_LIST;

	std::vector<glm::vec2> returnList;
	auto				  findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getVec2());
		}
	}

	return returnList;
}

std::vector<glm::vec3> ContentModelRef::getPropertyListVec3(const QString& propertyName) const {
	if (!mData) return EMPTY_VEC3_LIST;

	std::vector<glm::vec3> returnList;
	auto				  findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getVec3());
		}
	}

	return returnList;
}

std::vector<QRectF> ContentModelRef::getPropertyListRect(const QString& propertyName) const {
	if (!mData) return EMPTY_RECTF_LIST;

	std::vector<QRectF>	   returnList;
	auto				   findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnList.emplace_back(it.getRect());
		}
	}

	return returnList;
}

QString ContentModelRef::getPropertyListAsString(const QString& propertyName, const QString& delimiter) const {
	if (!mData) return EMPTY_STRING;

	QString		returnString;
	auto		findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		for (const auto& it : findy->second) {
			returnString.append(it.getValue());
			returnString.append(delimiter);
		}
	}

	return returnString;
}

void ContentModelRef::addPropertyToList(const QString& propertyListName, const QString& value) {
	createData();

	mData->mPropertyLists[propertyListName].emplace_back(ContentProperty(propertyListName, value));
}

void ContentModelRef::setPropertyList(const QString& propertyListName, const std::vector<QString>& value) {
	createData();

	std::vector<ContentProperty> propertyList;
	for (auto& it : value) {
		propertyList.emplace_back(ContentProperty(propertyListName, it));
	}

	mData->mPropertyLists[propertyListName] = propertyList;
}

void ContentModelRef::setPropertyList(const QString& propertyListName, const std::vector<ContentProperty>& value) {
	createData();

	mData->mPropertyLists[propertyListName] = value;
}

void ContentModelRef::clearPropertyList(const QString& propertyName) {
	if (!mData) return;

	auto findy = mData->mPropertyLists.find(propertyName);
	if (findy != mData->mPropertyLists.end()) {
		findy->second.clear();
	}
}

const std::vector<ContentModelRef>& ContentModelRef::getChildren() const {
	if (!mData) return EMPTY_DATAMODELREF_VECTOR;
	return mData->mChildren;
}

ContentModelRef ContentModelRef::getChild(const size_t index) {
	createData();

	if (index < mData->mChildren.size()) {
		return mData->mChildren[index];
	} else if (!mData->mChildren.empty()) {
		return mData->mChildren.back();
	}

	return EMPTY_DATAMODEL;
}

ContentModelRef ContentModelRef::getChildById(const QString& id) {
	createData();

	for (auto& it : mData->mChildren) {
		if (it.getId() == id) return it;
	}

	return EMPTY_DATAMODEL;
}

ContentModelRef ContentModelRef::getChildByName(const QString& childName) const {
	if (!mData || mData->mChildren.empty()) return EMPTY_DATAMODEL;

	if (childName.contains(".")) {
		auto childrens = childName.split(".", Qt::SkipEmptyParts);
		if (childrens.empty()) {
			qWarning() << "ContentModelRef::getChild() Cannot find a child with the name \".\"";
		} else {
			ContentModelRef curChild = getChildByName(childrens.front());
			for (int i = 1; i < childrens.size(); i++) {
				curChild = curChild.getChildByName(childrens[i]);
			}
			return curChild;
		}
	}

	for (auto& it : mData->mChildren) {
		if (it.getName() == childName) return it;
	}

	return EMPTY_DATAMODEL;
}

dsqt::model::ContentModelRef ContentModelRef::getDescendant(const QString& childId) const {
	for (auto& it : getChildren()) {
		if (it.getId() == childId) {
			return it;
		} else {
			auto chillin = it.getDescendant(childId);
			if (!chillin.empty()) {
				return chillin;
			}
		}
	}

	return ContentModelRef();
}

std::vector<ContentModelRef> ContentModelRef::getChildrenWithLabel(const QString& label) const {
	std::vector<ContentModelRef> childrenWithLabel;
	for (auto& it : getChildren()) {
		if (it.getLabel() == label) {
			childrenWithLabel.push_back(it);
		}
	}
	return childrenWithLabel;
}

ContentModelRef ContentModelRef::findChildByPropertyValue(const QString& propertyName, const QString& propertyValue) const {
	for (auto& it : getChildren()) {
		if (it.getPropertyString(propertyName) == propertyValue) {
			return it;
		}
	}
	return ContentModelRef();
}

bool ContentModelRef::hasChild(const QString& name) const {
	return !getChildByName(name).empty();
}

void ContentModelRef::addChild(const ContentModelRef& datamodel) {
	createData();

	mData->mChildren.emplace_back(datamodel);
}

void ContentModelRef::addChild(const ContentModelRef& datamodel, const size_t index) {
	createData();

	if (index < mData->mChildren.size()) {
		mData->mChildren.insert(mData->mChildren.begin() + index, datamodel);
	} else {
		mData->mChildren.emplace_back(datamodel);
	}
}

void ContentModelRef::replaceChild(const dsqt::model::ContentModelRef& datamodel) {
	createData();

	const auto& name = datamodel.getName();
	std::vector<dsqt::model::ContentModelRef> allChillins;
	for (const auto& it : mData->mChildren) {
		if (it.getName() == name) continue;
		allChillins.emplace_back(it);
	}
	allChillins.emplace_back(datamodel);

	setChildren(allChillins);
}

bool ContentModelRef::hasDirectChild(const QString& name) const {
	if (!mData || mData->mChildren.empty()) return false;

	for (const auto& it : mData->mChildren) {
		if (it.getName() == name) return true;
	}

	return false;
}

bool ContentModelRef::hasChildren() const {
	if (!mData) return false;
	return !mData->mChildren.empty();
}

void ContentModelRef::setChildren(const std::vector<dsqt::model::ContentModelRef>& children) {
	createData();
	mData->mChildren = children;
}

void ContentModelRef::clearChildren() {
	if (!mData) return;
	mData->mChildren.clear();
}


void ContentModelRef::setReferences(const QString&											   referenceName,
									std::unordered_map<QString, dsqt::model::ContentModelRef>& reference) {
	createData();
	mData->mReferences[referenceName] = reference;
}

const std::unordered_map<QString, dsqt::model::ContentModelRef>& ContentModelRef::getReferences(const QString& name) const {
	if (!mData) return EMPTY_REFERENCE;
	auto findy = mData->mReferences.find(name);
	if (findy != mData->mReferences.end()) {
		return findy->second;
	}

	return EMPTY_REFERENCE;
}


dsqt::model::ContentModelRef ContentModelRef::getReference(const QString& referenceName, const QString nodeId) const {
	if (!mData) return EMPTY_DATAMODEL;
	auto theReference = getReferences(referenceName);
	if (theReference.empty()) return EMPTY_DATAMODEL;

	auto findy = theReference.find(nodeId);
	if (findy != theReference.end()) {
		return findy->second;
	}

	return EMPTY_DATAMODEL;
}


void ContentModelRef::clearReferences(const QString& name) {
	if (!mData) return;
	auto findy = mData->mReferences.find(name);
	if (findy != mData->mReferences.end()) {
		mData->mReferences.erase(name);
	}
}


void ContentModelRef::clearAllReferences() {
	if (!mData) return;
	mData->mReferences.clear();
}

void ContentModelRef::printTree(const bool verbose, const QString& indent) const {
	if (empty() || !mData) {
		qWarning() << indent << "Empty ContentModel.";
	} else {
		qWarning() << indent << "ContentModel id:" << mData->mId << " name:" << mData->mName << " label:" << mData->mLabel;
		if (verbose) {

			for (auto& it : mData->mProperties) {

				qDebug() << indent << "          prop:" << it.first << " value:" << it.second.getValue();
			}

			for (auto& it : mData->mPropertyLists) {
				for (auto& pit : it.second) {
					qDebug() << indent << "          prop list:" << it.first << " value:" << pit.getValue();
				}
			}
		}

		if (!mData->mChildren.empty()) {

			for (auto& it : mData->mChildren) {
				//	DS_LOG_INFO(indent << "          child:" << it.getName());
				it.printTree(verbose, indent + "  ");
			}
		}
	}
}

QJsonModel* ContentModelRef::getModel(QObject* parent) {
	QJsonModel* model = new QJsonModel(parent);

	QJsonDocument doc;
	doc.setObject(getJson().toObject());
	model->loadJson(doc.toJson());
	return model;
}

QQmlPropertyMap* ContentModelRef::getMap(QObject* parent) const {
	QQmlPropertyMap* map = new QQmlPropertyMap(parent);
	map->insert("uid", QVariant::fromValue(mData->mId));
	map->insert("id", QVariant::fromValue(mData->mId));
	map->insert("name", QVariant::fromValue(mData->mName));
	map->insert("label", QVariant::fromValue(mData->mLabel));
	for (const auto& prop : mData->mProperties) {
		map->insert(prop.first, QVariant::fromValue(prop.second.getValue()));
	}
	for (const auto& propList : mData->mPropertyLists) {
		QVariantList list;
		for (const auto& prop : propList.second) {
			list.append(QVariant::fromValue(prop.getValue()));
		}
		map->insert(propList.first, list);
	}
	QVariantList children;
	for (const auto& child : mData->mChildren) {
		children.append(QVariant::fromValue(child.getMap(map)));
	}
	map->insert("children", children);
	return map;
}

void ContentModelRef::detach() {
	mData.detach();
}


//----------- PRIVATE --------------//
void ContentModelRef::createData() {
	if (!mData) mData.reset(new Data());
}

QJsonValue ContentModelRef::getJson() {
	QJsonObject obj;
	obj.insert("id", mData->mId);
	obj.insert("name", mData->mName);
	obj.insert("label", mData->mLabel);
	for (const auto& prop : mData->mProperties) {
		obj.insert(prop.first, prop.second.getValue());
	}
	QJsonArray children;
	for (auto child : mData->mChildren) {
		children.append(child.getJson());
	}
	obj.insert("children", children);
	return obj;
}

}  // namespace dsqt::model
