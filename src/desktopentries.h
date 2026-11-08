#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// The app launchers (.desktop files) the Omarchy launcher shows, and Iconic's
// per-app changes to them.
//
// Changes never touch a package's own file. The launcher reads
// ~/.local/share/applications first and the first file per desktop id wins,
// so a copy there with the same id replaces the original, survives package
// updates, and goes away again on Reset. Copies are marked
// X-Iconic-Override=true. Launchers the user made (web apps, own apps) are
// edited in place, after a one-time backup of the original.
namespace DesktopEntries {

enum class Source { System, User, Flatpak, WebApp };

struct Entry {
    QString id;          // desktop id: path below applications/, "/" -> "-", no .desktop
    QString path;        // the file that wins
    QString name;
    QString comment;
    QString icon;        // Icon= as written (a name or an absolute path)
    QString exec;
    bool noDisplay = false;        // hidden from the launcher (NoDisplay/Hidden)
    bool hiddenByDesktop = false;  // OnlyShowIn/NotShowIn or Omarchy's own hide list
    bool override = false;         // our copy in the user folder
    bool backedUp = false;         // a user launcher we edited in place
    QString originalPath;          // what Reset goes back to
    QString originalName, originalIcon;
    bool originalNoDisplay = false;
    Source source = Source::System;
    bool modified() const { return override || backedUp; }
};

// applications/ folders in lookup order: the user's first.
QStringList applicationDirs();
QString userDir();
QString backupDir();
QString customIconDir();

// Every Application launcher, the winning file per id, sorted by name.
// `desktops` is XDG_CURRENT_DESKTOP split on ":"; `omarchyHides` lists ids
// Omarchy hides itself.
QList<Entry> scan(const QStringList &dirs = applicationDirs(), const QStringList &desktops = {},
                  const QSet<QString> &omarchyHides = {});

// Reads one key from the [Desktop Entry] group.
QString value(const QString &file, const QString &key);

struct Change {
    QString name;          // empty = keep
    QString icon;          // empty = keep
    int noDisplay = -1;    // -1 keep, 0 show, 1 hide
};

// Applies `change` to `entry` as described above. Returns the file written.
QString apply(const Entry &entry, const Change &change, QString *error = nullptr);

// Undoes Iconic's changes: deletes the override copy, or restores the backup.
bool reset(const Entry &entry, QString *error = nullptr);

// Copies a picked image to Iconic's icon folder and returns the absolute path
// to use as Icon=. Only PNG, SVG and XPM are accepted.
QString importIcon(const QString &file, const QString &id, QString *error = nullptr);

// Tells desktop-file users (and the launcher) that the folder changed.
void refreshDatabase();
}
