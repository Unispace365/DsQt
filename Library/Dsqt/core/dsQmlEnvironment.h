#ifndef DSQMLENVIRONMENT_H
#define DSQMLENVIRONMENT_H

#include <QObject>
#include <QQmlEngine>

namespace dsqt {

/**
 * @class DsQmlEnvironment
 * @brief Provides environment-related functionality for QML integration.
 *
 * This class acts as a bridge between the DsEnvironment and QML, allowing
 * expansion of strings and URLs, and management of platform names.
 * It is designed to be used within a QML context and cannot be created
 * directly in QML.
 */
class DsQmlEnvironment : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEnvironment)
    QML_UNCREATABLE("Aint no need for you to be making one.")

    /**
     * @property platformName
     * @brief The name of the current platform.
     *
     * This property holds the name of the platform and emits a signal
     * when it changes.
     */
    Q_PROPERTY(QString platformName READ platformName WRITE setPlatformName NOTIFY platformNameChanged)

  public:
    /**
     * @brief Constructs a DsQmlEnvironment object.
     * @param parent The parent QObject (optional).
     */
    explicit DsQmlEnvironment(QObject* parent = nullptr);

    /**
     * @brief Expands a string using environment variables.
     * @param string The input string to expand.
     * @return The expanded string.
     */
    Q_INVOKABLE const QString expand(const QString& string);

    /**
     * @brief Expands a string to a URL using environment variables.
     * @param string The input string to expand.
     * @return The expanded URL.
     */
    Q_INVOKABLE const QUrl    expandUrl(const QString& string);
    // Q_INVOKABLE QString getLocalFolder();

    /**
     * @brief Gets the current platform name.
     * @return The platform name.
     */
    QString platformName() const { return m_platformName; }

    /**
     * @brief Sets the platform name.
     * @param name The new platform name.
     */
    void    setPlatformName(const QString& name) {
        if (name == m_platformName) return;
        m_platformName = name;
        emit platformNameChanged();
    }

  signals:
    /**
     * @brief Signal emitted when the platform name changes.
     */
    void platformNameChanged(); // Signal emitted when platformName changes.

  private:
    /**
     * @brief Updates the environment state immediately.
     *
     * This method is called internally to refresh the platform information.
     */
    void updateNow();

  private:
    QString m_platformName{"Unknown Platform"};
};

} // namespace dsqt

#endif // DSQMLENVIRONMENT_H
