#include "iconthemes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QSet>
#include <algorithm>
#include <climits>

IconThemes::IconThemes(const QStringList &searchPaths, const QString &home)
    : m_searchPaths(searchPaths), m_home(home) {}

IconThemes::Parsed IconThemes::parseIndex(const QString &file) {
    Parsed parsed;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return parsed;

    // A minimal INI reader: QSettings would split values on commas and
    // mangle the sub-directory section names.
    QHash<QString, QHash<QString, QString>> sections;
    QString section;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq > 0)
            sections[section].insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }

    const auto head = sections.value("Icon Theme");
    if (head.isEmpty())
        return parsed;
    auto list = [](const QString &value) {
        QStringList out;
        for (const QString &part : value.split(',', Qt::SkipEmptyParts))
            if (!part.trimmed().isEmpty())
                out << part.trimmed();
        return out;
    };
    parsed.valid = true;
    parsed.name = head.value("Name");
    parsed.comment = head.value("Comment");
    parsed.inherits = list(head.value("Inherits"));
    parsed.hidden = head.value("Hidden").compare("true", Qt::CaseInsensitive) == 0;

    const QStringList dirs = list(head.value("Directories")) + list(head.value("ScaledDirectories"));
    QSet<QString> seen;
    for (const QString &dir : dirs) {
        if (seen.contains(dir))
            continue;
        seen.insert(dir);
        const auto keys = sections.value(dir);
        Directory d;
        d.path = dir;
        d.size = keys.value("Size").toInt();
        d.scale = qMax(1, keys.value("Scale", "1").toInt());
        d.type = keys.value("Type", "Threshold");
        d.minSize = keys.value("MinSize", QString::number(d.size)).toInt();
        d.maxSize = keys.value("MaxSize", QString::number(d.size)).toInt();
        d.threshold = keys.value("Threshold", "2").toInt();
        d.context = keys.value("Context");
        if (d.size > 0)
            parsed.directories << d;
    }
    return parsed;
}

QList<IconThemeInfo> IconThemes::scan() const {
    static const QSet<QString> skipped{"hicolor", "default", "locolor"};
    QList<IconThemeInfo> out;
    QSet<QString> seen;
    const QString userData = m_home + "/.local/share/icons";
    const QString userDot = m_home + "/.icons";

    for (const QString &base : m_searchPaths) {
        const QDir dir(base);
        for (const QString &id : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            // The first folder in search order wins, like icon lookup does.
            if (seen.contains(id) || skipped.contains(id) || id.startsWith("0-active-theme"))
                continue;
            const QString path = dir.filePath(id);
            const Parsed parsed = parseIndex(path + "/index.theme");
            if (!parsed.valid)
                continue;
            seen.insert(id);
            // Adwaita marks itself Hidden, but it is GNOME's default theme and a
            // real choice, like GNOME Tweaks shows it.
            if ((parsed.hidden && id != "Adwaita") || parsed.directories.isEmpty())
                continue;
            IconThemeInfo info;
            info.id = id;
            info.name = parsed.name.isEmpty() ? id : parsed.name;
            info.comment = parsed.comment;
            info.path = path;
            info.inherits = parsed.inherits;
            info.user = base == userData || base == userDot;
            out << info;
        }
    }
    std::sort(out.begin(), out.end(), [](const IconThemeInfo &a, const IconThemeInfo &b) {
        return QString::localeAwareCompare(a.id, b.id) < 0;
    });
    return out;
}

const IconThemes::Parsed &IconThemes::theme(const QString &id) {
    auto it = m_themes.find(id);
    if (it != m_themes.end())
        return *it;
    // A theme may be split over several base folders (system + user); the
    // index.theme comes from the first one, and every one is a lookup root.
    Parsed parsed;
    for (const QString &base : m_searchPaths) {
        const QString root = base + "/" + id;
        if (!QFileInfo(root).isDir())
            continue;
        if (!parsed.valid) {
            const Parsed index = parseIndex(root + "/index.theme");
            if (index.valid) {
                // Keep the roots found so far (a user folder without its own
                // index.theme, like ~/.local/share/icons/hicolor).
                const QStringList roots = parsed.roots;
                parsed = index;
                parsed.roots = roots;
            }
        }
        parsed.roots << root;
    }
    return *m_themes.insert(id, parsed);
}

static int sizeDistance(const IconThemes::Directory &d, int size) {
    const int scaled = d.size * d.scale;
    if (d.type == "Fixed")
        return qAbs(scaled - size);
    if (d.type == "Scalable") {
        if (size < d.minSize * d.scale)
            return d.minSize * d.scale - size;
        if (size > d.maxSize * d.scale)
            return size - d.maxSize * d.scale;
        return 0;
    }
    if (size < scaled - d.threshold)
        return scaled - d.threshold - size;
    if (size > scaled + d.threshold)
        return size - scaled - d.threshold;
    return 0;
}

QString IconThemes::lookup(const QString &id, const QString &icon, int size, QStringList &visited) {
    if (visited.contains(id))
        return {};
    visited << id;
    const Parsed &t = theme(id);
    if (!t.valid && t.roots.isEmpty())
        return {};

    static const QStringList extensions{".svg", ".png", ".xpm"};
    QString best;
    int bestDistance = INT_MAX;
    for (const Directory &d : t.directories) {
        const int distance = sizeDistance(d, size);
        if (distance >= bestDistance)
            continue;
        for (const QString &root : t.roots) {
            for (const QString &ext : extensions) {
                const QString file = root + "/" + d.path + "/" + icon + ext;
                if (QFileInfo::exists(file)) {
                    best = file;
                    bestDistance = distance;
                    break;
                }
            }
            if (bestDistance == distance)
                break;
        }
        if (bestDistance == 0)
            break;
    }
    if (!best.isEmpty())
        return best;

    QStringList parents = t.inherits;
    if (parents.isEmpty() && id != "hicolor")
        parents << "hicolor";
    for (const QString &parent : parents) {
        const QString found = lookup(parent, icon, size, visited);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

QString IconThemes::cached(const QString &key, const std::function<QString()> &find) {
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return *it;
    const QString found = find();
    m_cache.insert(key, found);
    return found;
}

QString IconThemes::resolve(const QString &themeId, const QString &icon, int size) {
    return cached(themeId + '\n' + icon + '\n' + QString::number(size), [&] {
        QStringList visited;
        QString found = lookup(themeId, icon, size, visited);
        // Every theme falls back to hicolor, even one that inherits others only.
        if (found.isEmpty() && !visited.contains("hicolor"))
            found = lookup("hicolor", icon, size, visited);
        return found;
    });
}

QString IconThemes::resolveThemed(const QString &themeId, const QString &icon, int size) {
    return cached("themed\n" + themeId + '\n' + icon + '\n' + QString::number(size), [&] {
        // Marking hicolor as seen keeps the lookup inside the theme's own family.
        QStringList visited{"hicolor"};
        return lookup(themeId, icon, size, visited);
    });
}

QImage IconThemes::render(const QString &file, const QSize &size) {
    QImageReader reader(file);
    if (file.endsWith(".svg", Qt::CaseInsensitive))
        reader.setScaledSize(size);
    QImage image = reader.read();
    if (!image.isNull() && image.size() != size)
        image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

QStringList IconThemes::appIconNames(const QString &themeId) {
    QMutexLocker lock(&m_mutex);
    QSet<QString> names;
    QStringList queue{themeId, "hicolor"}, seen;
    while (!queue.isEmpty()) {
        const QString id = queue.takeFirst();
        if (seen.contains(id))
            continue;
        seen << id;
        const Parsed &t = theme(id);
        for (const Directory &d : t.directories) {
            const bool apps = d.context == "Applications" || (d.context.isEmpty() && d.path.contains("apps"));
            if (!apps)
                continue;
            for (const QString &root : t.roots) {
                const QDir dir(root + "/" + d.path);
                for (const QString &file : dir.entryList({"*.svg", "*.png", "*.xpm"}, QDir::Files | QDir::System)) {
                    const QString name = QFileInfo(file).completeBaseName();
                    if (!name.endsWith("-symbolic"))
                        names.insert(name);
                }
            }
        }
        queue << t.inherits;
    }
    QStringList out(names.begin(), names.end());
    out.sort(Qt::CaseInsensitive);
    return out;
}

void IconThemes::clearCache() {
    QMutexLocker lock(&m_mutex);
    m_themes.clear();
    m_cache.clear();
}
