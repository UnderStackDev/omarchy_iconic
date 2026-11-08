#pragma once

#include <QString>
#include <QStringList>

class IconThemes;

// Makes Omarchy's launcher and dock show the chosen theme's app icons.
//
// The shell doesn't look app icons up in the active icon theme. It indexes
// every apps/ and devices/ icon under ~/.icons, ~/.local/share/icons and the
// XDG icon dirs, keeps the first file it finds per name, and lists SVGs
// before PNGs (AppLibrary.qml, iconIndexScanCommand). So a Firefox icon from
// whichever theme `find` reaches first wins, and a PNG theme such as Yaru
// never wins at all.
//
// The layer is a folder at the front of that scan, ~/.icons/iconic-apps/apps,
// with one SVG per launcher icon: a symlink when the theme has an SVG, or a
// small SVG wrapping the PNG otherwise. Without an index.theme it is not an
// icon theme, so GTK and Qt apps ignore it. The shell re-reads it whenever its
// app menu or app grid opens.
namespace ShellIcons {
struct Stats {
    int linked = 0;   // SVG symlinks
    int wrapped = 0;  // PNGs wrapped in SVG
    int themed = 0;   // icons the theme itself has
};

QString layerPath();
// The theme the layer was last built for, empty when there is none.
QString builtFor(const QString &layer = layerPath());

// Icon names used by installed launchers (.desktop Icon=), excluding paths.
QStringList desktopIconNames(const QStringList &applicationDirs);
QStringList defaultApplicationDirs();

// Names to try in a theme for a launcher's icon: as written, then the last
// part of a reverse-DNS id (org.mozilla.firefox -> firefox), then lowercased.
QStringList candidates(const QString &name);

// The file the launcher should show for `name` in `theme`: the theme's own
// icon under any candidate name, else the app's own hicolor or pixmaps icon.
QString resolveForLauncher(IconThemes &themes, const QString &theme, const QString &name, int size = 256,
                           bool *themed = nullptr);

// Rebuilds the layer for `theme`, replacing the old one atomically.
bool rebuild(IconThemes &themes, const QString &theme, const QStringList &iconNames,
             const QString &layer = layerPath(), QString *error = nullptr, Stats *stats = nullptr);

// An SVG that shows `png` at its own size. Public for tests.
QByteArray wrapRaster(const QString &file);
}
