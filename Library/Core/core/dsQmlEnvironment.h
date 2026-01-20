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
 * expansion of strings and URLs.
 *
 * It is designed to be used within a QML context and cannot be created
 * directly in QML.
 */
class DsQmlEnvironment : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(DsEnvironment)
    QML_UNCREATABLE("Aint no need for you to be making one.")

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
};

} // namespace dsqt

#endif // DSQMLENVIRONMENT_H
