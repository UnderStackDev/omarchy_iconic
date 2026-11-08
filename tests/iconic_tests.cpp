#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "backend.h"
#include "desktopsettings.h"
#include "iconthemes.h"
#include "shellicons.h"
#include "installer.h"
#include <QProcess>
#include <QImage>
#include "theme.h"
#include "translator.h"

class IconicTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Every test runs in a throwaway home, so nothing touches the real
        // desktop. HOME and the XDG variables steer QDir and QStandardPaths.
        QVERIFY(m_home.isValid());
        const QString h = m_home.path();
        qputenv("HOME", h.toUtf8());
        qputenv("XDG_CONFIG_HOME", (h + "/.config").toUtf8());
        qputenv("XDG_DATA_HOME", (h + "/.local/share").toUtf8());
        qputenv("XDG_DATA_DIRS", (h + "/system").toUtf8());

        // A fake gsettings that remembers the icon theme in a file.
        m_gsettings = h + "/gsettings";
        write(m_gsettings, "#!/bin/sh\nf=\"$(dirname \"$0\")/gsettings.value\"\n"
                           "case \"$1\" in get) printf \"'%s'\\n\" \"$(cat \"$f\")\";; set) printf %s \"$4\" > \"$f\";; esac\n");
        QFile::setPermissions(m_gsettings, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        write(h + "/gsettings.value", "Plain");

        // System themes: Plain (fixed + scalable dirs), Child inheriting
        // Plain, a cursor-only theme, a hidden one, and hicolor.
        theme(h + "/system/icons/Plain", "[Icon Theme]\nName=Plain Icons\nDirectories=32/apps,scalable/places\n\n"
                                        "[32/apps]\nSize=32\nType=Fixed\n\n[scalable/places]\nSize=48\nType=Scalable\nMinSize=8\nMaxSize=512\n");
        touch(h + "/system/icons/Plain/32/apps/terminal.png");
        touch(h + "/system/icons/Plain/scalable/places/folder.svg");
        theme(h + "/system/icons/Child", "[Icon Theme]\nName=Child\nInherits=Plain\nDirectories=16/apps\n\n[16/apps]\nSize=16\nType=Fixed\n");
        touch(h + "/system/icons/Child/16/apps/terminal.png");
        theme(h + "/system/icons/Pointers", "[Icon Theme]\nName=Pointers\n");
        theme(h + "/system/icons/Secret", "[Icon Theme]\nName=Secret\nHidden=true\nDirectories=16/apps\n\n[16/apps]\nSize=16\n");
        theme(h + "/system/icons/hicolor", "[Icon Theme]\nName=Hicolor\nDirectories=48/apps\n\n[48/apps]\nSize=48\nType=Threshold\n");
        touch(h + "/system/icons/hicolor/48/apps/only-hicolor.png");
        // A user copy of Child shadows the system one.
        theme(h + "/.local/share/icons/Child", "[Icon Theme]\nName=Child (mine)\nInherits=Plain\nDirectories=16/apps\n\n[16/apps]\nSize=16\n");
    }

    void gsettingsQuotesAreStripped()
    {
        QCOMPARE(Backend::parseGsettingsString("'Yaru-blue'\n"), QString("Yaru-blue"));
        QCOMPARE(Backend::parseGsettingsString("Plain"), QString("Plain"));
        QCOMPARE(Backend::parseGsettingsString("''"), QString());
    }

    void iconSearchPathsPutUserFoldersFirst()
    {
        const QStringList paths = Backend::iconSearchPaths();
        QCOMPARE(paths.value(0), m_home.path() + "/.icons");
        QCOMPARE(paths.value(1), m_home.path() + "/.local/share/icons");
        QVERIFY(paths.contains(m_home.path() + "/system/icons"));
    }

    void scanShowsOnlyRealIconThemes()
    {
        IconThemes themes(Backend::iconSearchPaths(), m_home.path());
        QStringList ids;
        for (const auto &t : themes.scan())
            ids << t.id;
        QCOMPARE(ids, QStringList({"Child", "Plain"}));
        const auto child = themes.scan().first();
        QCOMPARE(child.name, QString("Child (mine)"));
        QVERIFY(child.user);
        QVERIFY(!themes.scan().last().user);
    }

    void resolveFollowsSizesInheritsAndHicolor()
    {
        IconThemes themes(Backend::iconSearchPaths(), m_home.path());
        const QString sys = m_home.path() + "/system/icons";
        // Child's user copy has no files, but its system folder does.
        QCOMPARE(themes.resolve("Child", "terminal", 16), sys + "/Child/16/apps/terminal.png");
        QCOMPARE(themes.resolve("Child", "folder", 28), sys + "/Plain/scalable/places/folder.svg");
        QCOMPARE(themes.resolve("Child", "only-hicolor", 28), sys + "/hicolor/48/apps/only-hicolor.png");
        QVERIFY(themes.resolve("Child", "missing", 28).isEmpty());
    }

    void iniSetValueKeepsEverythingElse()
    {
        const QString file = m_home.path() + "/ini/test.ini";
        QDir().mkpath(m_home.path() + "/ini");
        write(file, "# comment\n[Settings]\ngtk-theme-name=Adwaita\ngtk-icon-theme-name=Old\n\n[Other]\nx=1\n");
        QVERIFY(IniFile::setValue(file, "Settings", "gtk-icon-theme-name", "New"));
        QCOMPARE(read(file), QByteArray("# comment\n[Settings]\ngtk-theme-name=Adwaita\ngtk-icon-theme-name=New\n\n[Other]\nx=1\n"));
        QVERIFY(IniFile::setValue(file, "Other", "y", "2"));
        QVERIFY(IniFile::setValue(file, "Icons", "Theme", "New"));
        QCOMPARE(IniFile::value(file, "Other", "y"), QString("2"));
        QCOMPARE(IniFile::value(file, "Icons", "Theme"), QString("New"));
        QCOMPARE(IniFile::value(file, "Settings", "gtk-theme-name"), QString("Adwaita"));
    }

    void configRoundTrips()
    {
        const QString file = m_home.path() + "/cfg/config.toml";
        IconicConfig config;
        config.lock = true;
        config.theme = "Yaru-purple";
        QVERIFY(config.save(file));
        const IconicConfig loaded = IconicConfig::load(file);
        QVERIFY(loaded.lock);
        QCOMPARE(loaded.theme, QString("Yaru-purple"));
        QVERIFY(!IconicConfig::load(file + ".missing").lock);
    }

    void hookScriptQuotesTheBinary()
    {
        const QString script = ThemeSetHook::script("/opt/it's/iconic");
        QVERIFY(script.contains("ICONIC='/opt/it'\\''s/iconic'"));
        QVERIFY(script.contains("hook theme-set"));
    }

    void applyWritesEveryMirror()
    {
        DesktopSettings desktop(m_gsettings);
        QVERIFY(!desktop.apply("bad name; rm -rf"));
        QVERIFY(desktop.apply("Child"));
        QCOMPARE(desktop.currentTheme(), QString("Child"));
        const QString c = m_home.path() + "/.config";
        QCOMPARE(IniFile::value(c + "/gtk-3.0/settings.ini", "Settings", "gtk-icon-theme-name"), QString("Child"));
        QCOMPARE(IniFile::value(c + "/gtk-4.0/settings.ini", "Settings", "gtk-icon-theme-name"), QString("Child"));
        QCOMPARE(IniFile::value(c + "/kdeglobals", "Icons", "Theme"), QString("Child"));
        // qt6ct is only touched when the user has it.
        QVERIFY(!QFile::exists(c + "/qt6ct/qt6ct.conf"));
    }

    void lockSurvivesAnOmarchyThemeSwitch()
    {
        Backend backend(nullptr, m_gsettings, "/usr/bin/iconic");
        QVERIFY(backend.applyTheme("Plain"));
        QVERIFY(backend.setLock(true));
        QCOMPARE(backend.lockedTheme(), QString("Plain"));
        QVERIFY(QFile::exists(ThemeSetHook::path()));
        QVERIFY(!backend.applyTheme("NotInstalled"));

        // Omarchy switches theme and sets its own icon theme in gsettings.
        write(m_home.path() + "/gsettings.value", "Child");
        Backend hook(nullptr, m_gsettings, "/usr/bin/iconic");
        QVERIFY(hook.runThemeSetHook());
        QCOMPARE(DesktopSettings(m_gsettings).currentTheme(), QString("Plain"));

        // In Follow mode the hook keeps Omarchy's pick and syncs the files.
        QVERIFY(hook.setLock(false));
        write(m_home.path() + "/gsettings.value", "Child");
        Backend follow(nullptr, m_gsettings, "/usr/bin/iconic");
        QVERIFY(follow.runThemeSetHook());
        QCOMPARE(DesktopSettings(m_gsettings).currentTheme(), QString("Child"));
        QCOMPARE(IniFile::value(m_home.path() + "/.config/kdeglobals", "Icons", "Theme"), QString("Child"));
    }

    void onlyUnusedUserThemesCanBeRemoved()
    {
        write(m_home.path() + "/gsettings.value", "Plain");
        Backend backend(nullptr, m_gsettings, "/usr/bin/iconic");
        QVERIFY(!backend.canRemove("Plain"));   // system and active
        QVERIFY(backend.canRemove("Child"));    // the user copy
        QVERIFY(backend.removeTheme("Child"));
        QVERIFY(!QFile::exists(m_home.path() + "/.local/share/icons/Child"));
        QVERIFY(QFile::exists(m_home.path() + "/system/icons/Child/index.theme"));
    }

    void shellIconCandidatesCoverReverseDnsIds()
    {
        QCOMPARE(ShellIcons::candidates("org.mozilla.firefox"), QStringList({"org.mozilla.firefox", "firefox"}));
        QCOMPARE(ShellIcons::candidates("Steam"), QStringList({"Steam", "steam"}));
    }

    void shellIconLayerPrefersTheThemeAndNeverLeaksOthers()
    {
        const QString h = m_home.path();
        // A PNG app icon in Plain, a reverse-DNS app only hicolor knows as an
        // SVG, an app only in pixmaps, and a launcher with no icon anywhere.
        QImage red(16, 16, QImage::Format_ARGB32);
        red.fill(Qt::red);
        QDir().mkpath(h + "/system/icons/Plain/32/apps");
        QVERIFY(red.save(h + "/system/icons/Plain/32/apps/firefox.png"));
        write(h + "/system/icons/hicolor/48/apps/org.example.Tool.svg", "<svg xmlns='http://www.w3.org/2000/svg'/>");
        write(h + "/system/applications/firefox.desktop", "[Desktop Entry]\nName=Firefox\nIcon=org.mozilla.firefox\n");
        write(h + "/system/applications/tool.desktop", "[Desktop Entry]\nName=Tool\nIcon=org.example.Tool\n[Desktop Action x]\nIcon=other\n");
        write(h + "/system/applications/abs.desktop", "[Desktop Entry]\nIcon=/opt/abs.png\n");
        write(h + "/system/applications/ghost.desktop", "[Desktop Entry]\nIcon=ghost\n");

        const QStringList names = ShellIcons::desktopIconNames({h + "/system/applications"});
        QCOMPARE(names, QStringList({"ghost", "org.example.Tool", "org.mozilla.firefox"}));

        IconThemes themes(Backend::iconSearchPaths(), h);
        const QString layer = h + "/layer/iconic-apps";
        ShellIcons::Stats stats;
        QString error;
        QVERIFY2(ShellIcons::rebuild(themes, "Plain", names, layer, &error, &stats), qPrintable(error));
        QCOMPARE(ShellIcons::builtFor(layer), QString("Plain"));
        QCOMPARE(stats.themed, 1);
        QCOMPARE(stats.wrapped, 1);
        QCOMPARE(stats.linked, 1);

        // Firefox's PNG became an SVG so the shell's SVG-first scan picks it.
        const QByteArray wrapped = read(layer + "/apps/org.mozilla.firefox.svg");
        QVERIFY(wrapped.startsWith("<svg"));
        QVERIFY(wrapped.contains("data:image/png;base64,"));
        QCOMPARE(QFileInfo(layer + "/apps/org.example.Tool.svg").symLinkTarget(),
                 h + "/system/icons/hicolor/48/apps/org.example.Tool.svg");
        QVERIFY(!QFileInfo::exists(layer + "/apps/ghost.svg"));

        // A rebuild replaces the folder rather than piling up.
        QVERIFY(ShellIcons::rebuild(themes, "Child", {"org.mozilla.firefox"}, layer));
        QCOMPARE(ShellIcons::builtFor(layer), QString("Child"));
        QVERIFY(!QFileInfo::exists(layer + "/apps/org.example.Tool.svg"));
        QCOMPARE(QDir(h + "/layer").entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), QStringList({"iconic-apps"}));
    }

    void listingLimitsCountSizeAndEntries()
    {
        const QByteArray listing =
            "drwxr-xr-x  0 a b       0 Jan  1  2026 Theme/\n"
            "-rw-r--r--  0 a b    1024 Jan  1  2026 Theme/index.theme\n"
            "lrwxr-xr-x  0 a b       0 Jan  1  2026 Theme/x.svg -> y.svg\n";
        qint64 bytes = 0;
        int entries = 0;
        QVERIFY(Installer::listingWithinLimits(listing, &bytes, &entries));
        QCOMPARE(bytes, qint64(1024));
        QCOMPARE(entries, 3);
        QVERIFY(!Installer::listingWithinLimits("-rw-r--r-- 0 a b 99999999999 Jan 1 2026 big\n"));
    }

    void copyTreeKeepsInternalLinksOnly()
    {
        const QString h = m_home.path() + "/copy";
        write(h + "/src/Theme/index.theme", "[Icon Theme]\n");
        write(h + "/src/Theme/32/apps/a.svg", "<svg/>");
        QVERIFY(QFile::link("a.svg", h + "/src/Theme/32/apps/b.svg"));
        write(h + "/secret.txt", "nope");
        QVERIFY(QFile::link(h + "/secret.txt", h + "/src/Theme/32/apps/evil.svg"));
        QVERIFY(QFile::link("/etc/passwd", h + "/src/Theme/32/apps/passwd.svg"));

        QVERIFY(Installer::copyTree(h + "/src/Theme", h + "/dst/Theme"));
        QCOMPARE(read(h + "/dst/Theme/32/apps/a.svg"), QByteArray("<svg/>"));
        QCOMPARE(QFileInfo(h + "/dst/Theme/32/apps/b.svg").symLinkTarget(), h + "/dst/Theme/32/apps/a.svg");
        QVERIFY(!QFileInfo(h + "/dst/Theme/32/apps/evil.svg").exists());
        QVERIFY(!QFileInfo(h + "/dst/Theme/32/apps/passwd.svg").isSymLink());

        // A variant may link into its sibling when both came in one archive.
        write(h + "/src/Base/32/apps/c.svg", "<svg/>");
        QDir().mkpath(h + "/src/Variant/32");
        QVERIFY(QFile::link("../../Base/32/apps", h + "/src/Variant/32/apps"));
        QVERIFY(Installer::copyTree(h + "/src/Variant", h + "/dst/Variant", h + "/src"));
        QCOMPARE(QFileInfo(h + "/dst/Variant/32/apps").symLinkTarget(), h + "/dst/Base/32/apps");
        // Without the archive as the allowed root, it is dropped.
        QVERIFY(Installer::copyTree(h + "/src/Variant", h + "/dst2/Variant"));
        QVERIFY(!QFileInfo(h + "/dst2/Variant/32/apps").isSymLink());
    }

    void archiveInstallFindsThemesAndHandlesConflicts()
    {
        const QString h = m_home.path() + "/arch";
        write(h + "/pack/Nice/index.theme", "[Icon Theme]\nName=Nice Icons\nDirectories=48/apps\n\n[48/apps]\nSize=48\n");
        write(h + "/pack/Nice/48/apps/app.png", "x");
        write(h + "/pack/Nice-Dark/index.theme", "[Icon Theme]\nName=Nice Dark\nInherits=Nice\nDirectories=48/apps\n\n[48/apps]\nSize=48\n");
        write(h + "/pack/Pointer/index.theme", "[Icon Theme]\nName=Pointer\n");
        QProcess tar;
        tar.setWorkingDirectory(h + "/pack");
        tar.start("bsdtar", {"-czf", h + "/themes.tar.gz", "Nice", "Nice-Dark", "Pointer"});
        QVERIFY(tar.waitForFinished() && tar.exitCode() == 0);

        const QString target = h + "/icons";
        Installer::Prepared prepared = Installer::prepare(h + "/themes.tar.gz", target);
        QVERIFY2(prepared.ok(), qPrintable(prepared.error));
        QStringList ids;
        for (const auto &t : prepared.themes) ids << t.id;
        ids.sort();
        QCOMPARE(ids, QStringList({"Nice", "Nice-Dark"}));
        QCOMPARE(prepared.skipped.size(), 1);
        QVERIFY(prepared.skipped.first().contains("cursor"));

        QVERIFY(Installer::install(prepared, Installer::Conflict::Skip, target));
        QVERIFY(QFile::exists(target + "/Nice/48/apps/app.png"));
        QVERIFY(QFile::exists(target + "/Nice-Dark/index.theme"));
        QVERIFY(QDir(target).entryList(QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).filter(".iconic").isEmpty());

        // Again: everything exists now.
        prepared = Installer::prepare(h + "/themes.tar.gz", target);
        QVERIFY(prepared.themes.first().exists);
        QString error;
        QVERIFY(!Installer::install(prepared, Installer::Conflict::Skip, target, &error));
        QVERIFY(error.contains("already installed"));

        prepared = Installer::prepare(h + "/themes.tar.gz", target);
        QVERIFY(Installer::install(prepared, Installer::Conflict::KeepBoth, target));
        QVERIFY(QFile::exists(target + "/Nice-2/index.theme"));

        write(target + "/Nice/stale.txt", "old");
        prepared = Installer::prepare(h + "/themes.tar.gz", target);
        QVERIFY(Installer::install(prepared, Installer::Conflict::Replace, target));
        QVERIFY(!QFile::exists(target + "/Nice/stale.txt"));
    }

    void prepareRejectsWhatIsNotATheme()
    {
        const QString h = m_home.path() + "/bad";
        write(h + "/notes.txt", "hello");
        QVERIFY(!Installer::prepare(h + "/notes.txt", h + "/icons").ok());
        QVERIFY(!Installer::prepare(h + "/missing.zip", h + "/icons").ok());
        QDir().mkpath(h + "/empty");
        QVERIFY(Installer::prepare(h + "/empty", h + "/icons").error.contains("No icon theme"));
    }

    void backendInstallsDroppedFoldersInTheBackground()
    {
        const QString h = m_home.path();
        write(h + "/drop/Dropped/index.theme", "[Icon Theme]\nName=Dropped\nDirectories=16/apps\n\n[16/apps]\nSize=16\n");
        Backend backend(nullptr, m_gsettings, "/usr/bin/iconic");
        backend.installPaths({QUrl::fromLocalFile(h + "/drop/Dropped").toString()});
        QTRY_VERIFY(!backend.busy());
        QCOMPARE(backend.lastInstalled(), QStringList({"Dropped"}));
        QVERIFY(QFile::exists(h + "/.local/share/icons/Dropped/index.theme"));

        // Dropping it again asks before touching the installed copy.
        backend.installPaths({h + "/drop/Dropped"});
        QTRY_VERIFY(!backend.busy() && !backend.conflicts().isEmpty());
        QCOMPARE(backend.conflicts(), QStringList({"Dropped"}));
        backend.resolveConflicts("cancel");
        QVERIFY(backend.conflicts().isEmpty());

        // A variant linking into Dropped keeps it from being removed first.
        write(h + "/.local/share/icons/Variant/index.theme", "[Icon Theme]\nName=Variant\nDirectories=16/apps\n\n[16/apps]\nSize=16\n");
        QDir().mkpath(h + "/.local/share/icons/Variant/16");
        QFile::remove(h + "/.local/share/icons/Variant/16/apps");
        QVERIFY(QFile::link("../../Dropped/16/apps", h + "/.local/share/icons/Variant/16/apps"));
        backend.refresh();
        QCOMPARE(backend.dependents("Dropped"), QStringList({"Variant"}));
        QVERIFY(!backend.removeTheme("Dropped"));
        QVERIFY(QFile::exists(h + "/.local/share/icons/Dropped/index.theme"));
        QVERIFY(backend.removeTheme("Variant"));
        QVERIFY(backend.removeTheme("Dropped"));

        backend.installPaths({"https://example.com/theme.zip"});
        QVERIFY(backend.error().contains("Download"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    QTemporaryDir m_home;
    QString m_gsettings;

    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
    static void touch(const QString &path) { write(path, "x"); }
    static void theme(const QString &dir, const QByteArray &index) { write(dir + "/index.theme", index); }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
};

QTEST_MAIN(IconicTests)
#include "iconic_tests.moc"
