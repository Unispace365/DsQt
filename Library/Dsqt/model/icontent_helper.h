#ifndef ICONTENT_HELPER_H
#define ICONTENT_HELPER_H

#include <QObject>
#include <QQmlEngine>
#include "model/content_model.h"

namespace dsqt {
   class DsQmlApplicationEngine;
}
namespace dsqt::model {

/**
 * @brief The IContentHelper class.
 * This class is an interface(abstract class) that
 * is intended as a base class to provide helper functions to
 * standard content in DS projects.
 */
class IContentHelper: public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    IContentHelper(QObject *parent = nullptr);

    struct PlaylistFilter {
        enum class FilterMode { All, PlatformFallback, PlatformOverride };
        QString eventTypeKey;		  // what type of event has the playlist
        QString eventPropertyName;	  // what is the property name on the event
        QString platformPropertyName; // what is the property name on the platform
        QString playlistTypeKey;	  // what type of playlist
        FilterMode	filterMode = FilterMode::PlatformFallback;
    };

    static const QString DEFAULTCATEGORY;
    static const QString ANYCATEGORY;
    static const QString WAFFLESCATEGORY;
    static const QString PRESENTATIONCATEGORY;
    static const QString AMBIENTCATEGORY;

    virtual QString		getCompositeKeyForPlatform()		   = 0;
    virtual ContentModelRef getRecordByUid(const QString& uid) = 0;
    virtual DSResource		getBackgroundForPlatform()			   = 0;

    virtual ContentModelRef getPresentation()			= 0; // getInteractivePlaylist
    virtual ContentModelRef getAmbientPlaylist()		= 0;
    virtual QString		getInitialPresentationUid() = 0;

    virtual std::vector<ContentModelRef> getFilteredPlaylists(const PlaylistFilter& filter)				 = 0;
    virtual std::vector<ContentModelRef> getContentForPlatform()										 = 0; // getAssets
    virtual std::vector<ContentModelRef> getStreamSources(const QString& category = DEFAULTCATEGORY) = 0;
    virtual ContentModelRef				 getStreamSourceForStream(ContentModelRef stream, const QString& category = DEFAULTCATEGORY) = 0;

    virtual std::vector<DSResource> findMediaResources() = 0;

    virtual bool isValidFolder(ContentModelRef model, const QString& category = DEFAULTCATEGORY)	   = 0;
    virtual bool isValidMedia(ContentModelRef model, const QString& category = DEFAULTCATEGORY)		   = 0;
    virtual bool isValidStreamSource(ContentModelRef model, const QString& category = DEFAULTCATEGORY) = 0;
    virtual bool isValidStream(ContentModelRef model, const QString& category = DEFAULTCATEGORY)	   = 0;
    virtual bool isValidPlaylist(ContentModelRef model, const QString& category = DEFAULTCATEGORY)	   = 0;

    virtual QString getMediaPropertyKey(ContentModelRef model, const QString& category = DEFAULTCATEGORY)		= 0;
    virtual QString getStreamMatchKey(ContentModelRef model, const QString& category = DEFAULTCATEGORY)			= 0;
    virtual QString getStreamSourceAddressKey(ContentModelRef model, const QString& category = DEFAULTCATEGORY) = 0;
    virtual QString getStreamSourceTypeKey(ContentModelRef model, const QString& category = DEFAULTCATEGORY)	= 0;

    virtual std::vector<ContentModelRef> getRecordsOfType(const std::vector<ContentModelRef>& records,
                                                          const QString&				  type)=0;

    virtual std::vector<ContentProperty> findAllProperties(const std::vector<ContentModelRef>& records,
                                                           const QString&				   propertyName)=0;
    virtual void setEngine(DsQmlApplicationEngine* engine) = 0;
    virtual ContentModelRef getPlatform()=0;
};
}

#endif // ICONTENT_HELPER_H
