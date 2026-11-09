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
    // The icon theme picked in Iconic, in both modes.
    QString theme;
    // The Omarchy theme that was active when `theme` was picked. In Follow
    // mode Iconic's pick stands until the Omarchy theme actually changes;
    // Omarchy updates re-run theme-set with the same theme, and that alone
    // must not throw the pick away.
    QString omarchyTheme;

    // The Omarchy theme in use now (~/.local/state/omarchy/current/theme.name).
    static QString currentOmarchyTheme();

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
    QString program() const;

private:
    QString m_gsettings;
};

// Omarchy hooks that keep Iconic's choices: theme-set runs after an Omarchy
// theme switch (after Omarchy has set its own icon theme, so the hook gets
// the last word); post-update runs after `omarchy update`, whose migrations
// can refresh the theme and re-copy Omarchy's own launchers.
namespace ThemeSetHook {
QStringList events();
QString path(const QString &event = "theme-set");
QString script(const QString &binary, const QString &event = "theme-set");
// Where hooks were before the rename to omarchy_iconic (0.1.x).
QString legacyPath(const QString &event);
bool hasLegacy();
// Writes every hook for this binary, only where it differs, and removes
// the 0.1.x ones. Returns false
// and sets `error` when one can't be written.
bool ensure(const QString &binary, QString *error = nullptr);
}
