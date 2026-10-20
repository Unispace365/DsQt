#include "packagecatalog.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>
#include <QRegularExpression>
#include <algorithm>

namespace {
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

// UI order is independent of the registry baseline and dependency resolution.
// Stable releases always come first, even when a development base is newer.
bool numericIdentifier(const QString &value)
{
    return !value.isEmpty() && std::all_of(value.cbegin(), value.cend(), [](QChar c) {
        return c >= QLatin1Char('0') && c <= QLatin1Char('9');
    });
}

int compareNumber(QString left, QString right)
{
    while (left.size() > 1 && left.startsWith(QLatin1Char('0'))) left.remove(0, 1);
    while (right.size() > 1 && right.startsWith(QLatin1Char('0'))) right.remove(0, 1);
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    return QString::compare(left, right, Qt::CaseSensitive);
}

int compareIdentifiers(const QStringList &left, const QStringList &right)
{
    for (qsizetype i = 0; i < std::min(left.size(), right.size()); ++i) {
        const bool leftNumeric = numericIdentifier(left[i]);
        const bool rightNumeric = numericIdentifier(right[i]);
        const int result = leftNumeric && rightNumeric ? compareNumber(left[i], right[i])
            : leftNumeric != rightNumeric ? (leftNumeric ? -1 : 1)
            : QString::compare(left[i], right[i], Qt::CaseSensitive);
        if (result) return result;
    }
    return left.size() == right.size() ? 0 : left.size() < right.size() ? -1 : 1;
}

bool packageBefore(const QVariant &left, const QVariant &right)
{
    static const QRegularExpression pattern(
        R"(^([0-9]+(?:\.[0-9]+)*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z.-]+)?$)");
    const auto a = left.toMap();
    const auto b = right.toMap();
    const auto aMatch = pattern.match(a.value("version").toString());
    const auto bMatch = pattern.match(b.value("version").toString());
    if (aMatch.hasMatch() != bMatch.hasMatch()) return aMatch.hasMatch();
    if (!aMatch.hasMatch()) return a.value("version").toString() > b.value("version").toString();
    const auto aPre = aMatch.captured(2);
    const auto bPre = bMatch.captured(2);
    if (aPre.isEmpty() != bPre.isEmpty()) return aPre.isEmpty();
    const int baseOrder = compareIdentifiers(aMatch.captured(1).split('.'), bMatch.captured(1).split('.'));
    if (baseOrder) return baseOrder > 0;
    // The numbered dev format succeeds the old branch-specific previews.
    if (aPre.startsWith("dev.") != bPre.startsWith("dev.")) return aPre.startsWith("dev.");
    const int previewOrder = compareIdentifiers(aPre.split('.'), bPre.split('.'));
    if (previewOrder) return previewOrder > 0;
    return a.value("portVersion").toInt() > b.value("portVersion").toInt();
}

bool stablePackage(const QString &version)
{
    static const QRegularExpression pattern(R"(^[0-9]+(?:\.[0-9]+)*(?:\+[0-9A-Za-z.-]+)?$)");
    return pattern.match(version).hasMatch();
}

const QStringList moduleOrder = {"Core", "Bridge", "Waffles", "Qsys", "Spout", "Touch", "TouchEngine"};
const QString registryUrl = "https://github.com/Unispace365/vcpkg-registry";
}

PackageCatalog::PackageCatalog(QObject *parent) : QObject(parent)
{
    m_bundled = readJson(":/dsqt/dsqt-modules.json");
    m_path = qEnvironmentVariable("DSQT_LOCAL_PACKAGE_ROOT");
    if (!m_path.isEmpty()) m_source = "local";
    else m_path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/DsQt";
    m_cache = qEnvironmentVariable("DSQT_CLONER_CACHE_DIR", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)) + "/registry.git";
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, &m_git, &QProcess::kill);
    connect(&m_git, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        auto callback = std::move(m_callback);
        if (callback) callback(code == 0 && status == QProcess::NormalExit, m_git.readAllStandardOutput());
    });
    connect(&m_git, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_timeout.stop();
        auto callback = std::move(m_callback);
            if (callback) callback(false, {});
        }
    });
    if (m_source != "registry") inspectPath();
}

PackageCatalog::~PackageCatalog()
{
    m_timeout.stop();
    disconnect(&m_git, nullptr, this, nullptr);
    if (m_git.state() != QProcess::NotRunning) {
        m_git.kill();
        m_git.waitForFinished(1000);
    }
}

void PackageCatalog::setSource(const QString &value)
{
    if (m_busy || value == m_source || !QStringList{"installed", "local", "registry"}.contains(value)) return;
    m_source = value;
    m_error.clear();
    if (value == "installed") m_path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/DsQt";
    else if (value == "local") {
        m_path = qEnvironmentVariable("DSQT_LOCAL_PACKAGE_ROOT");
        if (m_path.isEmpty()) {
            const auto localAppData = qEnvironmentVariable("LOCALAPPDATA");
            if (!localAppData.isEmpty()) m_path = QDir::fromNativeSeparators(localAppData) + "/DsQt/vcpkg";
        }
    }
    inspectPath();
}

void PackageCatalog::setPackagePath(const QString &value)
{
    if (m_busy || m_path == value) return;
    m_path = QDir::fromNativeSeparators(value);
    inspectPath();
}

void PackageCatalog::setPackageIndex(int value)
{
    if (m_busy || value < 0 || value >= m_packages.size() || value == m_index) return;
    m_index = value;
    emit changed();
}

void PackageCatalog::inspectPath()
{
    m_local = {};
    m_localMetadata = {};
    if (m_source == "local") {
        m_error.clear();
        auto packagePath = m_path;
        const auto defaultFile = m_path + "/dsqt-local-default.json";
        if (QFile::exists(defaultFile)) {
            const auto localDefault = readJson(defaultFile);
            const auto packageRoot = localDefault.value("packageRoot").toString();
            if (localDefault.value("formatVersion").toInt() != 1 || !QDir::isAbsolutePath(packageRoot)) {
                m_error = tr("Invalid local package default. Run build_and_install.bat -vcpkg -qt <version> again.");
                emit changed();
                return;
            }
            // Keep the store as the selection; explicit snapshot paths still
            // pin a package, while the default follows successful local builds.
            packagePath = QDir::fromNativeSeparators(packageRoot);
        }
        m_local = readJson(packagePath + "/dsqt-local-package.json");
        m_localMetadata = readJson(packagePath + "/source/Library/dsqt-modules.json");
    } else if (m_source == "installed") {
        m_localMetadata = readJson(m_path + "/lib/cmake/Dsqt/dsqt-modules.json");
        if (m_localMetadata.isEmpty()) m_localMetadata = readJson(m_path + "/share/dsqt/dsqt-modules.json");
    }
    emit changed();
}

QJsonObject PackageCatalog::metadata() const
{
    if (m_source == "registry") {
        if (m_index >= 0 && m_index < m_packages.size())
            return QJsonObject::fromVariantMap(m_packages[m_index].toMap().value("metadata").toMap());
        return {};
    }
    return m_localMetadata;
}

QStringList PackageCatalog::selectedModules() const
{
    const auto specs = (metadata().isEmpty() ? m_bundled : metadata()).value("modules").toObject();
    QStringList result = m_requested;
    if (!result.contains("Core")) result.prepend("Core");
    for (int pass = 0; pass < moduleOrder.size(); ++pass) {
        const auto current = result;
        for (const auto &name : current)
            for (const auto &dependency : specs.value(name).toObject().value("requires").toArray())
                if (!result.contains(dependency.toString())) result.append(dependency.toString());
    }
    QStringList ordered;
    for (const auto &name : moduleOrder) if (result.contains(name)) ordered.append(name);
    return ordered;
}

QVariantList PackageCatalog::modules() const
{
    const auto specs = (metadata().isEmpty() ? m_bundled : metadata()).value("modules").toObject();
    const auto selected = selectedModules();
    QVariantList result;
    for (const auto &name : moduleOrder) {
        const auto spec = specs.value(name).toObject();
        QStringList dependents;
        for (const auto &other : selected)
            if (specs.value(other).toObject().value("requires").toArray().contains(name)) dependents.append(other);
        const bool locked = name == "Core" || !dependents.isEmpty();
        QString detail = spec.value("description").toString();
        if (locked) detail += name == "Core" ? tr(" · Required") : tr(" · Required by %1").arg(dependents.join(", "));
        result.append(QVariantMap{{"name", name}, {"label", spec.value("label").toString(name)},
            {"detail", detail}, {"selected", selected.contains(name)}, {"locked", locked}});
    }
    return result;
}

void PackageCatalog::setModule(const QString &name, bool enabled)
{
    if (m_busy || name == "Core" || !moduleOrder.contains(name)) return;
    if (enabled && !m_requested.contains(name)) m_requested.append(name);
    if (!enabled) m_requested.removeAll(name);
    emit changed();
}

QString PackageCatalog::minimumQt() const
{
    const auto data = metadata().isEmpty() ? m_bundled : metadata();
    auto result = QVersionNumber::fromString(data.value("minimumQt").toString());
    for (const auto &name : selectedModules())
        result = qMax(result, QVersionNumber::fromString(data.value("modules").toObject().value(name).toObject().value("minimumQt").toString()));
    // This version of the application template itself requires Qt 6.12.
    return qMax(result, QVersionNumber(6, 12, 0)).toString();
}

QString PackageCatalog::exactQt() const
{
    if (m_source == "local") return m_local.value("qtVersion").toString();
    if (m_source == "installed") return metadata().value("builtQtVersion").toString();
    return {};
}

QString PackageCatalog::requirements() const
{
    if (metadata().isEmpty()) return tr("Package Qt requirements are not verified.\nThis application template requires Qt 6.12.0 or newer.");
    QString text = tr("Minimum Qt %1 · Windows x64 / MSVC").arg(minimumQt());
    if (!exactQt().isEmpty()) text += tr("\nThis build requires the exact Qt %1 kit.").arg(exactQt());
    text += tr("\nCore uses Canvas Painter, so disabling Waffles does not lower this package's Qt minimum.");
    return text;
}

bool PackageCatalog::acceptsQt(const QString &version) const
{
    return QVersionNumber::fromString(version) >= QVersionNumber::fromString(minimumQt())
        && (exactQt().isEmpty() || version == exactQt());
}

bool PackageCatalog::ready() const
{
    const auto data = metadata();
    if (m_busy || !m_error.isEmpty() || data.value("formatVersion").toInt() != 1 || data.value("templateApi").toInt() != 1) return false;
    if (m_source != "registry" && exactQt().isEmpty()) return false;
    if (m_source == "local" && (m_local.value("formatVersion").toInt() != 1 || m_local.value("packageVersion").toString().isEmpty())) return false;
    const auto built = data.value("builtModules").toArray();
    for (const auto &name : selectedModules()) {
        if (!data.value("modules").toObject().contains(name)) return false;
        if (m_source == "installed" && !built.contains(name)) return false;
    }
    return m_source != "registry" || !m_registryCommit.isEmpty();
}

QString PackageCatalog::statusMessage() const
{
    if (m_busy) return tr("Reading published packages…");
    if (!m_error.isEmpty()) return m_error;
    if (m_source == "registry" && m_packages.isEmpty()) return tr("Refresh to read published versions using your Git credentials.");
    if (m_source == "registry" && m_index < 0) return tr("Select a published package. Development builds require an explicit selection.");
    if (metadata().isEmpty()) return tr("Module requirements are unavailable for this package. Select a package built with module metadata.");
    if (!ready()) return tr("This package does not provide the selected modules or a compatible template API.");
    if (m_source == "registry") return tr("The selected version and registry commit will be pinned in your project.");
    if (m_source == "local") return tr("DsQt %1 - Ready. Other selected modules will be built from this source package if needed.").arg(m_local.value("packageVersion").toString());
    return tr("Package verified. Select the modules your application needs.");
}

QJsonObject PackageCatalog::selection() const
{
    QJsonObject result{{"source", m_source}, {"path", m_path}, {"modules", QJsonArray::fromStringList(selectedModules())},
        {"minimumQt", minimumQt()}, {"exactQt", exactQt()}, {"metadata", metadata()}};
    if (m_source == "registry" && m_index >= 0) {
        const auto package = QJsonObject::fromVariantMap(m_packages[m_index].toMap());
        result.insert("version", package.value("version"));
        result.insert("portVersion", package.value("portVersion"));
        result.insert("baseline", m_registryCommit);
        result.insert("repository", registryUrl);
    }
    return result;
}

void PackageCatalog::runGit(const QStringList &args, std::function<void(bool, QByteArray)> callback)
{
    m_callback = std::move(callback);
    m_git.start("git", args);
    m_timeout.start(60000);
}

void PackageCatalog::refreshRegistry()
{
    if (m_busy) return;
    m_busy = true;
    m_error.clear();
    emit changed();
    const bool exists = QFile::exists(m_cache + "/HEAD");
    QDir().mkpath(QFileInfo(m_cache).absolutePath());
    const QStringList args = exists
        ? QStringList{"--git-dir=" + m_cache, "fetch", "origin", "main"}
        : QStringList{"clone", "--bare", registryUrl, m_cache};
    runGit(args, [this, exists](bool ok, QByteArray) {
        if (!ok) { m_busy = false; m_error = tr("Cannot read the registry. Check Git access to Unispace365/vcpkg-registry, then refresh."); emit changed(); return; }
        runGit({"--git-dir=" + m_cache, "rev-parse", exists ? "FETCH_HEAD" : "refs/heads/main"}, [this](bool valid, QByteArray output) {
            if (!valid) { m_busy = false; m_error = tr("Cannot resolve the registry commit."); emit changed(); return; }
            m_registryCommit = QString::fromUtf8(output).trimmed();
            readVersions();
        });
    });
}

void PackageCatalog::readVersions()
{
    runGit({"--git-dir=" + m_cache, "show", m_registryCommit + ":versions/d-/dsqt.json"}, [this](bool ok, QByteArray output) {
        m_packages.clear();
        m_index = -1;
        if (ok) {
            for (const auto &value : QJsonDocument::fromJson(output).object().value("versions").toArray()) {
                const auto item = value.toObject();
                QString version;
                for (const auto &key : {"version-semver", "version", "version-string", "version-date"})
                    if (item.contains(key)) version = item.value(key).toString();
                m_packages.append(QVariantMap{{"version", version}, {"portVersion", item.value("port-version").toInt()},
                    {"label", version}, {"tree", item.value("git-tree").toString()}});
            }
        }
        if (m_packages.isEmpty()) { m_busy = false; m_error = tr("No DsQt versions were found in the registry."); emit changed(); return; }
        std::stable_sort(m_packages.begin(), m_packages.end(), packageBefore);
        readNextPackage(0);
    });
}

void PackageCatalog::readNextPackage(int index)
{
    if (index == m_packages.size()) {
        m_busy = false;
        m_index = -1;
        for (int i = 0; i < m_packages.size(); ++i) {
            const auto item = m_packages[i].toMap();
            const auto data = item.value("metadata").toMap();
            if (stablePackage(item.value("version").toString())
                && data.value("formatVersion").toInt() == 1 && data.value("templateApi").toInt() == 1) {
                m_index = i;
                break;
            }
        }
        emit changed();
        return;
    }
    const auto tree = m_packages[index].toMap().value("tree").toString();
    runGit({"--git-dir=" + m_cache, "show", tree + ":dsqt-modules.json"}, [this, index](bool ok, QByteArray output) {
        auto item = m_packages[index].toMap();
        const auto data = ok ? QJsonDocument::fromJson(output).object() : QJsonObject();
        item.insert("metadata", data.toVariantMap());
        item.insert("label", item.value("version").toString() + (item.value("portVersion").toInt() ? " #" + item.value("portVersion").toString() : "")
            + (stablePackage(item.value("version").toString()) ? tr(" - Stable") : tr(" - Development"))
            + (data.isEmpty() ? tr(" — requirements unavailable") : tr(" — Qt %1+").arg(data.value("minimumQt").toString())));
        m_packages[index] = item;
        readNextPackage(index + 1);
    });
}
