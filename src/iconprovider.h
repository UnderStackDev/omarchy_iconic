#pragma once

#include <QIcon>
#include <QQuickImageProvider>

// Serves "image://icon/<name>" from the active icon theme. Qt Quick has no
// built-in provider for theme icons, and QIcon already knows the freedesktop
// lookup rules (sizes, inheritance, hicolor fallback).
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override {
        const QSize wanted = requestedSize.isValid() ? requestedSize : QSize(48, 48);
        QPixmap pixmap = QIcon::fromTheme(id).pixmap(wanted);
        if (size)
            *size = pixmap.size();
        return pixmap;
    }
};
