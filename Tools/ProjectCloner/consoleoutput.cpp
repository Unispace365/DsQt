// ConsoleOutput.cpp
#include "consoleoutput.h"
#include <QDateTime>
#include <QThread>

ConsoleOutput* ConsoleOutput::s_instance = nullptr;

ConsoleOutput::ConsoleOutput(QObject *parent)
    : QObject(parent)
{
    s_instance = this;

    // Install Qt message handler
    m_oldHandler = qInstallMessageHandler(messageHandler);
}

ConsoleOutput::~ConsoleOutput()
{
    // Restore default message handler
    qInstallMessageHandler(nullptr);
    s_instance = nullptr;
}

void ConsoleOutput::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!s_instance) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString prefix;

    switch (type) {
    case QtDebugMsg:
        prefix = "[DEBUG]";
        break;
    case QtInfoMsg:
        prefix = "[INFO]";
        break;
    case QtWarningMsg:
        prefix = "[WARNING]";
        break;
    case QtCriticalMsg:
        prefix = "[CRITICAL]";
        break;
    case QtFatalMsg:
        prefix = "[FATAL]";
        break;
    }

    QString formatted = QString("%1 %2 %3").arg(timestamp, prefix, msg);

// Also output to original console
    if (s_instance->m_oldHandler) {
        s_instance->m_oldHandler(type, context, msg);
    }

    s_instance->appendMessageThreadSafe(formatted);
}

void ConsoleOutput::appendMessage(const QString &message)
{
    appendMessageThreadSafe(message);
}

void ConsoleOutput::appendMessageThreadSafe(const QString &message)
{
    QMutexLocker locker(&m_mutex);

    m_model.addMessage(message);
    emit newMessage(message);
}

void ConsoleOutput::clear()
{
    QMutexLocker locker(&m_mutex);
    m_model.clear();
}

void ConsoleOutput::setMaxLines(int lines)
{
    if (m_maxLines != lines) {
        m_maxLines = lines;
        emit maxLinesChanged();
    }
}

// StreamRedirect implementation
StreamRedirect::StreamRedirect(std::ostream &stream, ConsoleOutput *output)
    : m_stream(stream), m_output(output)
{
    m_oldBuf = m_stream.rdbuf(this);
}

StreamRedirect::~StreamRedirect()
{
    m_stream.rdbuf(m_oldBuf);
}

StreamRedirect::int_type StreamRedirect::overflow(int_type ch)
{
    if (ch == '\n') {
        if (m_output) {
            m_output->appendMessage(QString::fromStdString(m_buffer));
        }
        m_buffer.clear();
    } else if (ch != EOF) {
        m_buffer += static_cast<char>(ch);
    }
    return ch;
}

std::streamsize StreamRedirect::xsputn(const char* s, std::streamsize n)
{
    m_buffer.append(s, n);
    size_t pos = 0;
    while ((pos = m_buffer.find('\n')) != std::string::npos) {
        if (m_output) {
            m_output->appendMessage(QString::fromStdString(m_buffer.substr(0, pos)));
        }
        m_buffer.erase(0, pos + 1);
    }
    return n;
}

// ConsoleOutput.cpp - ConsoleModel implementation
ConsoleModel::ConsoleModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConsoleModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    //QMutexLocker locker(&m_mutex);
    return m_messages.count();
}

QVariant ConsoleModel::data(const QModelIndex &index, int role) const
{
    //QMutexLocker locker(&m_mutex);

    if (index.row() < 0 || index.row() >= m_messages.count())
        return QVariant();

    const QString &message = m_messages[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case MessageRole:
        return message;
    case TimestampRole:
        // Extract timestamp if needed
        if (message.length() > 12)
            return message.left(12);
        return QString();
    case TypeRole:
        // Extract type [DEBUG], [WARNING], etc.
        if (message.contains("[DEBUG]")) return "debug";
        if (message.contains("[WARNING]")) return "warning";
        if (message.contains("[CRITICAL]")) return "critical";
        if (message.contains("[ERROR]")) return "error";
        if (message.contains("[FATAL]")) return "fatal";
        return "info";
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ConsoleModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[MessageRole] = "message";
    roles[TimestampRole] = "timestamp";
    roles[TypeRole] = "messageType";
    return roles;
}

void ConsoleModel::addMessage(const QString &message)
{
    //QMutexLocker locker(&m_mutex);

    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());
    m_messages.append(message);
    endInsertRows();

    // Remove old messages if we exceed max
    if (m_messages.count() > m_maxLines) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_messages.removeFirst();
        endRemoveRows();
    }
}

void ConsoleModel::clear()
{
    //QMutexLocker locker(&m_mutex);
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

