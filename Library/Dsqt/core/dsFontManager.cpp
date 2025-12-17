#include "dsFontManager.h"
#include "dsSettings.h"
#include <QGuiApplication>
#include <QDebug>
#include <QFile>
#include <QDir>

namespace dsqt {

DsFontManager::DsFontManager(DsSettingsRef settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

int DsFontManager::loadFonts(std::function<QString(const QString&)> expandFunc)
{
    if (!m_settings) {
        qWarning() << "DsFontManager::loadFonts: No settings object provided";
        return 0;
    }

    // Clear previously loaded fonts
    m_loadedFonts.clear();

    // Get the font paths array from settings
    QVariantList fontPaths = m_settings->getOr<QVariantList>("engine.font.paths", QVariantList());

    int successCount = 0;

    for (const QVariant& pathVariant : std::as_const(fontPaths)) {
        QVariantMap pathMap = pathVariant.toMap();

        QString path = pathMap.value("path").toString();
        QString name = pathMap.value("name").toString();

        if (path.isEmpty()) {
            qWarning() << "DsFontManager::loadFonts: Empty font path in configuration";
            continue;
        }

        // Expand path placeholders if expand function is provided
        QString expandedPath = expandFunc ? expandFunc(path) : path;

        // Check if file exists
        if (!QFile::exists(expandedPath)) {
            QString errorMsg = QString("Font file does not exist: %1").arg(expandedPath);
            qWarning() << "DsFontManager::loadFonts:" << errorMsg;
            emit fontLoadError(expandedPath, errorMsg);
            continue;
        }

        // Load the font
        int fontId = QFontDatabase::addApplicationFont(expandedPath);

        if (fontId == -1) {
            QString errorMsg = QString("Failed to load font: %1").arg(expandedPath);
            qWarning() << "DsFontManager::loadFonts:" << errorMsg;
            emit fontLoadError(expandedPath, errorMsg);
            continue;
        }

        // Store the loaded font information
        FontPath fontPath;
        fontPath.path = expandedPath;
        fontPath.name = name;
        fontPath.fontId = fontId;
        m_loadedFonts.push_back(fontPath);

        // Get the actual font families that were loaded
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        qDebug() << "DsFontManager::loadFonts: Successfully loaded font" << name
                 << "from" << expandedPath
                 << "with families:" << families;

        emit fontLoaded(expandedPath, name);
        successCount++;
    }

    qInfo() << "DsFontManager::loadFonts: Loaded" << successCount << "of" << fontPaths.size() << "fonts";
    return successCount;
}

QFont DsFontManager::createDefaultFont()
{
    if (!m_settings) {
        qWarning() << "DsFontManager::createDefaultFont: No settings object provided";
        return QFont();
    }

    auto map = m_settings->get<QVariantMap>("engine.font.default");
    if(!map.has_value()) {
        qWarning() << "DsFontManager::createDefaultFont: No default font found";
        return QFont();
    }
    QFont font;

    // Font family settings
    QString family = m_settings->getOr<QString>("engine.font.default.family", "");
    if (!family.isEmpty()) {
        font.setFamily(family);
    }

    // Alternative: use families array for fallbacks
    QVariantList families = m_settings->getOr<QVariantList>("engine.font.default.families", QVariantList());
    if (!families.isEmpty()) {
        QStringList familyList;
        for (const QVariant& f : std::as_const(families)) {
            familyList << f.toString();
        }
        if (!familyList.isEmpty()) {
            font.setFamilies(familyList);
        }
    }

    // Basic font properties
    font.setBold(m_settings->getOr<bool>("engine.font.default.bold", false));
    font.setItalic(m_settings->getOr<bool>("engine.font.default.italic", false));
    font.setUnderline(m_settings->getOr<bool>("engine.font.default.underline", false));
    font.setOverline(m_settings->getOr<bool>("engine.font.default.overline", false));
    font.setStrikeOut(m_settings->getOr<bool>("engine.font.default.strikeOut", false));

    // Size settings - prefer pointSizeF over pointSize, and pointSize over pixelSize
    double pointSizeF = m_settings->getOr<double>("engine.font.default.pointSizeF", 0.0);
    if (pointSizeF > 0.0) {
        font.setPointSizeF(pointSizeF);
    } else {
        int pointSize = m_settings->getOr<int>("engine.font.default.pointSize", 0);
        if (pointSize > 0) {
            font.setPointSize(pointSize);
        } else {
            int pixelSize = m_settings->getOr<int>("engine.font.default.pixelSize", 0);
            if (pixelSize > 0) {
                font.setPixelSize(pixelSize);
            }
        }
    }

    // Weight
    int weight = m_settings->getOr<int>("engine.font.default.weight", 400);
    font.setWeight(weightIntToEnum(weight));

    // Stretch factor
    int stretch = m_settings->getOr<int>("engine.font.default.stretch", 100);
    font.setStretch(stretch);

    // Style name
    QString styleName = m_settings->getOr<QString>("engine.font.default.styleName", "");
    if (!styleName.isEmpty()) {
        font.setStyleName(styleName);
    }

    // Style
    QString style = m_settings->getOr<QString>("engine.font.default.style", "StyleNormal");
    font.setStyle(parseStyle(style));

    // Capitalization
    QString capitalization = m_settings->getOr<QString>("engine.font.default.capitalization", "MixedCase");
    font.setCapitalization(parseCapitalization(capitalization));

    // Fixed pitch
    font.setFixedPitch(m_settings->getOr<bool>("engine.font.default.fixedPitch", false));

    // Kerning
    font.setKerning(m_settings->getOr<bool>("engine.font.default.kerning", true));

    // Letter spacing
    QString letterSpacingType = m_settings->getOr<QString>("engine.font.default.letterSpacingType", "PercentageSpacing");
    double letterSpacing = m_settings->getOr<double>("engine.font.default.letterSpacing", 100.0);
    QFont::SpacingType spacingType = parseSpacingType(letterSpacingType);
    font.setLetterSpacing(spacingType, letterSpacing);

    // Word spacing
    double wordSpacing = m_settings->getOr<double>("engine.font.default.wordSpacing", 0.0);
    font.setWordSpacing(wordSpacing);

    // Hinting preference
    QString hintingPref = m_settings->getOr<QString>("engine.font.default.hintingPreference", "PreferDefaultHinting");
    font.setHintingPreference(parseHintingPreference(hintingPref));

    // Style hint
    QString styleHint = m_settings->getOr<QString>("engine.font.default.styleHint", "AnyStyle");
    font.setStyleHint(parseStyleHint(styleHint));

    // Style strategy
    QString styleStrategy = m_settings->getOr<QString>("engine.font.default.styleStrategy", "PreferDefault");
    font.setStyleStrategy(parseStyleStrategy(styleStrategy));

    // OpenType features (if Qt version supports it)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVariantMap features = m_settings->getOr<QVariantMap>("engine.font.default.features", QVariantMap());
    for (auto it = features.begin(); it != features.end(); ++it) {
        QString tag = it.key();
        if (tag.size() == 4) {  // OpenType tags are 4 characters
            auto tagOptional = QFont::Tag::fromString(tag);
            if(tagOptional.has_value()) {
                font.setFeature(tagOptional.value(), it.value().toUInt());
            }
        }
    }
#endif

    // Variable font axes (if Qt version supports it)
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QVariantMap variableAxes = m_settings->getOr<QVariantMap>("engine.font.default.variableAxes", QVariantMap());
    for (auto it = variableAxes.begin(); it != variableAxes.end(); ++it) {
        QByteArray tag = it.key().toUtf8();
        if (tag.size() == 4) {  // Variable font axis tags are 4 characters
            auto tagOptional = QFont::Tag::fromString(it.key());
            if(tagOptional.has_value()){
                font.setVariableAxis(tagOptional.value(), it.value().toFloat());
            }
        }
    }
#endif

    return font;
}

void DsFontManager::setApplicationDefaultFont(const QFont& font)
{
    QGuiApplication::setFont(font);
}

QFont::Style DsFontManager::parseStyle(const QString& style) const
{
    if (style == "StyleItalic") return QFont::StyleItalic;
    if (style == "StyleOblique") return QFont::StyleOblique;
    return QFont::StyleNormal;
}

QFont::Capitalization DsFontManager::parseCapitalization(const QString& cap) const
{
    if (cap == "AllUppercase") return QFont::AllUppercase;
    if (cap == "AllLowercase") return QFont::AllLowercase;
    if (cap == "SmallCaps") return QFont::SmallCaps;
    if (cap == "Capitalize") return QFont::Capitalize;
    return QFont::MixedCase;
}

QFont::StyleHint DsFontManager::parseStyleHint(const QString& hint) const
{
    if (hint == "SansSerif") return QFont::SansSerif;
    if (hint == "Helvetica") return QFont::Helvetica;
    if (hint == "Serif") return QFont::Serif;
    if (hint == "Times") return QFont::Times;
    if (hint == "TypeWriter") return QFont::TypeWriter;
    if (hint == "Courier") return QFont::Courier;
    if (hint == "OldEnglish") return QFont::OldEnglish;
    if (hint == "Decorative") return QFont::Decorative;
    if (hint == "Monospace") return QFont::Monospace;
    if (hint == "Fantasy") return QFont::Fantasy;
    if (hint == "Cursive") return QFont::Cursive;
    return QFont::AnyStyle;
}

QFont::StyleStrategy DsFontManager::parseStyleStrategy(const QString& strategy) const
{
    if (strategy == "PreferBitmap") return QFont::PreferBitmap;
    if (strategy == "PreferDevice") return QFont::PreferDevice;
    if (strategy == "PreferOutline") return QFont::PreferOutline;
    if (strategy == "ForceOutline") return QFont::ForceOutline;
    if (strategy == "PreferMatch") return QFont::PreferMatch;
    if (strategy == "PreferQuality") return QFont::PreferQuality;
    if (strategy == "PreferAntialias") return QFont::PreferAntialias;
    if (strategy == "NoAntialias") return QFont::NoAntialias;
    if (strategy == "NoSubpixelAntialias") return QFont::NoSubpixelAntialias;
    if (strategy == "PreferNoShaping") return QFont::PreferNoShaping;
    if (strategy == "NoFontMerging") return QFont::NoFontMerging;
    return QFont::PreferDefault;
}

QFont::HintingPreference DsFontManager::parseHintingPreference(const QString& pref) const
{
    if (pref == "PreferNoHinting") return QFont::PreferNoHinting;
    if (pref == "PreferVerticalHinting") return QFont::PreferVerticalHinting;
    if (pref == "PreferFullHinting") return QFont::PreferFullHinting;
    return QFont::PreferDefaultHinting;
}

QFont::SpacingType DsFontManager::parseSpacingType(const QString& type) const
{
    if (type == "AbsoluteSpacing") return QFont::AbsoluteSpacing;
    return QFont::PercentageSpacing;
}

QFont::Weight DsFontManager::weightIntToEnum(int weight) const
{
    // Map numeric weight values (100-900) to QFont::Weight enum
    // This follows the CSS font-weight specification
    if (weight <= 100) return QFont::Thin;
    if (weight <= 200) return QFont::ExtraLight;
    if (weight <= 300) return QFont::Light;
    if (weight <= 400) return QFont::Normal;
    if (weight <= 500) return QFont::Medium;
    if (weight <= 600) return QFont::DemiBold;
    if (weight <= 700) return QFont::Bold;
    if (weight <= 800) return QFont::ExtraBold;
    return QFont::Black;
}

} // namespace dsqt
