#pragma once

#include <QHash>
#include <functional>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QSize>
#include <QString>
#include <QStringList>

// One installed icon theme, read from its index.theme.
struct IconThemeInfo {
    QString id;           // folder name, which is what gsettings stores
    QString name;         // Name= from index.theme, for display
    QString comment;
    QString path;         // the folder that won (user folders shadow system ones)
    QStringList inherits;
    bool user = false;    // lives in the user's own icon folders, so it can be removed
};

// Finds icon themes and looks up icons in any of them, not just the active
// one. QIcon only knows a single global theme, so previews of every theme
// side by side need their own lookup. It follows the freedesktop icon theme
// spec closely enough for previews: per-directory sizes, Inherits, and the
// hicolor fallback.
class IconThemes {
public:
    explicit IconThemes(const QStringList &searchPaths, const QString &home);

    // Themes worth showing: they have icon directories (so cursor-only
    // themes drop out), aren't Hidden (Adwaita aside), and aren't the hicolor fallback or the
    // "default" cursor theme. Sorted by folder name.
    QList<IconThemeInfo> scan() const;

    // The file that shows `icon` at about `size` pixels in `theme`, or empty.
    // Thread safe, since Qt Quick loads images off the GUI thread.
    QString resolve(const QString &theme, const QString &icon, int size);
    // Like resolve(), but only inside the theme and the themes it inherits,
    // never the hicolor fallback, so callers can tell "the theme has it" from
    // "only the app's own icon exists".
    QString resolveThemed(const QString &theme, const QString &icon, int size);

    // Renders a resolved file at `size`. SVGs go through Qt's SVG image
    // plugin at the target size, so they stay sharp.
    static QImage render(const QString &file, const QSize &size);

    // Forgets cached lookups, after themes were installed or removed.
    void clearCache();

    struct Directory {
        QString path;
        int size = 0, minSize = 0, maxSize = 0, threshold = 2, scale = 1;
        QString type = "Threshold";
    };
    struct Parsed {
        QString name, comment;
        QStringList inherits, roots;
        QList<Directory> directories;
        bool hidden = false;
        bool valid = false;
    };

    // Reads one index.theme. Public for tests.
    static Parsed parseIndex(const QString &file);

private:
    const Parsed &theme(const QString &id);
    QString lookup(const QString &theme, const QString &icon, int size, QStringList &visited);
    QString cached(const QString &key, const std::function<QString()> &find);

    QStringList m_searchPaths;
    QString m_home;
    QMutex m_mutex;
    QHash<QString, Parsed> m_themes;
    QHash<QString, QString> m_cache;
};
