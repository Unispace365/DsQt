#ifndef DSFONTMANAGER_H
#define DSFONTMANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

namespace dsqt {

class DsSettings;
using DsSettingsRef = std::shared_ptr<DsSettings>;

/**
 * @brief The DsFontManager class manages font loading and configuration from TOML settings
 *
 * This class reads font paths from engine.font.paths array and creates a default font
 * based on engine.font.default table configuration.
 */
class DsFontManager : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Font path entry structure
     */
    struct FontPath {
        QString path;
        QString name;
        int fontId = -1; // QFontDatabase id after loading
    };

    /**
     * @brief Construct a new DsFontManager
     * @param settings Shared pointer to DsSettings containing font configuration
     * @param parent Parent QObject
     */
    explicit DsFontManager(DsSettingsRef settings, QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~DsFontManager() = default;

    /**
     * @brief Load all fonts specified in engine.font.paths
     * @param expandFunc Function to expand path placeholders like %APP%
     * @return Number of successfully loaded fonts
     */
    int loadFonts(std::function<QString(const QString&)> expandFunc = nullptr);

    /**
     * @brief Create and return a QFont based on engine.font.default settings
     * @return Configured QFont object
     */
    QFont createDefaultFont();

    /**
     * @brief Apply the default font to a QGuiApplication or QApplication
     * @param font The font to set as default
     */
    static void setApplicationDefaultFont(const QFont& font);

    /**
     * @brief Get list of loaded font paths
     * @return Vector of FontPath structures
     */
    const std::vector<FontPath>& getLoadedFonts() const { return m_loadedFonts; }

    /**
     * @brief Check if fonts have been loaded
     * @return True if at least one font was loaded successfully
     */
    bool hasFontsLoaded() const { return !m_loadedFonts.empty(); }

  signals:
    /**
     * @brief Emitted when a font is successfully loaded
     * @param fontPath Path to the loaded font
     * @param fontName Name of the loaded font
     */
    void fontLoaded(const QString& fontPath, const QString& fontName);

    /**
     * @brief Emitted when a font fails to load
     * @param fontPath Path to the font that failed to load
     * @param error Error message
     */
    void fontLoadError(const QString& fontPath, const QString& error);

  private:
    /**
     * @brief Parse font style string to QFont::Style enum
     */
    QFont::Style parseStyle(const QString& style) const;

    /**
     * @brief Parse capitalization string to QFont::Capitalization enum
     */
    QFont::Capitalization parseCapitalization(const QString& cap) const;

    /**
     * @brief Parse style hint string to QFont::StyleHint enum
     */
    QFont::StyleHint parseStyleHint(const QString& hint) const;

    /**
     * @brief Parse style strategy string to QFont::StyleStrategy enum
     */
    QFont::StyleStrategy parseStyleStrategy(const QString& strategy) const;

    /**
     * @brief Parse hinting preference string to QFont::HintingPreference enum
     */
    QFont::HintingPreference parseHintingPreference(const QString& pref) const;

    /**
     * @brief Parse letter spacing type string to QFont::SpacingType enum
     */
    QFont::SpacingType parseSpacingType(const QString& type) const;

    /**
     * @brief Convert QFont weight value (100-900) to QFont::Weight enum
     */
    QFont::Weight weightIntToEnum(int weight) const;

  private:
    DsSettingsRef m_settings;
    std::vector<FontPath> m_loadedFonts;
};

} // namespace dsqt

#endif // DSFONTMANAGER_H
