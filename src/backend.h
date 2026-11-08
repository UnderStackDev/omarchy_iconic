#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>

#include "desktopsettings.h"
#include "iconthemes.h"
#include "installer.h"
#include "desktopentries.h"

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
    // Apps tab.
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY appsChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY appsChanged)

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
    QVariantList apps() const;
    QList<DesktopEntries::Entry> appList() const { return m_apps; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

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

    // Re-reads every launcher.
    Q_INVOKABLE void refreshApps();
    // Saves a launcher's name, icon (a theme name or absolute path) and
    // whether it's hidden. Unchanged values are left alone.
    Q_INVOKABLE bool saveApp(const QString &id, const QString &name, const QString &icon, bool hidden);
    // Puts a launcher back the way its package or the user made it.
    Q_INVOKABLE bool resetApp(const QString &id);
    // Copies a picked image into Iconic's icon folder; returns its path.
    Q_INVOKABLE QString importIcon(const QString &pathOrUrl, const QString &id);
    // Icon names in the current theme for the picker, filtered, at most `limit`.
    Q_INVOKABLE QStringList themeIconNames(const QString &query, int limit = 400);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Window mode rebuilds the launcher icon layer off the GUI thread.
    void setAsyncShellIcons(bool async) { m_asyncShellIcons = async; }

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
    void appsChanged();
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

    // Undo keeps the bytes of every file a change touched (or that it was
    // absent), so undo and redo are exact.
    struct Snapshot {
        QList<QPair<QString, QByteArray>> files;  // path, content
        QStringList absent;
    };
    Snapshot snapshot(const QString &id) const;
    void restore(const Snapshot &snap);
    const DesktopEntries::Entry *findApp(const QString &id) const;
    void afterAppChange();

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

    QList<DesktopEntries::Entry> m_apps;
    QList<QPair<QString, Snapshot>> m_undo, m_redo;  // app id, state before
    bool m_asyncShellIcons = false;
    bool m_shellIconsRunning = false;
    bool m_shellIconsAgain = false;
};
