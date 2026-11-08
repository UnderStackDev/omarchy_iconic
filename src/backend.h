#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>

#include "desktopsettings.h"
#include "iconthemes.h"
#include "installer.h"

// Iconic's state and actions, shared by the window (as `backend` in QML) and
// the command line. Nothing here needs a display.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentTheme READ currentTheme NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)
    Q_PROPERTY(bool lock READ lock NOTIFY changed)
    Q_PROPERTY(QString lockedTheme READ lockedTheme NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // Install state for the Install tab.
    Q_PROPERTY(bool busy READ busy NOTIFY installChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY installChanged)
    Q_PROPERTY(QStringList conflicts READ conflicts NOTIFY installChanged)
    Q_PROPERTY(QStringList lastInstalled READ lastInstalled NOTIFY installChanged)
    Q_PROPERTY(QStringList lastNotes READ lastNotes NOTIFY installChanged)

public:
    // `gsettings` and `hookBinary` are swapped out by tests; the defaults are
    // the real command and this executable.
    explicit Backend(QObject *parent = nullptr, const QString &gsettings = "gsettings",
                     const QString &hookBinary = {}, const QString &shellLayer = {});

    QString currentTheme() const { return m_currentTheme; }
    QVariantList themes() const;
    QList<IconThemeInfo> themeList() const { return m_themes; }
    bool lock() const { return m_config.lock; }
    QString lockedTheme() const { return m_config.theme; }
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }
    QString busyText() const { return m_busyText; }
    QStringList conflicts() const;
    QStringList lastInstalled() const { return m_lastInstalled; }
    QStringList lastNotes() const { return m_lastNotes; }

    IconThemes *iconThemes() { return m_iconThemes.get(); }

    // Re-reads installed themes, gsettings and the config.
    Q_INVOKABLE void refresh();
    // Makes `id` the desktop's icon theme (and the locked one in Lock mode).
    Q_INVOKABLE bool applyTheme(const QString &id);
    // Lock keeps the current icon theme across Omarchy theme switches.
    Q_INVOKABLE bool setLock(bool lock);
    Q_INVOKABLE bool canRemove(const QString &id) const;
    Q_INVOKABLE bool removeTheme(const QString &id);
    // Other user themes that link into `id` (Papirus-Dark into Papirus) and
    // would break without it.
    Q_INVOKABLE QStringList dependents(const QString &id) const;
    Q_INVOKABLE void clearError();
    // Shows a message from the window side (e.g. the file picker failed).
    Q_INVOKABLE void reportError(const QString &message) { fail(message); }

    // Unpacks and installs dropped or picked files/folders (paths or file://
    // URLs) in the background. Themes that already exist wait in
    // `conflicts` for resolveConflicts("replace" | "keepBoth" | "skip").
    Q_INVOKABLE void installPaths(const QVariantList &paths);
    Q_INVOKABLE void resolveConflicts(const QString &choice);
    Q_INVOKABLE void clearInstallResult();

    // The same, blocking, for the command line. Returns the installed ids.
    QStringList installNow(const QString &path, Installer::Conflict conflict, QStringList *notes = nullptr);

    // What the Omarchy theme-set hook runs. Returns false on failure.
    bool runThemeSetHook();

    // Rebuilds the launcher/dock icon layer for the current theme (see
    // shellicons.h). With `onlyIfStale`, skips it when it already matches.
    bool updateShellIcons(bool onlyIfStale = false);

    // Where icon themes live, in freedesktop lookup order: the user's own
    // folders first, then every XDG data dir. Flatpak only sees the
    // ~/.local/share/icons and /usr/share/icons ones.
    static QStringList iconSearchPaths();

    // Kept for callers that only need the gsettings parsing.
    static QString parseGsettingsString(const QString &output) {
        return DesktopSettings::parseGsettingsString(output);
    }

signals:
    void changed();
    void installChanged();
    void errorChanged();
    // The desktop's icon theme changed; the window re-themes its own icons.
    void themeApplied(const QString &id);

private:
    bool fail(const QString &message);
    bool installed(const QString &id) const;
    bool ensureHook();
    void setBusy(bool busy, const QString &text = {});
    void runInstall(Installer::Conflict conflict);
    void finishInstall(const QList<Installer::Prepared> &done, const QString &error);

    DesktopSettings m_desktop;
    QString m_hookBinary;
    QString m_shellLayer;
    std::unique_ptr<IconThemes> m_iconThemes;
    QList<IconThemeInfo> m_themes;
    IconicConfig m_config;
    QString m_currentTheme;
    QString m_error;

    bool m_busy = false;
    QString m_busyText;
    QList<Installer::Prepared> m_pending;
    QStringList m_lastInstalled, m_lastNotes;
};
