#include "dsQmlContentHelper.h"
#include <QString>
#include "core/dsEnvironment.h"
#include "dsresource.h"
Q_LOGGING_CATEGORY(lgContentHelper, "model.contenthelper")
Q_LOGGING_CATEGORY(lgContentHelperVerbose, "model.contenthelper.verbose")


namespace dsqt::model {
using namespace Qt::Literals::StringLiterals;
ContentHelper::ContentHelper(QObject *parent)
    : IContentHelper(parent)
{
    /*auto foldersCount = mEngine.getWafflesSettings().countSetting("content:folder:key");

    for (int i = 0; i < foldersCount; ++i) {
        auto folder	  = mEngine.getWafflesSettings().getString("content:folder:key", i, "");
        auto category = mEngine.getWafflesSettings().getAttribute("content:folder:key", 0, "category", DEFAULTCATEGORY);
        mAcceptableFolders[category].push_back(folder);
        if (mAcceptableFolders[DEFAULTCATEGORY].isEmpty()) mAcceptableFolders[DEFAULTCATEGORY].push_back(folder);
    }

    auto mediaCount = mEngine.getWafflesSettings().countSetting("content:media:key");
    for (int i = 0; i < mediaCount; ++i) {
        auto media					 = mEngine.getWafflesSettings().getString("content:media:key", i);
        auto mediaProp				 = mEngine.getWafflesSettings().getAttribute("content:media:key", i, "property_key", "");
        auto category				 = mEngine.getWafflesSettings().getAttribute("content:media:key", i, "category", DEFAULTCATEGORY);
        mMediaProps[category][media] = mediaProp;
        if (mMediaProps[DEFAULTCATEGORY][media].isEmpty()) mMediaProps[DEFAULTCATEGORY][media] = mediaProp;
        mAcceptableMedia[category].push_back(media);
        if (mAcceptableMedia[DEFAULTCATEGORY].isEmpty()) mAcceptableMedia[DEFAULTCATEGORY].push_back(media);
    }

    auto playlistCount = mEngine.getWafflesSettings().countSetting("content:playlist:key");
    for (int i = 0; i < playlistCount; ++i) {
        auto playlist = mEngine.getWafflesSettings().getString("content:playlist:key", i, "");
        auto category = mEngine.getWafflesSettings().getAttribute("content:playlist:key", i, "category", DEFAULTCATEGORY);
        if (!playlist.isEmpty()) {
            mAcceptablePlaylists[category].push_back(playlist);
            if (mAcceptablePlaylists[DEFAULTCATEGORY].isEmpty()) mAcceptablePlaylists[DEFAULTCATEGORY].push_back(playlist);
        }
    }

    auto streamCount = mEngine->getAppSettings()..get.countSetting("content:stream:key");
    for (int i = 0; i < streamCount; ++i) {
        auto stream			 = mEngine.getWafflesSettings().getString("content:stream:key", i, "");
        auto streamMatchProp = mEngine.getWafflesSettings().getAttribute("content:stream:key", i, "match_key", "");

        auto category					   = mEngine.getWafflesSettings().getAttribute("content:stream:key", i, "category", DEFAULTCATEGORY);
        mStreamMatchProp[category][stream] = streamMatchProp;
        if (mStreamMatchProp[DEFAULTCATEGORY][stream].isEmpty()) mStreamMatchProp[DEFAULTCATEGORY][stream] = streamMatchProp;
        mAcceptableStreams[category].push_back(stream);
        if (mAcceptableStreams[DEFAULTCATEGORY].isEmpty()) mAcceptableStreams[DEFAULTCATEGORY].push_back(stream);
    }

    auto streamSourceCount = mEngine.getWafflesSettings().countSetting("content:stream_source:key");
    for (int i = 0; i < streamSourceCount; ++i) {
        auto streamSource			 = mEngine.getWafflesSettings().getString("content:stream_source:key", i, "");
        auto streamSourceAddressProp = mEngine.getWafflesSettings().getAttribute("content:stream_source:key", i, "address_key", "");
        auto streamSourceTypeProp	 = mEngine.getWafflesSettings().getAttribute("content:stream_source:key", i, "streamtype_key", "");
        auto streamMatchProp		 = mEngine.getWafflesSettings().getAttribute("content:stream_source:key", i, "match_key", "");
        auto category				 = mEngine.getWafflesSettings().getAttribute("content:stream_source:key", i, "category", DEFAULTCATEGORY);
        mStreamSourceAddressProps[category][streamSource] = streamSourceAddressProp;
        if (mStreamSourceAddressProps[DEFAULTCATEGORY][streamSource].isEmpty())
            mStreamSourceAddressProps[DEFAULTCATEGORY][streamSource] = streamSourceAddressProp;
        mStreamSourceTypeProps[category][streamSource] = streamSourceTypeProp;
        if (mStreamSourceTypeProps[DEFAULTCATEGORY][streamSource].isEmpty())
            mStreamSourceTypeProps[DEFAULTCATEGORY][streamSource] = streamSourceTypeProp;
        mStreamMatchProp[category][streamSource] = streamMatchProp;
        if (mStreamMatchProp[DEFAULTCATEGORY][streamSource].isEmpty()) mStreamMatchProp[DEFAULTCATEGORY][streamSource] = streamMatchProp;
        mAcceptableStreamSources[category].push_back(streamSource);
        if (mAcceptableStreamSources[DEFAULTCATEGORY].isEmpty()) mAcceptableStreamSources[DEFAULTCATEGORY].push_back(streamSource);
    }*/
}

ContentModelRef ContentHelper::getPlatform()
{
    auto platform = mEngine->getContentRoot().getChildByName("platform");
    return platform.getChild(0);
    // auto newPlatform = platforms.getChild(0).duplicate();
    // if(newPlatform == m_platform) return;
    // m_platform = newPlatform;
    // m_platform_qml = m_platform.getQml();
    // emit platformChanged();
}

void ContentHelper::setEngine(DsQmlApplicationEngine *engine)
{
    mEngine = engine;
}

QStringList ContentHelper::splitCategory(QString category)
{
    return category.isEmpty() ? QStringList(DEFAULTCATEGORY) : category.split(",");
}


QString ContentHelper::getCompositeKeyForPlatform() {
    // TODO: get key value pairs from waffles_app.xml
    auto key = mEngine->getAppSettings()->getOr<QString>("composite:key", "");
    return key;
}

ContentModelRef ContentHelper::getRecordByUid(const QString& uid) {
    return mEngine->getContentRoot().getReference("all_records", uid);
}

DSResource ContentHelper::getBackgroundForPlatform() {

    auto	 platform = mEngine->getContentHelper()->getPlatform();
    if (platform.empty()) return {};

    // get all the events scheduled for this platform, already sorted in order of importance
    const auto& allPlatformEvents = platform.getChildByName("current_events").getChildren();

    // check if events have playlists
    if (!allPlatformEvents.empty()) {
        for (const auto& event : allPlatformEvents) {
            if (event.getPropertyString("type_key") == "some_event" && !event.getPropertyResource("content-browsing-background").empty()) {

                return event.getPropertyResource("content-browsing-background");
            }
        }
    } else {
        qCWarning(lgContentHelper())<<u"No scheduled background for platform"_s<< platform.getPropertyString("name");
    }

    if (!platform.getPropertyResource("content-browsing-background").empty()) {
        return platform.getPropertyResource("content-browsing-background");
    }

    qCWarning(lgContentHelperVerbose()) << "No platform background for platform '" << platform.getPropertyString("name") << "'. Using default background.";

    return {DsEnvironment::expandq("%APP%/data/images/default_background.png")};
}

ContentModelRef ContentHelper::getPresentation() {
    PlaylistFilter filter;
    filter.playlistTypeKey		= "presentation";
    filter.platformPropertyName = "default_presentation";
    auto playlists				= getFilteredPlaylists(filter);
    if (playlists.empty()) {
        return {};
    }
    return playlists.front();
}

ContentModelRef ContentHelper::getAmbientPlaylist() {
    PlaylistFilter filter;
    filter.playlistTypeKey = "ambient_playlist";
    auto playlists		   = getFilteredPlaylists(filter);
    if (playlists.empty()) {
        return {};
    }
    return playlists.front();
}

QString ContentHelper::getInitialPresentationUid() {
    auto model = getPresentation();
    if (model.empty()) {
        return {};
    }
    return model.getId();

    // PlaylistFilter filter;
    // filter.playlistTypeKey = "presentation";
    // filter.platformPropertyName = "default_presentation";
    // auto playlists = getFilteredPlaylists(filter);
    // if (playlists.isEmpty()) {
    //	return {};
    // }
    // return playlists.front().getUid();
}

std::vector<ContentModelRef> ContentHelper::getContentForPlatform() {

    auto	 allValid	= getPlatform().getChildByName("content").getChildren();
    auto	 allContent = std::vector<ContentModelRef>();

    for (const auto& value : allValid) {
        allContent.push_back(value);
    }


    auto platformChildren = getPlatform().getChildren();
    for (const auto& value : platformChildren) {
        allContent.push_back(value);
    }
    return allContent;
}

std::vector<DSResource> ContentHelper::findMediaResources() {
    return {};
}

std::vector<ContentModelRef> ContentHelper::getFilteredPlaylists(const PlaylistFilter& filter) {
    auto eventPropName	  = filter.eventPropertyName.isEmpty() ? "playlist" : filter.eventPropertyName;
    auto platformPropName = filter.platformPropertyName.isEmpty() ? "default_playlist" : filter.platformPropertyName;


    auto	 platform = getPlatform();
    if (platform.empty()) return {};

    ContentModelRef thePlaylist;

    // get all the events scheduled for this platform, already sorted in order of importance
    const auto&					 allPlatformEvents = platform.getChildByName("all_events").getChildren();
    std::vector<ContentModelRef> thePlaylists;

    // check if events have playlists
    if (!allPlatformEvents.empty()) {
        for (const auto& event : allPlatformEvents) {
            auto eventTypeKey = event.getPropertyString("type_key");
            if ((eventTypeKey == filter.eventTypeKey || filter.eventTypeKey.isEmpty()) && !event.getPropertyString(eventPropName).isEmpty()) {

                const auto playlistSelection = event.getPropertyString(eventPropName).split(",");
                // check each playlist for correct type_key
                for (const auto& playlistUid : playlistSelection) {
                    auto playlist = getRecordByUid(playlistUid);
                    if (filter.playlistTypeKey.isEmpty() || playlist.getPropertyString("type_key") == filter.playlistTypeKey) {
                        thePlaylists.push_back(playlist);
                    }
                }
            }
        }
    } else {
        // DS_LOG_VERBOSE(1, "No scheduled ambient playlist for platform" << platform.getPropertyString("name"))
    }

    // if there is no event playlist scheduled then check default platform playlists
    const auto platformDefaultAmbientPlaylistId = platform.getPropertyString(platformPropName).split(",");
    if (!platformDefaultAmbientPlaylistId.isEmpty()) {
        // add to the playlist if filterMode is All, or clear the playlist if filterMode is PlatformOverride and if the
        // filterMode is PlatformFallback and the playlist is isEmpty skip this step
        if (filter.filterMode == PlaylistFilter::FilterMode::All) {
            for (const auto& playlistUid : platformDefaultAmbientPlaylistId) {
                auto playlist = getRecordByUid(playlistUid);
                if (filter.playlistTypeKey.isEmpty() || playlist.getPropertyString("type_key") == filter.playlistTypeKey) {
                    thePlaylists.push_back(playlist);
                }
            }
        } else if (filter.filterMode == PlaylistFilter::FilterMode::PlatformOverride) {
            thePlaylists.clear();
            for (const auto& playlistUid : platformDefaultAmbientPlaylistId) {
                auto playlist = getRecordByUid(playlistUid);
                if (filter.playlistTypeKey.isEmpty() || playlist.getPropertyString("type_key") == filter.playlistTypeKey) {
                    thePlaylists.push_back(playlist);
                }
            }
        } else if (filter.filterMode == PlaylistFilter::FilterMode::PlatformFallback && thePlaylists.empty()) {
            for (const auto& playlistUid : platformDefaultAmbientPlaylistId) {
                auto playlist = getRecordByUid(playlistUid);
                if (filter.playlistTypeKey.isEmpty() || playlist.getPropertyString("type_key") == filter.playlistTypeKey) {
                    thePlaylists.push_back(playlist);
                }
            }
        }
    }

    return thePlaylists;
}

bool ContentHelper::isValidFolder(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat = cat.trimmed();
        auto folders  = mAcceptableFolders[cleanCat];

        if (std::find(folders.begin(), folders.end(), key) != folders.end()) {
            return true;
        }
        if (std::find(folders.begin(), folders.end(), type) != folders.end()) {
            return true;
        }
    }

    return false;
}

bool ContentHelper::isValidMedia(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat = cat.trimmed();

        if (std::find(mAcceptableMedia[cleanCat].begin(), mAcceptableMedia[cleanCat].end(), key) != mAcceptableMedia[cleanCat].end()) {
            return true;
        }
        if (std::find(mAcceptableMedia[cleanCat].begin(), mAcceptableMedia[cleanCat].end(), type) != mAcceptableMedia[cleanCat].end()) {
            return true;
        }
    }

    return false;
}

bool ContentHelper::isValidPlaylist(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat  = cat.trimmed();
        auto playlists = mAcceptablePlaylists[cleanCat];

        if (std::find(playlists.begin(), playlists.end(), key) != playlists.end()) {
            return true;
        }
        if (std::find(playlists.begin(), playlists.end(), type) != playlists.end()) {
            return true;
        }
    }

    return false;
}

QString ContentHelper::getMediaPropertyKey(ContentModelRef model, const QString& category) {
    auto& props				 = mMediaProps[category.isEmpty() ? DEFAULTCATEGORY : category];
    auto  theType			 = model.getPropertyString("type_key");
    auto  theTypeUid		 = model.getPropertyString("type_uid");
    auto  media_property_key = props[theTypeUid];
    media_property_key		 = media_property_key.isEmpty() ? props[theType] : media_property_key;
    media_property_key		 = media_property_key.isEmpty() ? "media" : media_property_key;
    return media_property_key;
}

std::vector<ContentModelRef> ContentHelper::getStreamSources(const QString& category) {


    auto						 platformModel = getPlatform();
    auto						 kids		   = platformModel.getChildren();
    std::vector<ContentModelRef> sources;
    for (const auto& submodel : kids) {
        if (isValidStreamSource(submodel, category)) {
            sources.push_back(submodel);
        }
    }
    return sources;
}

ContentModelRef ContentHelper::getStreamSourceForStream(ContentModelRef stream, const QString& category) {
    if (isValidStream(stream, category)) {
        auto streamMatchKey = getStreamMatchKey(stream, category);
        auto sources		= getStreamSources(category);
        for (auto source : sources) {
            auto sourceMatchKey = getStreamMatchKey(source, category);
            auto streamMatch	= stream.getPropertyString(streamMatchKey);
            auto sourceMatch	= source.getPropertyString(sourceMatchKey);
            if (sourceMatch == streamMatch) {
                return source;
            }
        }
    }
    return {};
}

bool ContentHelper::isValidStreamSource(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat = cat.trimmed();
        auto sources  = mAcceptableStreamSources[cleanCat];
        if (std::find(sources.begin(), sources.end(), key) != sources.end()) {
            return true;
        }
        if (std::find(sources.begin(), sources.end(), type) != sources.end()) {
            return true;
        }
    }
    return false;
}

bool ContentHelper::isValidStream(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat = cat.trimmed();
        auto streams  = mAcceptableStreams[cleanCat];
        if (std::find(streams.begin(), streams.end(), key) != streams.end()) {
            return true;
        }
        if (std::find(streams.begin(), streams.end(), type) != streams.end()) {
            return true;
        }
    }
    return false;
}

QString ContentHelper::getStreamMatchKey(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat = cat.trimmed();
        auto matchKey = mStreamMatchProp[cleanCat][key];
        if (!matchKey.isEmpty()) {
            return matchKey;
        }
        matchKey = mStreamMatchProp[cleanCat][type];
        if (!matchKey.isEmpty()) {
            return matchKey;
        }
    }
    return {};
}

QString ContentHelper::getStreamSourceAddressKey(ContentModelRef model, const QString& category) {
    auto categories = splitCategory(category);
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    for (auto& cat : categories) {
        // trim whitespace from cat using std::find_not_last_of and std::find_not_first_of functions
        auto cleanCat  = cat.trimmed();
        auto streamKey = mStreamSourceAddressProps[cleanCat][key];
        if (!streamKey.isEmpty()) {
            return streamKey;
        }
        streamKey = mStreamSourceAddressProps[cleanCat][type];
        if (!streamKey.isEmpty()) {
            return streamKey;
        }
    }
    return {};
}

QString ContentHelper::getStreamSourceTypeKey(ContentModelRef model, const QString& category) {
    auto categories = category.isEmpty() ? QStringList(DEFAULTCATEGORY) : category.split(",");
    auto type		= model.getPropertyString("type_uid");
    auto key		= model.getPropertyString("type_key");
    // return the type key for the stream source
    for (auto& cat : categories) {
        auto cleanCat	   = cat.trimmed();
        auto streamTypeKey = mStreamSourceTypeProps[cleanCat][key];
        if (!streamTypeKey.isEmpty()) {
            return streamTypeKey;
        }
        streamTypeKey = mStreamSourceTypeProps[cleanCat][type];
        if (!streamTypeKey.isEmpty()) {
            return streamTypeKey;
        }
    }
    return {};
}

std::vector<ContentModelRef> ContentHelper::getRecordsOfType(const std::vector<ContentModelRef>& records,
                                                             const QString&					 type) {
    auto allOfType = std::vector<ContentModelRef>();
    getRecordsByType(records, type, allOfType);
    return allOfType;
}

std::vector<ContentProperty> ContentHelper::findAllProperties(const std::vector<ContentModelRef>& records,
                                                              const QString&				  propertyName) {
    auto allProps = std::vector<ContentProperty>();
    getPropertyByName(records, propertyName, allProps);
    return allProps;
}

void ContentHelper::getRecordsByUid(const std::vector<ContentModelRef>& records, const QString& uid,
                                    std::vector<ContentModelRef>& result) {
    for (const auto& it : records) {
        if (it.getId() == uid || it.getPropertyString("uid") == uid) {
            result.push_back(it);
        } else {
            getRecordsByUid(it.getChildren(), uid, result);
        }
    }
}

void ContentHelper::getRecordsByType(const std::vector<ContentModelRef>& records, const QString& type,
                                     std::vector<ContentModelRef>& result) {
    for (const auto& it : records) {
        if (it.getPropertyString("type_key") == type) {
            result.push_back(it);
        } else {
            getRecordsByType(it.getChildren(), type, result);
        }
    }
}

void ContentHelper::getPropertyByName(const std::vector<ContentModelRef>& records, const QString& propertyName,
                                      std::vector<ContentProperty>& result) {
    for (const auto& it : records) {
        auto prop = it.getProperty(propertyName);
        if (!prop.empty()) result.push_back(prop);

        const auto& children = it.getChildren();
        if (!children.empty()) getPropertyByName(children, propertyName, result);
    }
}

}
