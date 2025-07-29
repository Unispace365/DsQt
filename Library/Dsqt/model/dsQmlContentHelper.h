#ifndef DSCONTENTHELPER_H
#define DSCONTENTHELPER_H


#include <QObject>
#include <QQmlEngine>
#include "dsIContentHelper.h"
#include "dsQmlApplicationEngine.h"

Q_DECLARE_LOGGING_CATEGORY(lgContentHelper)
Q_DECLARE_LOGGING_CATEGORY(lgContentHelperVerbose)
namespace dsqt::model {
class ContentHelper : public IContentHelper
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DSContentHelper)
public:
    explicit ContentHelper(QObject *parent = nullptr);

signals:

    // IContentHelper interface
public:
    virtual QString getCompositeKeyForPlatform() override;
    virtual ContentModelRef getRecordByUid(const QString &uid) override;
    virtual dsqt::DSResource getBackgroundForPlatform() override;
    virtual ContentModelRef getPresentation() override;
    virtual ContentModelRef getAmbientPlaylist() override;
    virtual QString getInitialPresentationUid() override;
    virtual std::vector<ContentModelRef> getFilteredPlaylists(const PlaylistFilter &filter) override;
    virtual std::vector<ContentModelRef> getContentForPlatform() override;
    virtual std::vector<ContentModelRef> getStreamSources(const QString &category) override;
    virtual ContentModelRef getStreamSourceForStream(ContentModelRef stream, const QString &category) override;
    virtual std::vector<DSResource> findMediaResources() override;
    virtual bool isValidFolder(ContentModelRef model, const QString &category) override;
    virtual bool isValidMedia(ContentModelRef model, const QString &category) override;
    virtual bool isValidStreamSource(ContentModelRef model, const QString &category) override;
    virtual bool isValidStream(ContentModelRef model, const QString &category) override;
    virtual bool isValidPlaylist(ContentModelRef model, const QString &category) override;
    virtual QString getMediaPropertyKey(ContentModelRef model, const QString &category) override;
    virtual QString getStreamMatchKey(ContentModelRef model, const QString &category) override;
    virtual QString getStreamSourceAddressKey(ContentModelRef model, const QString &category) override;
    virtual QString getStreamSourceTypeKey(ContentModelRef model, const QString &category) override;
    virtual std::vector<ContentModelRef> getRecordsOfType(const std::vector<ContentModelRef> &records, const QString &type) override;
    virtual std::vector<ContentProperty> findAllProperties(const std::vector<ContentModelRef> &records, const QString &propertyName) override;
    virtual void setEngine(DsQmlApplicationEngine* engine) override;
    virtual ContentModelRef getPlatform() override;

protected:
    static void getRecordsByUid(const std::vector<ContentModelRef>& records, const QString& uid,
                                std::vector<ContentModelRef>& result);
    static void getRecordsByType(const std::vector<ContentModelRef>& records, const QString& type,
                                 std::vector<ContentModelRef>& result);
    static void getPropertyByName(const std::vector<ContentModelRef> &records, const QString &propertyName, std::vector<ContentProperty> &result);

    std::unordered_map<QString, std::vector<QString>> mAcceptableFolders;
    std::unordered_map<QString, std::vector<QString>> mAcceptableMedia;
    // std::unordered_map<QString, std::vector<QString>> mAcceptablePresentations;
    std::unordered_map<QString, std::vector<QString>>					  mAcceptablePlaylists;
    std::unordered_map<QString, std::vector<QString>>					  mAcceptableStreamSources;
    std::unordered_map<QString, std::vector<QString>>					  mAcceptableStreams;
    std::unordered_map<QString, std::unordered_map<QString, QString>> mStreamSourceAddressProps;
    std::unordered_map<QString, std::unordered_map<QString, QString>> mStreamSourceTypeProps;
    std::unordered_map<QString, std::unordered_map<QString, QString>> mStreamMatchProp;
    std::unordered_map<QString, std::unordered_map<QString, QString>> mMediaProps;
    DsQmlApplicationEngine* mEngine;
private:
    QStringList splitCategory(QString category);
};
}
#endif // DSCONTENTHELPER_H
