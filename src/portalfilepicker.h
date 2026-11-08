#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

// Opens files through xdg-desktop-portal's FileChooser, so the picker is the
// desktop's own (and works the same for sandboxed builds), never QFileDialog.
class PortalFilePicker : public QObject {
    Q_OBJECT
public:
    explicit PortalFilePicker(QObject *parent = nullptr) : QObject(parent) {}

    // Asks for one or more files; answers with picked() or failed().
    Q_INVOKABLE void openFiles(const QString &title);

signals:
    void picked(const QStringList &paths);
    void failed(const QString &message);

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    QString m_requestPath;
};
