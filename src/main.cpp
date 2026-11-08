// iconic — icon themes and app launchers for Omarchy.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "iconprovider.h"
#include "installer.h"
#include "portalfilepicker.h"
#include "previewprovider.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    // Commands run without a display (the Omarchy hook runs from a detached
    // batch), so they never start the GUI side of Qt.
    if (Cli::wantsCli(args)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("iconic");
        return Cli::run(args);
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("iconic");
    app.setApplicationVersion("0.1.0");

    // Matches the window to iconic.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("iconic");

    // Qt on Hyprland has no desktop platform theme to tell it where icon
    // themes live or which one is active, so give it the XDG folders and
    // take the theme from gsettings like GTK and Flatpak apps do.
    QIcon::setThemeSearchPaths(Backend::iconSearchPaths());
    app.setWindowIcon(QIcon::fromTheme("iconic"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;
    backend.setAsyncShellIcons(true);
    if (!backend.currentTheme().isEmpty())
        QIcon::setThemeName(backend.currentTheme());
    // Catches up after the theme was changed elsewhere, e.g. by the old plugin.
    backend.updateShellIcons(true);
    Installer::cleanupStale();
    QObject::connect(&backend, &Backend::themeApplied, &app, [](const QString &id) { QIcon::setThemeName(id); });

    QQmlApplicationEngine engine;
    engine.addImageProvider("icon", new IconProvider);
    engine.addImageProvider("preview", new PreviewProvider(backend.iconThemes()));
    engine.addImageProvider("appicon", new AppIconProvider(backend.iconThemes()));
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    PortalFilePicker filePicker;
    PortalFilePicker iconPicker;  // its own, so a picked icon never lands in the installer
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("filePicker", &filePicker);
    engine.rootContext()->setContextProperty("iconPicker", &iconPicker);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
