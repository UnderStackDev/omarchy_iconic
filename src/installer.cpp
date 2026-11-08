#include "installer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "iconthemes.h"

static QString tr(const char *text) {
    return QCoreApplication::translate("Installer", text);
}

QString Installer::targetRoot() {
    return QDir::homePath() + "/.local/share/icons";
}

bool Installer::isArchive(const QString &path) {
    static const QRegularExpression archive(R"(\.(tar(\.(gz|xz|bz2|zst|lz4|lzma))?|tgz|txz|tbz2?|zip|7z)$)",
                                            QRegularExpression::CaseInsensitiveOption);
    return archive.match(path).hasMatch();
}

bool Installer::listingWithinLimits(const QByteArray &listing, qint64 *bytes, int *entries) {
    // bsdtar -tv prints ls-style lines: perms, links, owner, group, size, ...
    qint64 total = 0;
    int count = 0;
    for (const QByteArray &line : listing.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        ++count;
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() > 4)
            total += qMax<qint64>(0, parts[4].toLongLong());
    }
    if (bytes)
        *bytes = total;
    if (entries)
        *entries = count;
    return total <= maxUnpackedBytes && count <= maxEntries;
}

static bool insideRoot(const QString &root, const QString &path) {
    const QString r = QDir::cleanPath(root) + "/";
    return QDir::cleanPath(path).startsWith(r);
}

bool Installer::copyTree(const QString &from, const QString &to, const QString &allowedRoot, QString *error,
                         bool move) {
    const QString root = QFileInfo(from).canonicalFilePath();
    const QString allowed = allowedRoot.isEmpty() ? root : QFileInfo(allowedRoot).canonicalFilePath();
    const QString targetParent = QFileInfo(to).absolutePath();
    if (root.isEmpty() || !QDir().mkpath(to)) {
        if (error)
            *error = tr("Could not create %1").arg(to);
        return false;
    }
    // Walk the resolved folder, so "inside" is judged on real paths.
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString relative = QDir(root).relativeFilePath(path);
        const QString dest = to + "/" + relative;

        if (info.isSymLink()) {
            // Icon themes link sizes and aliases to each other, and variants
            // link into their sibling theme (Papirus-Dark -> ../../Papirus).
            // Keep those; drop any link that leaves the archive or would
            // point outside the icon folder once installed.
            const QString resolved = QDir::cleanPath(info.symLinkTarget());
            if (resolved.isEmpty() || !insideRoot(allowed, resolved))
                continue;
            QString text;
            if (insideRoot(root, resolved)) {
                text = QFileInfo(dest).dir().relativeFilePath(to + "/" + QDir(root).relativeFilePath(resolved));
            } else {
                text = info.readSymLink();
                if (QDir::isAbsolutePath(text))
                    continue;
            }
            const QString landed = QDir::cleanPath(QFileInfo(dest).absolutePath() + "/" + text);
            if (!insideRoot(targetParent, landed))
                continue;
            QDir().mkpath(QFileInfo(dest).absolutePath());
            QFile::link(text, dest);
            continue;
        }
        if (info.isDir()) {
            QDir().mkpath(dest);
            continue;
        }
        if (!info.isFile())
            continue;  // devices, sockets, fifos
        QDir().mkpath(QFileInfo(dest).absolutePath());
        QFile::remove(dest);
        // Our own unpacked files can simply move; a dropped folder is copied.
        if (!(move && QFile::rename(path, dest)) && !QFile::copy(path, dest)) {
            if (error)
                *error = tr("Could not copy %1").arg(relative);
            return false;
        }
        QFile::setPermissions(dest, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
    return true;
}

static void findThemes(const QString &root, const QString &target, Installer::Prepared &out) {
    QDirIterator it(root, {"index.theme"}, QDir::Files, QDirIterator::Subdirectories);
    QStringList seen;
    while (it.hasNext()) {
        const QString file = it.next();
        // A symlinked index.theme could point anywhere; only real files count.
        if (QFileInfo(file).isSymLink())
            continue;
        const QString dir = QFileInfo(file).absolutePath();
        const QString id = QFileInfo(dir).fileName();
        const IconThemes::Parsed parsed = IconThemes::parseIndex(file);
        if (!parsed.valid)
            continue;
        if (parsed.directories.isEmpty()) {
            out.skipped << QCoreApplication::translate("Installer", "%1 is a cursor theme, not an icon theme").arg(id);
            continue;
        }
        static const QRegularExpression safe("^[A-Za-z0-9._+-]+$");
        if (!safe.match(id).hasMatch() || id.startsWith('.')) {
            out.skipped << QCoreApplication::translate("Installer", "%1 has an unusable folder name").arg(id);
            continue;
        }
        if (seen.contains(id))
            continue;
        seen << id;
        Installer::Found found;
        found.id = id;
        found.name = parsed.name.isEmpty() ? id : parsed.name;
        found.sourcePath = dir;
        found.exists = QFileInfo::exists(target + "/" + id);
        out.themes << found;
    }
}

Installer::Prepared Installer::prepare(const QString &path, const QString &target) {
    Prepared out;
    out.source = path;
    const QFileInfo info(path);
    if (!info.exists()) {
        out.error = tr("%1 does not exist").arg(path);
        return out;
    }

    QString root;
    if (info.isDir()) {
        root = info.canonicalFilePath();
        if (insideRoot(target, root) || QDir::cleanPath(root) == QDir::cleanPath(target)) {
            out.error = tr("%1 is already in the icon folder").arg(path);
            return out;
        }
    } else if (isArchive(path)) {
        QProcess list;
        list.start("bsdtar", {"-tvf", path});
        if (!list.waitForFinished(60000) || list.exitCode() != 0) {
            out.error = tr("Could not read the archive %1").arg(info.fileName());
            return out;
        }
        qint64 bytes = 0;
        int entries = 0;
        if (!listingWithinLimits(list.readAllStandardOutput(), &bytes, &entries)) {
            out.error = tr("%1 is too big to unpack (%2 MB in %3 files)")
                            .arg(info.fileName())
                            .arg(bytes / (1024 * 1024))
                            .arg(entries);
            return out;
        }
        // Unpack beside the target, on the same disk, so installing can move
        // files instead of copying tens of thousands of them.
        QDir().mkpath(target);
        out.unpacked = std::make_shared<QTemporaryDir>(target + "/.iconic-unpack-XXXXXX");
        if (!out.unpacked->isValid()) {
            out.error = tr("Could not create a temporary folder");
            return out;
        }
        QProcess extract;
        // bsdtar refuses absolute paths and ".." unless told otherwise (-P).
        extract.start("bsdtar", {"-xf", path, "-C", out.unpacked->path(), "--no-same-owner", "--no-same-permissions"});
        if (!extract.waitForFinished(300000) || extract.exitCode() != 0) {
            out.error = tr("Could not unpack %1: %2")
                            .arg(info.fileName(), QString::fromUtf8(extract.readAllStandardError()).trimmed());
            return out;
        }
        root = out.unpacked->path();
    } else {
        out.error = tr("%1 is not a folder or a supported archive (.tar.gz, .tar.xz, .tar.zst, .zip, .7z)")
                        .arg(info.fileName());
        return out;
    }

    out.root = root;
    findThemes(root, target, out);
    if (out.themes.isEmpty() && out.error.isEmpty())
        out.error = out.skipped.isEmpty() ? tr("No icon theme found in %1").arg(info.fileName())
                                          : out.skipped.join("; ");
    return out;
}

static QString freeId(const QString &target, const QString &id) {
    for (int n = 2;; ++n) {
        const QString candidate = QString("%1-%2").arg(id).arg(n);
        if (!QFileInfo::exists(target + "/" + candidate))
            return candidate;
    }
}

bool Installer::install(Prepared &prepared, Conflict conflict, const QString &target, QString *error) {
    if (!QDir().mkpath(target)) {
        if (error)
            *error = tr("Could not create %1").arg(target);
        return false;
    }
    bool any = false;
    for (Found &theme : prepared.themes) {
        QString id = theme.id;
        const bool exists = QFileInfo::exists(target + "/" + id);
        if (exists) {
            if (conflict == Conflict::Ask || conflict == Conflict::Skip) {
                prepared.skipped << tr("%1 is already installed; skipped").arg(id);
                continue;
            }
            if (conflict == Conflict::KeepBoth)
                id = freeId(target, id);
        }

        // Build beside the target, then swap in, so a failed copy leaves the
        // old theme untouched.
        const QString staging = target + "/.iconic-install-" + id;
        QDir(staging).removeRecursively();
        QString e;
        // Files unpacked by us move (same disk); a dropped folder is copied.
        if (!copyTree(theme.sourcePath, staging, prepared.root, &e, prepared.unpacked != nullptr)) {
            QDir(staging).removeRecursively();
            if (error)
                *error = e;
            return false;
        }
        const QString final = target + "/" + id;
        const QString old = target + "/.iconic-old-" + id;
        if (QFileInfo::exists(final)) {
            QDir(old).removeRecursively();
            const QFileInfo existing(final);
            // A symlinked theme loses only its link.
            if (existing.isSymLink() ? !QFile::remove(final) : !QDir().rename(final, old)) {
                QDir(staging).removeRecursively();
                if (error)
                    *error = tr("Could not replace %1").arg(final);
                return false;
            }
        }
        if (!QDir().rename(staging, final)) {
            QDir().rename(old, final);
            QDir(staging).removeRecursively();
            if (error)
                *error = tr("Could not move %1 into place").arg(id);
            return false;
        }
        QDir(old).removeRecursively();

        // The icon cache makes GTK lookups fast; a failure here only costs speed.
        QProcess cache;
        cache.start("gtk-update-icon-cache", {"-f", "-t", "-q", final});
        cache.waitForFinished(60000);

        theme.installedAs = id;
        any = true;
    }
    // The unpacked copy has done its job; don't leave it in the icon folder.
    prepared.unpacked.reset();
    if (!any && error && error->isEmpty())
        *error = prepared.skipped.isEmpty() ? tr("Nothing was installed") : prepared.skipped.join("; ");
    return any;
}

void Installer::cleanupStale(const QString &target) {
    const QDir dir(target);
    const auto hourAgo = QDateTime::currentDateTime().addSecs(-3600);
    for (const QFileInfo &entry : dir.entryInfoList({".iconic-unpack-*", ".iconic-install-*", ".iconic-old-*"},
                                                    QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
        if (!entry.isSymLink() && entry.lastModified() < hourAgo)
            QDir(entry.absoluteFilePath()).removeRecursively();
    }
}
