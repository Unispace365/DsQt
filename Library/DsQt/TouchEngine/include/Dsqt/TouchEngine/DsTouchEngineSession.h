#pragma once

#include <Dsqt/TouchEngine/DsTouchEngineTypes.h>

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QtQuick/QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace dsqt::touchengine {

namespace detail {
class DsTouchEngineSessionPrivate;
class DsTouchEngineViewRenderer;
}

class DsTouchEngineSession : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DsTouchEngineSession)

    Q_PROPERTY(QString componentPath READ componentPath WRITE setComponentPath NOTIFY componentPathChanged FINAL)
    Q_PROPERTY(QString preferredEnginePath READ preferredEnginePath WRITE setPreferredEnginePath NOTIFY preferredEnginePathChanged FINAL)
    Q_PROPERTY(double frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged FINAL)
    Q_PROPERTY(dsqt::touchengine::DsTouchEngineTypes::TimeMode timeMode READ timeMode WRITE setTimeMode NOTIFY timeModeChanged FINAL)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(dsqt::touchengine::DsTouchEngineTypes::State state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(dsqt::touchengine::DsTouchEngineTypes::GraphicsApi graphicsApi READ graphicsApi NOTIFY graphicsApiChanged FINAL)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged FINAL)
    Q_PROPERTY(QVariantList links READ links NOTIFY linksChanged FINAL)
    Q_PROPERTY(quint64 frameCount READ frameCount NOTIFY frameFinished FINAL)

public:
    explicit DsTouchEngineSession(QObject *parent = nullptr);
    ~DsTouchEngineSession() override;

    QString componentPath() const;
    void setComponentPath(const QString &path);

    QString preferredEnginePath() const;
    void setPreferredEnginePath(const QString &path);

    double frameRate() const;
    void setFrameRate(double framesPerSecond);

    DsTouchEngineTypes::TimeMode timeMode() const;
    void setTimeMode(DsTouchEngineTypes::TimeMode mode);

    bool isRunning() const;
    void setRunning(bool running);

    DsTouchEngineTypes::State state() const;
    DsTouchEngineTypes::GraphicsApi graphicsApi() const;
    bool isLoaded() const;
    bool isReady() const;
    QString errorString() const;
    QVariantList links() const;
    quint64 frameCount() const;

    Q_INVOKABLE void load();
    Q_INVOKABLE void unload();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void requestFrame();

    Q_INVOKABLE void setInputValue(const QString &link, const QVariant &value);
    Q_INVOKABLE void clearInputValue(const QString &link);
    Q_INVOKABLE QVariant outputValue(const QString &link) const;

    Q_INVOKABLE void setTextureInput(const QString &link, QQuickItem *sourceItem);
    Q_INVOKABLE void clearTextureInput(const QString &link);

signals:
    void componentPathChanged();
    void preferredEnginePathChanged();
    void frameRateChanged();
    void timeModeChanged();
    void runningChanged();
    void stateChanged();
    void graphicsApiChanged();
    void errorStringChanged();
    void linksChanged();
    void outputValueChanged(const QString &link, const QVariant &value);
    void frameFinished(quint64 frameNumber);

private:
    friend class DsTouchEngineView;
    friend class detail::DsTouchEngineViewRenderer;

    void wakeViews();
    void drainEvents();

    std::unique_ptr<detail::DsTouchEngineSessionPrivate> d;
};

} // namespace dsqt::touchengine
