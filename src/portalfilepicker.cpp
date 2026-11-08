#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QRandomGenerator>
#include <QUrl>

void PortalFilePicker::openFiles(const QString &title) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("No session bus, so no file picker"));
        return;
    }

    // The portal answers on a Request object whose path is known up front;
    // subscribing before the call means the answer can't be missed.
    const QString token = QString("iconic%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    const QString path = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    if (!m_requestPath.isEmpty())
        bus.disconnect("org.freedesktop.portal.Desktop", m_requestPath, "org.freedesktop.portal.Request",
                       "Response", this, SLOT(onResponse(uint, QVariantMap)));
    m_requestPath = path;
    bus.connect("org.freedesktop.portal.Desktop", path, "org.freedesktop.portal.Request", "Response", this,
                SLOT(onResponse(uint, QVariantMap)));

    QDBusMessage call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                       "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.FileChooser", "OpenFile");
    QVariantMap options{{"handle_token", token}, {"multiple", true}, {"modal", true}};
    call << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError())
            emit failed(tr("The file picker could not open: %1").arg(reply.error().message()));
        w->deleteLater();
    });
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    // 0 = picked, 1 = cancelled, 2 = failed.
    if (response == 1)
        return;
    if (response != 0) {
        emit failed(tr("The file picker failed"));
        return;
    }
    QStringList paths;
    for (const QString &uri : results.value("uris").toStringList()) {
        const QUrl url(uri);
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty())
        emit picked(paths);
}
