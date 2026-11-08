#include "cli.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "backend.h"

static const char *usage =
    "Usage: iconic [command]\n"
    "\n"
    "  (no command), open        Open the window\n"
    "  status                    Show the icon theme and the Follow/Lock mode\n"
    "  list-themes               List installed icon themes\n"
    "  apply <theme> [--lock|--follow]\n"
    "                            Set the desktop's icon theme\n"
    "  install <archive|folder>... [--replace|--keep-both] [--apply]\n"
    "                            Install icon themes into ~/.local/share/icons\n"
    "  lock | follow             Keep the current theme across Omarchy theme\n"
    "                            switches, or let each Omarchy theme pick\n"
    "  app list                  List launchers (--modified, --hidden to filter)\n"
    "  app set <id> [--name <name>] [--icon <name|file>] [--hide|--show]\n"
    "                            Change a launcher (a copy; the original stays)\n"
    "  app reset <id>            Undo Iconic's changes to a launcher\n"
    "  shell-icons               Rebuild the launcher/dock icon layer\n"
    "  hook theme-set            Run by Omarchy after a theme switch\n"
    "\n"
    "Every command takes --json.\n";

bool Cli::wantsCli(const QStringList &args) {
    return !args.isEmpty() && args.first() != "open";
}

static void print(const QString &text) {
    QTextStream(stdout) << text << Qt::endl;
}

static int failure(const QString &message, bool json) {
    if (json)
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"ok", false}, {"error", message}}).toJson(QJsonDocument::Compact)
                            << Qt::endl;
    QTextStream(stderr) << "iconic: " << message << Qt::endl;
    return 1;
}

static QJsonObject status(const Backend &backend) {
    return {{"ok", true},
            {"theme", backend.currentTheme()},
            {"mode", backend.lock() ? "lock" : "follow"},
            {"lockedTheme", backend.lock() ? backend.lockedTheme() : QString()}};
}

int Cli::run(const QStringList &rawArgs) {
    QStringList args = rawArgs;
    const bool json = args.removeAll("--json") > 0;
    const QString command = args.value(0);

    if (command == "help" || command == "--help" || command == "-h") {
        QTextStream(stdout) << usage;
        return 0;
    }

    Backend backend;

    if (command == "status") {
        if (json)
            print(QJsonDocument(status(backend)).toJson(QJsonDocument::Compact));
        else
            print(QString("%1 (%2)").arg(backend.currentTheme(),
                                         backend.lock() ? "locked" : "follows Omarchy"));
        return backend.error().isEmpty() ? 0 : failure(backend.error(), json);
    }

    if (command == "list-themes") {
        QJsonArray rows;
        for (const IconThemeInfo &t : backend.themeList()) {
            rows << QJsonObject{{"id", t.id}, {"name", t.name}, {"user", t.user}, {"path", t.path},
                                {"active", t.id == backend.currentTheme()}};
            if (!json)
                print(QString("%1 %2%3").arg(t.id == backend.currentTheme() ? "*" : " ", t.id,
                                             t.user ? "  (user)" : ""));
        }
        if (json)
            print(QJsonDocument(QJsonObject{{"ok", true}, {"themes", rows}}).toJson(QJsonDocument::Compact));
        return 0;
    }

    if (command == "apply") {
        const QString theme = args.value(1);
        if (theme.isEmpty() || theme.startsWith("--"))
            return failure("apply needs a theme name (see iconic list-themes)", json);
        if (args.contains("--lock") && args.contains("--follow"))
            return failure("use either --lock or --follow", json);
        if (args.contains("--follow") && !backend.setLock(false))
            return failure(backend.error(), json);
        if (!backend.applyTheme(theme))
            return failure(backend.error(), json);
        if (args.contains("--lock") && !backend.setLock(true))
            return failure(backend.error(), json);
        if (json)
            print(QJsonDocument(status(backend)).toJson(QJsonDocument::Compact));
        else
            print(QString("Icon theme: %1%2").arg(theme, backend.lock() ? " (locked)" : ""));
        return 0;
    }

    if (command == "lock" || command == "follow") {
        if (!backend.setLock(command == "lock"))
            return failure(backend.error(), json);
        if (json)
            print(QJsonDocument(status(backend)).toJson(QJsonDocument::Compact));
        else
            print(command == "lock" ? QString("Locked to %1").arg(backend.currentTheme())
                                    : QString("Following the Omarchy theme"));
        return 0;
    }

    if (command == "install") {
        const QStringList sources = args.mid(1).filter(QRegularExpression("^[^-]"));
        if (sources.isEmpty())
            return failure("install needs an archive or folder", json);
        const auto conflict = args.contains("--replace")     ? Installer::Conflict::Replace
                              : args.contains("--keep-both") ? Installer::Conflict::KeepBoth
                                                             : Installer::Conflict::Skip;
        QStringList installed, notes;
        for (const QString &source : sources) {
            QStringList n;
            installed << backend.installNow(source, conflict, &n);
            notes << n;
            if (!backend.error().isEmpty() && installed.isEmpty())
                return failure(backend.error(), json);
        }
        if (args.contains("--apply") && !installed.isEmpty() && !backend.applyTheme(installed.first()))
            return failure(backend.error(), json);
        if (json) {
            print(QJsonDocument(QJsonObject{{"ok", true},
                                            {"installed", QJsonArray::fromStringList(installed)},
                                            {"notes", QJsonArray::fromStringList(notes)}})
                      .toJson(QJsonDocument::Compact));
        } else {
            for (const QString &id : installed)
                print("Installed " + id);
            for (const QString &note : notes)
                print(note);
        }
        return installed.isEmpty() ? 1 : 0;
    }

    if (command == "app") {
        const QString sub = args.value(1);
        auto option = [&](const QString &flag) {
            const int i = args.indexOf(flag);
            return i >= 0 ? args.value(i + 1) : QString();
        };
        if (sub == "list") {
            QJsonArray rows;
            for (const DesktopEntries::Entry &e : backend.appList()) {
                if (args.contains("--modified") && !e.modified())
                    continue;
                if (args.contains("--hidden") && !e.noDisplay)
                    continue;
                rows << QJsonObject{{"id", e.id}, {"name", e.name}, {"icon", e.icon}, {"hidden", e.noDisplay},
                                    {"modified", e.modified()}, {"path", e.path}};
                if (!json)
                    print(QString("%1%2  %3  (%4)").arg(e.modified() ? "* " : "  ", e.id, e.name, e.icon));
            }
            if (json)
                print(QJsonDocument(QJsonObject{{"ok", true}, {"apps", rows}}).toJson(QJsonDocument::Compact));
            return 0;
        }
        const QString id = args.value(2);
        const DesktopEntries::Entry *entry = nullptr;
        for (const DesktopEntries::Entry &e : backend.appList())
            if (e.id == id || e.id == QString(id).remove(".desktop"))
                entry = &e;
        if (!entry)
            return failure(QString("no launcher with id \"%1\" (see iconic app list)").arg(id), json);
        const QString realId = entry->id;
        if (sub == "set") {
            QString icon = option("--icon");
            if (!icon.isEmpty() && (icon.contains('/') || icon.contains('.')) && QFileInfo::exists(icon)) {
                icon = backend.importIcon(QFileInfo(icon).absoluteFilePath(), realId);
                if (icon.isEmpty())
                    return failure(backend.error(), json);
            }
            const QString name = option("--name").isEmpty() ? entry->name : option("--name");
            const bool hidden = args.contains("--hide") ? true : args.contains("--show") ? false : entry->noDisplay;
            if (!backend.saveApp(realId, name, icon, hidden))
                return failure(backend.error(), json);
        } else if (sub == "reset") {
            if (!backend.resetApp(realId))
                return failure(backend.error(), json);
        } else {
            return failure("app needs list, set or reset", json);
        }
        if (json)
            print(QJsonDocument(QJsonObject{{"ok", true}, {"id", realId}}).toJson(QJsonDocument::Compact));
        else
            print(QString("%1 %2").arg(sub == "set" ? "Changed" : "Reset", realId));
        return 0;
    }

    if (command == "shell-icons") {
        if (!backend.updateShellIcons())
            return failure(backend.error(), json);
        if (json)
            print(QJsonDocument(status(backend)).toJson(QJsonDocument::Compact));
        else
            print(QString("Launcher icons rebuilt for %1").arg(backend.currentTheme()));
        return 0;
    }

    if (command == "hook" && args.value(1) == "theme-set") {
        if (!backend.runThemeSetHook())
            return failure(backend.error(), json);
        if (json)
            print(QJsonDocument(status(backend)).toJson(QJsonDocument::Compact));
        return 0;
    }

    QTextStream(stderr) << usage;
    return failure(QString("unknown command: %1").arg(command), json);
}
