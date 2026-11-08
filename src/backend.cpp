#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

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

bool Backend::removeTheme(const QString &id) {
    if (!canRemove(id))
        return fail(tr("\"%1\" can't be removed: it's a system theme or in use").arg(id));
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
