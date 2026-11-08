#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QFutureWatcher>
#include <QSaveFile>
#include <QSet>
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
    // A rebuild running on another thread reads the cache; leave it be.
    if (!m_shellIconsRunning)
        m_iconThemes->clearCache();
    m_themes = m_iconThemes->scan();
    m_config = IconicConfig::load();
    QString e;
    m_currentTheme = m_desktop.currentTheme(&e);
    if (!e.isEmpty())
        fail(e);
    emit changed();
    refreshApps();
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
    const QStringList names = ShellIcons::desktopIconNames(ShellIcons::defaultApplicationDirs());
    if (!m_asyncShellIcons) {
        QString e;
        if (!ShellIcons::rebuild(*m_iconThemes, m_currentTheme, names, m_shellLayer, &e))
            return fail(e);
        return true;
    }
    // One rebuild at a time; a change during one queues exactly one more.
    if (m_shellIconsRunning) {
        m_shellIconsAgain = true;
        return true;
    }
    m_shellIconsRunning = true;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const QString e = watcher->result();
        watcher->deleteLater();
        m_shellIconsRunning = false;
        if (!e.isEmpty())
            fail(e);
        if (m_shellIconsAgain) {
            m_shellIconsAgain = false;
            updateShellIcons();
        }
    });
    IconThemes *themes = m_iconThemes.get();
    const QString theme = m_currentTheme, layer = m_shellLayer;
    watcher->setFuture(QtConcurrent::run([themes, theme, names, layer] {
        QString e;
        ShellIcons::rebuild(*themes, theme, names, layer, &e);
        return e;
    }));
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

static QString sourceName(DesktopEntries::Source source) {
    switch (source) {
    case DesktopEntries::Source::User: return "user";
    case DesktopEntries::Source::Flatpak: return "flatpak";
    case DesktopEntries::Source::WebApp: return "webapp";
    default: return "system";
    }
}

static QSet<QString> omarchyHides() {
    const QString root = qEnvironmentVariable("OMARCHY_PATH", "/usr/share/omarchy");
    QFile file(root + "/default/omarchy/launcher.hides");
    QSet<QString> ids;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        for (const QString &line : QString::fromUtf8(file.readAll()).split('\n')) {
            QString id = line.trimmed();
            if (id.endsWith(".desktop"))
                id.chop(8);
            if (!id.isEmpty() && !id.startsWith('#'))
                ids.insert(id);
        }
    return ids;
}

void Backend::refreshApps() {
    m_apps = DesktopEntries::scan(DesktopEntries::applicationDirs(),
                                  qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts),
                                  omarchyHides());
    emit appsChanged();
}

QVariantList Backend::apps() const {
    QVariantList out;
    for (const DesktopEntries::Entry &e : m_apps) {
        QVariantMap row;
        row["id"] = e.id;
        row["name"] = e.name;
        row["comment"] = e.comment;
        row["icon"] = e.icon;
        row["path"] = e.path;
        row["hidden"] = e.noDisplay;
        row["hiddenByDesktop"] = e.hiddenByDesktop;
        row["modified"] = e.modified();
        row["source"] = sourceName(e.source);
        row["originalName"] = e.originalName;
        row["originalIcon"] = e.originalIcon;
        row["originalHidden"] = e.originalNoDisplay;
        out << row;
    }
    return out;
}

const DesktopEntries::Entry *Backend::findApp(const QString &id) const {
    for (const DesktopEntries::Entry &e : m_apps)
        if (e.id == id)
            return &e;
    return nullptr;
}

Backend::Snapshot Backend::snapshot(const QString &id) const {
    Snapshot snap;
    QStringList paths{DesktopEntries::userDir() + "/" + id + ".desktop",
                      DesktopEntries::backupDir() + "/" + id + ".desktop"};
    if (const auto *e = findApp(id))
        paths << e->path;
    paths.removeDuplicates();
    for (const QString &path : paths) {
        // Only Iconic's own folders and the user's launchers are ever restored.
        if (!path.startsWith(DesktopEntries::userDir() + "/") && !path.startsWith(DesktopEntries::backupDir() + "/"))
            continue;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            snap.files << qMakePair(path, f.readAll());
        else
            snap.absent << path;
    }
    return snap;
}

void Backend::restore(const Snapshot &snap) {
    for (const auto &file : snap.files) {
        QDir().mkpath(QFileInfo(file.first).absolutePath());
        QSaveFile out(file.first);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(file.second);
            out.commit();
        }
    }
    for (const QString &path : snap.absent)
        QFile::remove(path);
}

void Backend::afterAppChange() {
    DesktopEntries::refreshDatabase();
    refreshApps();
    updateShellIcons();
}

bool Backend::saveApp(const QString &id, const QString &name, const QString &icon, bool hidden) {
    const auto *entry = findApp(id);
    if (!entry)
        return fail(tr("No launcher called %1").arg(id));
    if (name.trimmed().isEmpty())
        return fail(tr("The name can't be empty"));
    DesktopEntries::Change change;
    if (name.trimmed() != entry->name)
        change.name = name.trimmed();
    if (!icon.trimmed().isEmpty() && icon.trimmed() != entry->icon)
        change.icon = icon.trimmed();
    if (hidden != entry->noDisplay)
        change.noDisplay = hidden ? 1 : 0;
    if (change.name.isEmpty() && change.icon.isEmpty() && change.noDisplay < 0)
        return true;

    const Snapshot before = snapshot(id);
    QString e;
    if (DesktopEntries::apply(*entry, change, &e).isEmpty()) {
        restore(before);
        return fail(e);
    }
    m_undo << qMakePair(id, before);
    m_redo.clear();
    afterAppChange();
    return true;
}

bool Backend::resetApp(const QString &id) {
    const auto *entry = findApp(id);
    if (!entry || !entry->modified())
        return true;
    const Snapshot before = snapshot(id);
    QString e;
    if (!DesktopEntries::reset(*entry, &e))
        return fail(e);
    m_undo << qMakePair(id, before);
    m_redo.clear();
    afterAppChange();
    return true;
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const auto step = m_undo.takeLast();
    m_redo << qMakePair(step.first, snapshot(step.first));
    restore(step.second);
    afterAppChange();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    const auto step = m_redo.takeLast();
    m_undo << qMakePair(step.first, snapshot(step.first));
    restore(step.second);
    afterAppChange();
}

QString Backend::importIcon(const QString &pathOrUrl, const QString &id) {
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    QString e;
    const QString imported = DesktopEntries::importIcon(path, id, &e);
    if (imported.isEmpty())
        fail(e);
    return imported;
}

QStringList Backend::themeIconNames(const QString &query, int limit) {
    const QStringList all = m_iconThemes->appIconNames(m_currentTheme);
    const QString q = query.trimmed().toLower();
    QStringList out;
    for (const QString &name : all) {
        if (q.isEmpty() || name.toLower().contains(q))
            out << name;
        if (out.size() >= limit)
            break;
    }
    return out;
}
