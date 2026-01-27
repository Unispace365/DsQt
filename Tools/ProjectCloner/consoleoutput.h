// ConsoleOutput.h
#pragma once
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <QStringListModel>
#include <QMutex>
#include <QMutexLocker>
#include <QRangeModel>
#include <iostream>
#include <sstream>

class ConsoleModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum ConsoleRoles {
        MessageRole = Qt::UserRole + 1,
        TimestampRole,
        TypeRole
    };

    explicit ConsoleModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addMessage(const QString &message);
    void clear();
    void setMaxLines(int max) { m_maxLines = max; }

private:
    QStringList m_messages;
    mutable QMutex m_mutex;
    int m_maxLines = 1000;
};

class ConsoleOutput : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(ConsoleModel* model READ model CONSTANT)
    Q_PROPERTY(int maxLines READ maxLines WRITE setMaxLines NOTIFY maxLinesChanged)

public:
    explicit ConsoleOutput(QObject *parent = nullptr);
    ~ConsoleOutput();

    ConsoleModel* model() { return &m_model; }
    int maxLines() const { return m_maxLines; }
    void setMaxLines(int lines);

    Q_INVOKABLE void clear();
    Q_INVOKABLE void appendMessage(const QString &message);

signals:
    void maxLinesChanged();
    void newMessage(const QString &message);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void appendMessageThreadSafe(const QString &message);

    ConsoleModel m_model;
    QMutex m_mutex;
    int m_maxLines = 1000;
    static ConsoleOutput* s_instance;
    QtMessageHandler m_oldHandler=nullptr;
};

// Custom streambuf for capturing std::cout/std::cerr
class StreamRedirect : public std::streambuf
{
public:
    StreamRedirect(std::ostream &stream, ConsoleOutput *output);
    ~StreamRedirect();

protected:
    virtual int_type overflow(int_type ch) override;
    virtual std::streamsize xsputn(const char* s, std::streamsize n) override;

private:
    std::ostream &m_stream;
    std::streambuf *m_oldBuf;
    ConsoleOutput *m_output;
    std::string m_buffer;

};
