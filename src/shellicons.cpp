#include "shellicons.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>
#include <QStandardPaths>

#include "iconthemes.h"

QString ShellIcons::layerPath() {
    return QDir::homePath() + "/.icons/iconic-apps";
}

QString ShellIcons::builtFor(const QString &layer) {
    QFile marker(layer + "/.theme");
    if (!marker.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(marker.readAll()).trimmed();
}

QStringList ShellIcons::defaultApplicationDirs() {
    QStringList dirs;
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << dir + "/applications";
    dirs.removeDuplicates();
    return dirs;
}

QStringList ShellIcons::desktopIconNames(const QStringList &applicationDirs) {
    QSet<QString> names;
    for (const QString &dir : applicationDirs) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile file(it.next());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            bool main = false;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith('[')) {
                    main = line == "[Desktop Entry]";
                    continue;
                }
                if (main && line.startsWith("Icon=")) {
                    const QString icon = line.mid(5).trimmed();
                    if (!icon.isEmpty() && !icon.startsWith('/') && !icon.contains("://"))
                        names.insert(icon);
                    break;
                }
            }
        }
    }
    QStringList out(names.begin(), names.end());
    out.sort();
    return out;
}

QStringList ShellIcons::candidates(const QString &name) {
    QStringList out{name};
    const QString last = name.section('.', -1);
    if (name.contains('.') && !last.isEmpty())
        out << last;
    out << name.toLower() << last.toLower();
    out.removeDuplicates();
    return out;
}

QByteArray ShellIcons::wrapRaster(const QString &file) {
    QImage image = QImageReader(file).read();
    if (image.isNull())
        return {};
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    // Re-encoding also turns XPM and other formats into PNG.
    image.save(&buffer, "PNG");
    const QString w = QString::number(image.width()), h = QString::number(image.height());
    return QString("<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                   "width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">"
                   "<image width=\"%1\" height=\"%2\" xlink:href=\"data:image/png;base64,")
               .arg(w, h)
               .toUtf8() +
           png.toBase64() + "\"/></svg>\n";
}

static QString pixmap(const QString &name) {
    for (const QString &ext : {".svg", ".png", ".xpm"}) {
        const QString file = "/usr/share/pixmaps/" + name + ext;
        if (QFileInfo::exists(file))
            return file;
    }
    return {};
}

QString ShellIcons::resolveForLauncher(IconThemes &themes, const QString &theme, const QString &name, int size,
                                      bool *themed) {
    // The theme's own icon first, under any of the app's usual names; then
    // the app's own icon, so no other theme leaks in.
    for (const QString &candidate : candidates(name)) {
        const QString file = themes.resolveThemed(theme, candidate, size);
        if (!file.isEmpty()) {
            if (themed)
                *themed = true;
            return file;
        }
    }
    if (themed)
        *themed = false;
    const QString own = themes.resolve("hicolor", name, size);
    return own.isEmpty() ? pixmap(name) : own;
}

bool ShellIcons::rebuild(IconThemes &themes, const QString &theme, const QStringList &iconNames,
                         const QString &layer, QString *error, Stats *stats) {
    Stats local;
    Stats &s = stats ? *stats : local;
    const QString parent = QFileInfo(layer).absolutePath();
    const QString building = parent + "/.iconic-apps-" + QString::number(QCoreApplication::applicationPid());
    QDir(building).removeRecursively();
    if (!QDir().mkpath(building + "/apps")) {
        if (error)
            *error = QCoreApplication::translate("ShellIcons", "Could not create %1").arg(building);
        return false;
    }

    for (const QString &name : iconNames) {
        bool fromTheme = false;
        const QString file = resolveForLauncher(themes, theme, name, 256, &fromTheme);
        if (fromTheme)
            ++s.themed;
        if (file.isEmpty())
            continue;

        const QString target = building + "/apps/" + name + ".svg";
        if (file.endsWith(".svg", Qt::CaseInsensitive)) {
            if (QFile::link(file, target))
                ++s.linked;
            continue;
        }
        const QByteArray svg = wrapRaster(file);
        QFile out(target);
        if (!svg.isEmpty() && out.open(QIODevice::WriteOnly) && out.write(svg) == svg.size())
            ++s.wrapped;
    }

    QFile marker(building + "/.theme");
    if (marker.open(QIODevice::WriteOnly))
        marker.write(theme.toUtf8() + "\n");
    marker.close();

    // Swap the finished folder in, so the shell never scans a half-built one.
    const QString old = parent + "/.iconic-apps-old";
    QDir(old).removeRecursively();
    if (QFileInfo::exists(layer) && !QDir().rename(layer, old)) {
        QDir(building).removeRecursively();
        if (error)
            *error = QCoreApplication::translate("ShellIcons", "Could not replace %1").arg(layer);
        return false;
    }
    if (!QDir().rename(building, layer)) {
        QDir().rename(old, layer);
        QDir(building).removeRecursively();
        if (error)
            *error = QCoreApplication::translate("ShellIcons", "Could not create %1").arg(layer);
        return false;
    }
    QDir(old).removeRecursively();
    return true;
}
