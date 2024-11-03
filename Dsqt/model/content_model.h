#pragma once
#include "qqmlpropertymap.h"
#ifndef DS_CONTENT_CONTENT_MODEL
#define DS_CONTENT_CONTENT_MODEL


#include <model/qjsonmodel.h>
#include <QColor>
#include <QLoggingCategory>
#include <QRect>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QUrl>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>


#include "resource.h"
Q_DECLARE_LOGGING_CATEGORY(lgContentModel)
Q_DECLARE_LOGGING_CATEGORY(lgContentModelVerbose)
namespace dsqt::model {

class QContentModel;

/**
 * \class ContentProperty
 * \brief A single property on a ContentModel.
 *		 For instance, this could be ContentProperty("longitude", 123.456); or ContentProperty("title", "The title
 *of this thing")
 */

class ContentProperty {
  public:
	ContentProperty();
	ContentProperty(const QString& name, const QString& value);
	ContentProperty(const QString& name, const QString& value, const int& valueInt, const double& valueDouble);
	ContentProperty(const ContentProperty& prop)
	  : mName(prop.mName)
	  , mValue(prop.mValue)
	  , mIntValue(prop.mIntValue)
	  , mDoubleValue(prop.mDoubleValue)
	  , mResource(prop.mResource){};
	~ContentProperty() = default;

	/// Get the name of this property
	const QString& getName() const;
	void		   setName(const QString& name);

	const QString&	   getValue() const;
	void			   setValue(const QString& value);
	void			   setValue(const std::wstring& value);
	void			   setValue(const int& value);
	void			   setValue(const double& value);
	void			   setValue(const float& value);
	void			   setValue(const QColor& value);
	// void			   setValue(const ci::ColorA& value);
	void setValue(const glm::vec2& value);
	void setValue(const glm::vec3& value);
	void setValue(const glm::vec4& value);
	void setValue(const QRectF& value);
	// todo: add qvector values as in settings

	// im guesing we don't need to worry about
	// resources as we'll rely on QML's system.
	Resource getResource() const;
	void	 setResource(const dsqt::Resource& resource);

	// replace resources with urls
	void setValue(const QUrl& value);

	/// The name, value and resource are equal
	bool operator==(const ContentProperty&) const;

	bool empty() const;
	/// ------- This value, type converted when called --------- //

	bool   getBool() const;
	int	   getInt() const;
	float  getFloat() const;
	double getDouble() const;
	QUrl   getUrl() const;

	/// The Engine is supplied to look up named colors
	QColor getColor() const;

	const QString&	   getString() const;  // same as getValue(), but supplied here for convenience
	const QString&	   getQString() const;
	// std::wstring       getWString() const;

	glm::vec2 getVec2() const;
	glm::vec3 getVec3() const;
	QRectF	  getRect() const;

  protected:
	QString					  mName;
	QString					  mValue;  // this should always be valid.
	int			  mIntValue;
	double		  mDoubleValue;
	std::shared_ptr<Resource> mResource;
};

class ContentModelRef;
class Data : public QSharedData {
  public:
	Data();
	Data(const Data& other);
	~Data();


	QString															mName;
	QString															mLabel;
	void*															mUserData;
	QString															mId;
	std::map<QString, ContentProperty>								mProperties;
	std::map<QString, std::vector<ContentProperty>>					mPropertyLists;
	std::vector<ContentModelRef>									mChildren;
	std::map<QString, std::unordered_map<QString, ContentModelRef>> mReferences;
};

/**
 * \class ContentModelRef
 * \brief A nodal hierarchy-based generic content model
 *		 Each ContentModelRef points to an underlying Data object by a shared pointer, so you can copy these
 *instances around freely Each ContentModelRef has:
 *			* A series of properties (e.g. Title, Body, ImageResource, Latitude, etc)
 *			* A series of children, all ContentModelRefs themselves, to build a hierarchy
 *			* A name, which can be used to look up models. Typically the name of a table from a database
 *			* A label, for identifying models to humans
 *			* An Id, which has no guarantee of uniqueness, that typically comes from a database, and can be used to
 *look up models
 */
class ContentModelRef {
  public:
	/// TODO: auto validation (e.g. exists, is a date, media meets certain qualifications, etc)
	/// TODO: remove child

	ContentModelRef();
	ContentModelRef(const ContentModelRef& other);
	ContentModelRef& operator=(const ContentModelRef& other);
	ContentModelRef(const QString& name, const QString& id = 0, const QString& label = "");

	/// Enables doing `if (mModel) ...` to check if model is valid
	operator bool() const { return !empty(); }

	/// Get the id for this item
	const QString& getId() const;
	void		   setId(const QString& id);

	/// Get the name of this item
	/// Name is generally inherited by the table or thing this belongs to
	/// This is used in the getChildByName()
	const QString& getName() const;
	void		   setName(const QString& name);

	/// Get the label for this item
	/// This is a helpful name or display name for this thing
	const QString& getLabel() const;
	void		   setLabel(const QString& label);

	/// Get the user data pointer.
	void* getUserData() const;
	void  setUserData(void* userData);

	/// If this item has no data, value, name, id, properties or children
	bool empty() const;

	/// Removes all data (children, properties, name, id, etc). After calling this, empty() will return true
	void clear();

	/// Makes a copy of this content.
	/// This make a brand new ContentModelRef with a different underlying data object, so you can modify the two
	/// independently
	dsqt::model::ContentModelRef duplicate() const;

	/// Tests if this ContentModelRef has the same Id, Name, Label and underlying data pointer
	bool operator==(const ContentModelRef&) const;

	bool weakEqual(const ContentModelRef& b) const;

	bool equalChildrenAndReferences(const ContentModelRef&							  b,
									std::vector<std::pair<const void*, const void*>>& alreadyChecked) const;

	bool operator!=(const ContentModelRef&) const;

	/// Use this for looking stuff up only. Recommend using the other functions to manage the list
	const std::map<QString, ContentProperty>& getProperties() const;
	void									  setProperties(const std::map<QString, ContentProperty>& newProperties);
	/// This can return an empty property, which is why it's const.
	/// If you want to modify a property, use the setProperty() function

	ContentProperty getProperty(const QString& propertyName) const;
	QString			getPropertyValue(const QString& propertyName) const;
	// QString		getPropertyValue(const QString& propertyName) const;

	bool getPropertyBool(const QString& propertyName) const;
	// bool			getPropertyBool(const QString& propertyName) const;

	int	   getPropertyInt(const QString& propertyName) const;
	float  getPropertyFloat(const QString& propertyName) const;
	double getPropertyDouble(const QString& propertyName) const;

	/// The Engine is supplied to look up named colors
	QColor getPropertyColor(const QString& propertyName) const;

	QString	  getPropertyString(const QString& propertyName) const;
	// QString	  getPropertyQString(const QString& propertyName) const;
	glm::vec2 getPropertyVec2(const QString& propertyName) const;
	glm::vec3 getPropertyVec3(const QString& propertyName) const;
	glm::vec4 getPropertyVec4(const QString& propertyName) const;
	QRectF	  getPropertyRect(const QString& propertyName) const;
	QUrl	  getPropertyUrl(const QString& propertyName) const;
	Resource  getPropertyResource(const QString& propertyName) const;

	/// Set the property with a given name
	void setProperty(const QString& propertyName, ContentProperty& theProp);
	void setProperty(const QString& propertyName, char* value);
	void setProperty(const QString& propertyName, const QString& value);
	void setProperty(const QString& propertyName, const int& value);
	void setProperty(const QString& propertyName, const double& value);
	void setProperty(const QString& propertyName, const float& value);
	void setProperty(const QString& propertyName, const QColor& value);

	void setProperty(const QString& propertyName, const glm::vec2& value);
	void setProperty(const QString& propertyName, const glm::vec3& value);
	void setProperty(const QString& propertyName, const glm::vec4& value);
	void setProperty(const QString& propertyName, const QRectF& value);
	void setProperty(const QString& propertyName, const QUrl& value);
	void setPropertyResource(const QString& propertyName, const Resource& resource);

	/// property lists are stored separately from regular properties
	const std::map<QString, std::vector<ContentProperty>>& getAllPropertyLists() const;
	const std::vector<ContentProperty>&					   getPropertyList(const QString& propertyName) const;
	std::vector<bool>									   getPropertyListBool(const QString& propertyName) const;
	std::vector<int>									   getPropertyListInt(const QString& propertyName) const;
	std::vector<float>									   getPropertyListFloat(const QString& propertyName) const;
	std::vector<double>									   getPropertyListDouble(const QString& propertyName) const;
	std::vector<QColor>									   getPropertyListColor(const QString& propertyName) const;
	// std::vector<ci::ColorA>                                    getPropertyListColorA(ds::ui::SpriteEngine&, const QString&
	// propertyName) const;
	std::vector<QString>   getPropertyListString(const QString& propertyName) const;
	std::vector<QString>   getPropertyListQString(const QString& propertyName) const;
	std::vector<glm::vec2> getPropertyListVec2(const QString& propertyName) const;
	std::vector<glm::vec3> getPropertyListVec3(const QString& propertyName) const;
	std::vector<QRectF>	   getPropertyListRect(const QString& propertyName) const;

	/// Returns the list as a delimiter-separated string
	QString getPropertyListAsString(const QString& propertyName, const QString& delimiter = "; ") const;
	QString getPropertyListAsQString(const QString& propertyName, const QString& delimiter = "; ") const;

	/// Adds this to a property list
	void addPropertyToList(const QString& propertyListName, const QString& value);

	/// Replaces a property list
	void setPropertyList(const QString& propertyListName, const std::vector<QString>& value);
	void setPropertyList(const QString& propertyListName, const std::vector<ContentProperty>& value);

	/// Clears the list for a specific property
	void clearPropertyList(const QString& propertyName);

	/// Gets all of the children
	/// Don't modify the children here, use the other functions
	const std::vector<ContentModelRef>& getChildren() const;

	/// If no children exist, returns an empty data model
	/// If index is greater than the size of the children, returns the last child
	ContentModelRef getChild(const size_t index);

	/// Get the first child that matches this id
	/// If no children exist or match that id, returns an empty data model
	ContentModelRef getChildById(const QString& id);

	/// Get the first child that matches this name
	/// Can get nested children using dot notation. for example:
	/// getChildByName("the_stories.chapter_one.first_paragraph"); If no children exist or match that id, returns an
	/// empty data model
	ContentModelRef getChildByName(const QString& childName) const;

	/// Looks through the entire tree to find a child that matches the name and id.
	/// For instance, if you have a branched tree several levels deep and need to find a specific node.
	/// Depends on children having a consistent name and unique id.
	ContentModelRef getDescendant(const QString& childId) const;

	/// Looks through all direct children, and returns all children that have a given label.
	/// Useful for models that have children from more than one table
	/// \note By default, labels are in the form "sql_table_name row"
	std::vector<ContentModelRef> getChildrenWithLabel(const QString& label) const;

	/// Get first direct decendant where 'propertyName' has a value of 'propertyValue'
	/// Returns an empty model if no match is found
	ContentModelRef findChildByPropertyValue(const QString& propertyName, const QString& propertyValue) const;

	/// Adds this child to the end of this children list, or at the index supplied
	void addChild(const ContentModelRef& datamodel);
	void addChild(const ContentModelRef& datamodel, const size_t index);

	/// If there's a direct descendant with the name, replaces it, adds it if it doesn't exist
	void replaceChild(const dsqt::model::ContentModelRef& datamodel);

	/// Is there a child with this name?
	bool hasChild(const QString& name) const;
	bool hasDirectChild(const QString& name) const;

	/// Is there at least one child?
	bool hasChildren() const;

	/// Replaces all children
	void setChildren(const std::vector<dsqt::model::ContentModelRef>& children);

	/// Removes all children
	void clearChildren();

	/// Adds a reference map with the corresponding string name
	void setReferences(const QString& referenceName, std::unordered_map<QString, dsqt::model::ContentModelRef>& reference);

	/// Gets a map of all the references for the given name. If you need to modify the map, make a copy and set it
	/// again using setReference
	const std::unordered_map<QString, dsqt::model::ContentModelRef>& getReferences(const QString& referenceName) const;

	/// Returns a content model from a specific reference by the reference name and the node id
	dsqt::model::ContentModelRef getReference(const QString& referenceName, const QString nodeId) const;

	/// Clears the reference map at the specified name
	void clearReferences(const QString& name);

	/// Removes all references
	void clearAllReferences();

	/// Logs this, it's properties, and all it's children recursively
	void printTree(const bool verbose, const QString& indent = "") const;

	/// get a json model for this content model and its children;
	QJsonModel* getModel(QObject* parent = nullptr);
	/// break this content model from other copies. (copy on write behavior)
	// void detach();
	/// get a property map for this content model
	QQmlPropertyMap* getMap(QObject* parent = nullptr) const;

	void detach();

  private:
	void								  createData();
	QJsonValue							  getJson();
	QExplicitlySharedDataPointer<dsqt::model::Data> mData;
};

}  // namespace dsqt::model

#endif
