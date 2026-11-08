#pragma once

#include <QQuickImageProvider>
#include <QUrl>

#include "iconthemes.h"
#include "shellicons.h"

// Serves "image://preview/<theme>/<icon>|<fallback>|…": one icon from any
// installed theme, so every theme card can show its own icons. The first name
// the theme has wins, since themes name the same app differently.
class PreviewProvider : public QQuickImageProvider {
public:
    explicit PreviewProvider(IconThemes *themes)
        : QQuickImageProvider(QQuickImageProvider::Image), m_themes(themes) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        const QSize wanted = requestedSize.isValid() ? requestedSize : QSize(32, 32);
        // Qt Quick may hand the id over still percent-encoded ("|" as %7C).
        const QString decoded = QUrl::fromPercentEncoding(id.toUtf8());
        const int slash = decoded.indexOf('/');
        QImage image;
        if (slash > 0) {
            const QString theme = decoded.left(slash);
            for (const QString &name : decoded.mid(slash + 1).split('|', Qt::SkipEmptyParts)) {
                const QString file = m_themes->resolve(theme, name, wanted.width());
                if (!file.isEmpty()) {
                    image = IconThemes::render(file, wanted);
                    if (!image.isNull())
                        break;
                }
            }
        }
        // An icon the theme lacks stays blank rather than failing the load.
        if (image.isNull()) {
            image = QImage(wanted, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        }
        if (size)
            *size = image.size();
        return image;
    }

private:
    IconThemes *m_themes;
};

// Serves "image://appicon/<theme>/<Icon= value>": an app's icon the way the
// Omarchy launcher will show it with Iconic's layer (theme first, then the
// app's own), or the file itself for an absolute path.
class AppIconProvider : public QQuickImageProvider {
public:
    explicit AppIconProvider(IconThemes *themes)
        : QQuickImageProvider(QQuickImageProvider::Image), m_themes(themes) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        const QSize wanted = requestedSize.isValid() ? requestedSize : QSize(32, 32);
        const QString decoded = QUrl::fromPercentEncoding(id.toUtf8());
        const int slash = decoded.indexOf('/');
        QImage image;
        if (slash > 0) {
            const QString theme = decoded.left(slash);
            const QString icon = decoded.mid(slash + 1);
            const QString file = icon.startsWith('/') ? icon
                                 : ShellIcons::resolveForLauncher(*m_themes, theme, icon, wanted.width());
            if (!file.isEmpty())
                image = IconThemes::render(file, wanted);
        }
        if (image.isNull()) {
            image = QImage(wanted, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        }
        if (size)
            *size = image.size();
        return image;
    }

private:
    IconThemes *m_themes;
};
