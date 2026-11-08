#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFutureWatcher>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include "shellicons.h"

Backend::Backend(QObject *parent, const QString &gsettings, const QString &hookBinary, const QString &shellLayer)
    : QObject(parent),
      m_desktop(gsettings),
      m_hookBinary(hookBinary.isEmpty() ? QCoreApplication::applicationFilePath() : hookBinary),
      m_shellLayer(shellLayer.isEmpty() ? ShellIcons::layerPath() : shellLayer),
      m_iconThemes(std::make_unique<IconThemes>(iconSearchPaths(), QDir::homePath())) {
    refresh();
}

QStringList Backend::iconSearchPaths() {
    QStringList paths{QDir::homePath() + "/.icons"};
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        paths << dir + "/icons";
    paths << "/usr/share/pixmaps";
    paths.removeDuplicates();
    return paths;
}

QVariantList Backend::themes() const {
    QVariantList out;
    for (const IconThemeInfo &t : m_themes) {
        QVariantMap row;
        row["id"] = t.id;
        row["name"] = t.name;
        row["comment"] = t.comment;
        row["user"] = t.user;
        row["path"] = t.path;
        out << row;
    }
    return out;
}

void Backend::refresh() {
    m_iconThemes->clearCache();
    m_themes = m_iconThemes->scan();
    m_config = IconicConfig::load();
    QString e;
    m_currentTheme = m_desktop.currentTheme(&e);
    if (!e.isEmpty())
        fail(e);
    emit changed();
}

bool Backend::installed(const QString &id) const {
    for (const IconThemeInfo &t : m_themes)
        if (t.id == id)
            return true;
    return false;
}

bool Backend::fail(const QString &message) {
    m_error = message;
    emit errorChanged();
    return false;
}

void Backend::clearError() {
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit errorChanged();
}

bool Backend::ensureHook() {
    QString e;
    if (!ThemeSetHook::ensure(m_hookBinary, &e))
        return fail(e);
    return true;
}

bool Backend::applyTheme(const QString &id) {
    if (!installed(id))
        return fail(tr("Icon theme \"%1\" is not installed").arg(id));
    QString e;
    if (!m_desktop.apply(id, &e))
        return fail(e);
    m_currentTheme = id;
    if (m_config.lock) {
        m_config.theme = id;
        if (!m_config.save(IconicConfig::path(), &e))
            return fail(e);
    }
    ensureHook();
    updateShellIcons();
    emit changed();
    emit themeApplied(id);
    return true;
}

bool Backend::setLock(bool lock) {
    m_config.lock = lock;
    if (lock)
        m_config.theme = m_currentTheme;
    QString e;
    if (!m_config.save(IconicConfig::path(), &e))
        return fail(e);
    ensureHook();
    emit changed();
    return true;
}

bool Backend::canRemove(const QString &id) const {
    if (id == m_currentTheme || (m_config.lock && id == m_config.theme))
        return false;
    for (const IconThemeInfo &t : m_themes)
        if (t.id == id)
            return t.user;
    return false;
}

QStringList Backend::dependents(const QString &id) const {
    QString path;
    for (const IconThemeInfo &t : m_themes)
        if (t.id == id)
            path = QFileInfo(t.path).canonicalFilePath();
    if (path.isEmpty())
        return {};
    QStringList out;
    for (const IconThemeInfo &t : m_themes) {
        if (!t.user || t.id == id)
            continue;
        // Variants link whole size/context folders (16x16/apps), two levels
        // down; looking there is enough and stays fast.
        bool uses = false;
        const QDir root(t.path);
        for (const QString &size : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            for (const QFileInfo &entry : QDir(root.filePath(size)).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System)) {
                if (!entry.isSymLink())
                    continue;
                // Resolved by hand, so a link counts even if its target is gone.
                const QString raw = entry.readSymLink();
                const QString target = QDir::cleanPath(QDir::isAbsolutePath(raw) ? raw : entry.absolutePath() + "/" + raw);
                if (target.startsWith(path + "/")) {
                    uses = true;
                    break;
                }
            }
            if (uses)
                break;
        }
        if (uses)
            out << t.id;
    }
    return out;
}

bool Backend::removeTheme(const QString &id) {
    if (!canRemove(id))
        return fail(tr("\"%1\" can't be removed: it's a system theme or in use").arg(id));
    const QStringList users = dependents(id);
    if (!users.isEmpty())
        return fail(tr("%1 uses files from %2; remove it first").arg(users.join(", "), id));
    QString path;
    for (const IconThemeInfo &t : m_themes)
        if (t.id == id)
            path = t.path;
    const QFileInfo info(path);
    // A symlinked theme loses only its link, never the folder it points to.
    const bool removed = info.isSymLink() ? QFile::remove(path) : QDir(path).removeRecursively();
    refresh();
    if (!removed)
        return fail(tr("Could not remove %1").arg(path));
    return true;
}

bool Backend::runThemeSetHook() {
    QString e;
    if (m_config.lock && !m_config.theme.isEmpty()) {
        if (installed(m_config.theme)) {
            if (!m_desktop.apply(m_config.theme, &e))
                return fail(e);
            m_currentTheme = m_config.theme;
            return updateShellIcons();
        }
        // The locked theme was uninstalled; let Omarchy's pick stand.
        fail(tr("Locked icon theme \"%1\" is not installed; following Omarchy").arg(m_config.theme));
    }
    if (m_currentTheme.isEmpty())
        return false;
    if (!m_desktop.syncFiles(m_currentTheme, &e))
        return fail(e);
    if (!updateShellIcons())
        return false;
    return m_error.isEmpty();
}

bool Backend::updateShellIcons(bool onlyIfStale) {
    if (m_currentTheme.isEmpty())
        return false;
    if (onlyIfStale && ShellIcons::builtFor(m_shellLayer) == m_currentTheme)
        return true;
    QString e;
    const QStringList names = ShellIcons::desktopIconNames(ShellIcons::defaultApplicationDirs());
    if (!ShellIcons::rebuild(*m_iconThemes, m_currentTheme, names, m_shellLayer, &e))
        return fail(e);
    return true;
}

QStringList Backend::conflicts() const {
    QStringList out;
    for (const Installer::Prepared &p : m_pending)
        for (const Installer::Found &f : p.themes)
            if (f.exists)
                out << f.id;
    return out;
}

void Backend::setBusy(bool busy, const QString &text) {
    m_busy = busy;
    m_busyText = text;
    emit installChanged();
}

void Backend::clearInstallResult() {
    m_lastInstalled.clear();
    m_lastNotes.clear();
    emit installChanged();
}

void Backend::installPaths(const QVariantList &paths) {
    if (m_busy)
        return;
    QStringList files;
    for (const QVariant &v : paths) {
        const QString text = v.toString();
        const QUrl url(text);
        if (url.isLocalFile())
            files << url.toLocalFile();
        else if (url.scheme().isEmpty() || text.startsWith('/'))
            files << text;
        else {
            fail(tr("Download %1 first, then drop the file here").arg(text));
            return;
        }
    }
    if (files.isEmpty())
        return;

    m_lastInstalled.clear();
    m_lastNotes.clear();
    setBusy(true, tr("Unpacking %n item(s)…", "", files.size()));

    // Unpacking can take a while for big archives; keep the window responsive.
    auto *watcher = new QFutureWatcher<QList<Installer::Prepared>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const QList<Installer::Prepared> prepared = watcher->result();
        watcher->deleteLater();
        m_pending.clear();
        QStringList errors;
        for (const Installer::Prepared &p : prepared) {
            m_lastNotes << p.skipped;
            if (p.ok())
                m_pending << p;
            else
                errors << p.error;
        }
        if (!errors.isEmpty())
            fail(errors.join("\n"));
        if (m_pending.isEmpty()) {
            setBusy(false);
            return;
        }
        if (!conflicts().isEmpty()) {
            // Wait for the user's choice; resolveConflicts() continues.
            setBusy(false);
            return;
        }
        runInstall(Installer::Conflict::Skip);
    });
    const QString target = Installer::targetRoot();
    watcher->setFuture(QtConcurrent::run([files, target] {
        QList<Installer::Prepared> out;
        for (const QString &f : files)
            out << Installer::prepare(f, target);
        return out;
    }));
}

void Backend::resolveConflicts(const QString &choice) {
    if (m_pending.isEmpty())
        return;
    if (choice == "cancel") {
        m_pending.clear();
        emit installChanged();
        return;
    }
    runInstall(choice == "replace"    ? Installer::Conflict::Replace
               : choice == "keepBoth" ? Installer::Conflict::KeepBoth
                                      : Installer::Conflict::Skip);
}

void Backend::runInstall(Installer::Conflict conflict) {
    QList<Installer::Prepared> pending = m_pending;
    m_pending.clear();
    setBusy(true, tr("Installing…"));
    const QString target = Installer::targetRoot();
    auto *watcher = new QFutureWatcher<QPair<QList<Installer::Prepared>, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        finishInstall(result.first, result.second);
    });
    watcher->setFuture(QtConcurrent::run([pending, conflict, target]() mutable {
        QStringList errors;
        for (Installer::Prepared &p : pending) {
            QString e;
            if (!Installer::install(p, conflict, target, &e) && !e.isEmpty())
                errors << e;
        }
        return qMakePair(pending, errors.join("\n"));
    }));
}

void Backend::finishInstall(const QList<Installer::Prepared> &done, const QString &error) {
    for (const Installer::Prepared &p : done) {
        for (const Installer::Found &f : p.themes)
            if (!f.installedAs.isEmpty())
                m_lastInstalled << f.installedAs;
        for (const QString &note : p.skipped)
            if (!m_lastNotes.contains(note))
                m_lastNotes << note;
    }
    refresh();
    if (m_lastInstalled.isEmpty() && !error.isEmpty())
        fail(error);
    setBusy(false);
}

QStringList Backend::installNow(const QString &path, Installer::Conflict conflict, QStringList *notes) {
    Installer::Prepared prepared = Installer::prepare(path);
    if (!prepared.ok()) {
        fail(prepared.error);
        return {};
    }
    QString e;
    const bool ok = Installer::install(prepared, conflict, Installer::targetRoot(), &e);
    if (notes)
        *notes = prepared.skipped;
    QStringList installed;
    for (const Installer::Found &f : prepared.themes)
        if (!f.installedAs.isEmpty())
            installed << f.installedAs;
    refresh();
    if (!ok)
        fail(e);
    return installed;
}
