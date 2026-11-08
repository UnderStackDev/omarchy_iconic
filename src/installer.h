#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <memory>

class QTemporaryDir;

// Installs icon themes from archives or folders into ~/.local/share/icons,
// the one user folder Flatpak apps can also see (~/.icons is not).
//
// Archives are untrusted, so they are listed before they are unpacked
// (size and entry limits), unpacked by bsdtar into a private temp folder
// (which refuses absolute paths and ".." on its own), and only regular files,
// folders and symlinks that stay inside the theme are copied out.
namespace Installer {

enum class Conflict { Ask, Replace, KeepBoth, Skip };

struct Found {
    QString id;          // folder name, what gsettings will store
    QString name;        // Name= for display
    QString sourcePath;  // inside the unpacked temp folder (or the dropped folder)
    bool exists = false; // a theme with this id is already in the target folder
    QString installedAs; // filled in after install
};

struct Prepared {
    QString source;
    std::shared_ptr<QTemporaryDir> unpacked;  // kept alive until install
    QString root;         // what was unpacked or dropped; links may not leave it
    QList<Found> themes;
    QStringList skipped;  // human-readable notes: cursor-only themes etc.
    QString error;
    bool ok() const { return error.isEmpty(); }
};

// Limits for one archive.
constexpr qint64 maxUnpackedBytes = 2LL * 1024 * 1024 * 1024;
constexpr int maxEntries = 300000;

QString targetRoot();
bool isArchive(const QString &path);

// Unpacks (when needed) and finds the icon themes in `path`.
Prepared prepare(const QString &path, const QString &target = targetRoot());

// Copies the found themes into `target`. A theme that exists is handled per
// `conflict`; Ask is treated as Skip, the caller asks first.
bool install(Prepared &prepared, Conflict conflict, const QString &target = targetRoot(),
             QString *error = nullptr);

// Copies `from` into `to` keeping only files, folders and symlinks that
// resolve inside `allowedRoot` (default `from`) and, once installed, inside
// the folder that holds `to`. Public for tests.
// With `move`, files are moved rather than copied (for our own temp copy).
bool copyTree(const QString &from, const QString &to, const QString &allowedRoot = {}, QString *error = nullptr,
              bool move = false);

// Removes leftovers of an install that was killed midway (older than an hour).
void cleanupStale(const QString &target = targetRoot());

// Parses the listing of `bsdtar -tvf` for total size and entry count.
bool listingWithinLimits(const QByteArray &listing, qint64 *bytes = nullptr, int *entries = nullptr);
}
