#pragma once

#include <QString>

// Sets one key in an INI-style file and leaves every other line as it was.
// The file is replaced atomically, keeps its permissions, and a symlinked
// file is written through to its target.
namespace IniFile {
bool setValue(const QString &path, const QString &section, const QString &key, const QString &value,
              QString *error = nullptr);
QString value(const QString &path, const QString &section, const QString &key);
}

// What Iconic remembers: whether the icon theme follows the Omarchy theme or
// stays on the user's pick. Stored in ~/.config/iconic/config.toml.
struct IconicConfig {
    bool lock = false;
    QString theme;

    static QString path();
    static IconicConfig load(const QString &file = path());
    bool save(const QString &file = path(), QString *error = nullptr) const;
};

// Writes the icon theme everywhere the desktop reads it. gsettings is what
// GTK apps and the Flatpak settings portal use; the INI files are what GTK
// (when dconf isn't reachable), KDE/Qt apps, and qt5ct/qt6ct read.
class DesktopSettings {
public:
    explicit DesktopSettings(const QString &gsettings = "gsettings");

    QString currentTheme(QString *error = nullptr) const;
    bool apply(const QString &theme, QString *error = nullptr) const;

    // Copies `theme` into the INI files only, without touching gsettings.
    // After an Omarchy theme switch in Follow mode, gsettings already has
    // Omarchy's pick and only these files are stale.
    bool syncFiles(const QString &theme, QString *error = nullptr) const;

    static QString parseGsettingsString(const QString &output);

private:
    QString m_gsettings;
};

// The Omarchy theme-set hook that keeps the icon theme after a theme switch.
// Omarchy runs theme-set hooks after it has set its own icon theme, so the
// hook gets the last word.
namespace ThemeSetHook {
QString path();
QString script(const QString &binary);
// Writes the hook for this binary, only when it differs. Returns false and
// sets `error` when it can't be written.
bool ensure(const QString &binary, QString *error = nullptr);
}
