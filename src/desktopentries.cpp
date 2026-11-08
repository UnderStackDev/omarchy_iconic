#include "desktopentries.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

#include "desktopsettings.h"

using namespace DesktopEntries;

static QString tr(const char *text) {
    return QCoreApplication::translate("DesktopEntries", text);
}

QStringList DesktopEntries::applicationDirs() {
    QStringList dirs;
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << dir + "/applications";
    dirs.removeDuplicates();
    return dirs;
}

QString DesktopEntries::userDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/applications";
}

QString DesktopEntries::backupDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/iconic/backups";
}

QString DesktopEntries::customIconDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/iconic/icons";
}

namespace {
struct Parsed {
    QHash<QString, QString> keys;  // [Desktop Entry] only
    bool valid = false;
};

Parsed parse(const QString &file) {
    Parsed parsed;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return parsed;
    bool main = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith('[')) {
            main = line == "[Desktop Entry]";
            parsed.valid = parsed.valid || main;
            continue;
        }
        const int eq = line.indexOf('=');
        if (main && eq > 0 && !line.startsWith('#'))
            parsed.keys.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return parsed;
}

bool listMatches(const QString &list, const QStringList &desktops) {
    for (const QString &item : list.split(';', Qt::SkipEmptyParts))
        if (desktops.contains(item.trimmed()))
            return true;
    return false;
}

bool isTrue(const QString &value) {
    return value.compare("true", Qt::CaseInsensitive) == 0;
}

bool inside(const QString &dir, const QString &path) {
    return QDir::cleanPath(path).startsWith(QDir::cleanPath(dir) + "/");
}

Source sourceOf(const QString &path, const Parsed &p) {
    if (path.contains("/flatpak/exports/") || p.keys.contains("X-Flatpak"))
        return Source::Flatpak;
    if (inside(DesktopEntries::userDir(), path)) {
        const QString exec = p.keys.value("Exec");
        if (exec.contains("omarchy-launch-webapp") || exec.contains("--app="))
            return Source::WebApp;
        return Source::User;
    }
    return Source::System;
}

bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        if (error)
            *error = tr("Could not write %1").arg(path);
        return false;
    }
    return true;
}

QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Drops Name[xx]= lines from [Desktop Entry], so a new name shows in every
// language rather than only where no translation exists.
bool removeLocalizedNames(const QString &path, QString *error) {
    QStringList lines = QString::fromUtf8(readFile(path)).split('\n');
    bool main = false, changed = false;
    for (int i = 0; i < lines.size();) {
        const QString t = lines[i].trimmed();
        if (t.startsWith('[')) {
            main = t == "[Desktop Entry]";
        } else if (main && t.startsWith("Name[")) {
            lines.removeAt(i);
            changed = true;
            continue;
        }
        ++i;
    }
    return !changed || writeFile(path, lines.join('\n').toUtf8(), error);
}
}  // namespace

QString DesktopEntries::value(const QString &file, const QString &key) {
    return parse(file).keys.value(key);
}

QList<Entry> DesktopEntries::scan(const QStringList &dirs, const QStringList &desktops,
                                  const QSet<QString> &omarchyHides) {
    // Every file per id, in lookup order; the first one wins.
    QHash<QString, QStringList> files;
    QStringList order;
    for (const QString &dir : dirs) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        QStringList found;
        while (it.hasNext())
            found << it.next();
        found.sort();
        for (const QString &file : found) {
            QString id = QDir(dir).relativeFilePath(file);
            id.chop(8);
            id.replace('/', '-');
            if (!files.contains(id))
                order << id;
            files[id] << file;
        }
    }

    const QString backups = backupDir();
    QList<Entry> out;
    for (const QString &id : order) {
        const QStringList &candidates = files[id];
        const Parsed p = parse(candidates.first());
        if (!p.valid || p.keys.value("Type", "Application") != "Application")
            continue;
        Entry e;
        e.id = id;
        e.path = candidates.first();
        e.name = p.keys.value("Name", id);
        e.comment = p.keys.value("Comment");
        e.icon = p.keys.value("Icon");
        e.exec = p.keys.value("Exec");
        e.noDisplay = isTrue(p.keys.value("NoDisplay")) || isTrue(p.keys.value("Hidden"));
        const QString only = p.keys.value("OnlyShowIn"), notIn = p.keys.value("NotShowIn");
        e.hiddenByDesktop = (!only.isEmpty() && !listMatches(only, desktops)) ||
                            (!notIn.isEmpty() && listMatches(notIn, desktops)) || omarchyHides.contains(id);
        e.override = isTrue(p.keys.value("X-Iconic-Override"));
        e.source = sourceOf(e.path, p);

        if (e.override && candidates.size() > 1) {
            e.originalPath = candidates.at(1);
            e.source = sourceOf(e.originalPath, parse(e.originalPath));
        } else if (!e.override && QFile::exists(backups + "/" + id + ".desktop")) {
            e.backedUp = true;
            e.originalPath = backups + "/" + id + ".desktop";
        }
        if (!e.originalPath.isEmpty()) {
            const Parsed o = parse(e.originalPath);
            e.originalName = o.keys.value("Name", id);
            e.originalIcon = o.keys.value("Icon");
            e.originalNoDisplay = isTrue(o.keys.value("NoDisplay")) || isTrue(o.keys.value("Hidden"));
        } else {
            e.originalName = e.name;
            e.originalIcon = e.icon;
            e.originalNoDisplay = e.noDisplay;
        }
        out << e;
    }
    std::sort(out.begin(), out.end(), [](const Entry &a, const Entry &b) {
        return QString::localeAwareCompare(a.name.toLower(), b.name.toLower()) < 0;
    });
    return out;
}

QString DesktopEntries::apply(const Entry &entry, const Change &change, QString *error) {
    QString target;
    if (entry.override) {
        target = entry.path;
    } else if (inside(userDir(), entry.path)) {
        // The user's own launcher: keep the original once, then edit it.
        target = entry.path;
        const QString backup = backupDir() + "/" + entry.id + ".desktop";
        if (!QFile::exists(backup) && !writeFile(backup, readFile(entry.path), error))
            return {};
    } else {
        target = userDir() + "/" + entry.id + ".desktop";
        QByteArray content = readFile(entry.path);
        if (content.isEmpty()) {
            if (error)
                *error = tr("Could not read %1").arg(entry.path);
            return {};
        }
        if (!writeFile(target, content, error))
            return {};
        if (!IniFile::setValue(target, "Desktop Entry", "X-Iconic-Override", "true", error))
            return {};
    }

    auto set = [&](const QString &key, const QString &v) {
        return IniFile::setValue(target, "Desktop Entry", key, v, error);
    };
    if (!change.name.isEmpty()) {
        QString name = change.name;
        name.replace('\n', ' ');
        if (!set("Name", name.trimmed()) || !removeLocalizedNames(target, error))
            return {};
    }
    if (!change.icon.isEmpty() && !set("Icon", change.icon.trimmed()))
        return {};
    if (change.noDisplay >= 0) {
        if (!set("NoDisplay", change.noDisplay ? "true" : "false"))
            return {};
        // Hidden=true means "deleted"; showing the app again has to clear it.
        if (!change.noDisplay && isTrue(value(target, "Hidden")) && !set("Hidden", "false"))
            return {};
    }
    return target;
}

bool DesktopEntries::reset(const Entry &entry, QString *error) {
    if (entry.override) {
        if (!QFile::remove(entry.path)) {
            if (error)
                *error = tr("Could not remove %1").arg(entry.path);
            return false;
        }
        return true;
    }
    if (entry.backedUp) {
        if (!writeFile(entry.path, readFile(entry.originalPath), error))
            return false;
        QFile::remove(entry.originalPath);
        return true;
    }
    return true;
}

QString DesktopEntries::importIcon(const QString &file, const QString &id, QString *error) {
    const QString ext = QFileInfo(file).suffix().toLower();
    if (ext != "png" && ext != "svg" && ext != "xpm") {
        if (error)
            *error = tr("Icons must be PNG, SVG or XPM files");
        return {};
    }
    const QByteArray data = readFile(file);
    QImageReader reader(file);
    if (data.isEmpty() || !reader.canRead()) {
        if (error)
            *error = tr("%1 is not a readable image").arg(QFileInfo(file).fileName());
        return {};
    }
    // Content-addressed, so the same picture picked twice is stored once.
    const QString hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().left(10);
    QString safeId = id;
    safeId.replace(QRegularExpression("[^A-Za-z0-9._-]"), "_");
    const QString target = customIconDir() + "/" + safeId + "-" + hash + "." + ext;
    if (!QFile::exists(target) && !writeFile(target, data, error))
        return {};
    return target;
}

void DesktopEntries::refreshDatabase() {
    QProcess::startDetached("update-desktop-database", {"-q", userDir()});
}
