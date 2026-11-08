#include "desktopsettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

static QString configHome() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

static bool writeAtomically(const QString &path, const QByteArray &data, QString *error) {
    // Write through a symlink to its target, so a dotfiles link survives.
    const QFileInfo info(path);
    const QString target = info.isSymLink() ? info.symLinkTarget() : path;
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = QObject::tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool IniFile::setValue(const QString &path, const QString &section, const QString &key, const QString &value,
                       QString *error) {
    QStringList lines;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lines = QString::fromUtf8(in.readAll()).split('\n');
        if (!lines.isEmpty() && lines.last().isEmpty())
            lines.removeLast();
        in.close();
    }

    const QString header = "[" + section + "]";
    int sectionStart = -1, sectionEnd = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const QString t = lines[i].trimmed();
        if (t == header) {
            sectionStart = i;
            continue;
        }
        if (sectionStart >= 0 && t.startsWith('[') && t.endsWith(']')) {
            sectionEnd = i;
            break;
        }
    }

    const QString entry = key + "=" + value;
    if (sectionStart < 0) {
        if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
            lines << QString();
        lines << header << entry;
    } else {
        bool replaced = false;
        for (int i = sectionStart + 1; i < sectionEnd; ++i) {
            const QString t = lines[i].trimmed();
            const int eq = t.indexOf('=');
            if (eq > 0 && t.left(eq).trimmed() == key) {
                lines[i] = entry;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            // Right after the section's last key, before any blank lines.
            int at = sectionEnd;
            while (at > sectionStart + 1 && lines[at - 1].trimmed().isEmpty())
                --at;
            lines.insert(at, entry);
        }
    }
    return writeAtomically(path, (lines.join('\n') + '\n').toUtf8(), error);
}

QString IniFile::value(const QString &path, const QString &section, const QString &key) {
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString header = "[" + section + "]";
    bool inside = false;
    for (const QString &raw : QString::fromUtf8(in.readAll()).split('\n')) {
        const QString t = raw.trimmed();
        if (t.startsWith('[') && t.endsWith(']')) {
            inside = t == header;
            continue;
        }
        const int eq = t.indexOf('=');
        if (inside && eq > 0 && t.left(eq).trimmed() == key)
            return t.mid(eq + 1).trimmed();
    }
    return {};
}

QString IconicConfig::path() {
    return configHome() + "/iconic/config.toml";
}

IconicConfig IconicConfig::load(const QString &file) {
    IconicConfig config;
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return config;
    const QString text = QString::fromUtf8(in.readAll());
    static const QRegularExpression mode(R"re(^\s*mode\s*=\s*"(\w+)")re", QRegularExpression::MultilineOption);
    static const QRegularExpression theme(R"re(^\s*theme\s*=\s*"([^"]*)")re", QRegularExpression::MultilineOption);
    config.lock = mode.match(text).captured(1) == "lock";
    config.theme = theme.match(text).captured(1);
    return config;
}

bool IconicConfig::save(const QString &file, QString *error) const {
    QString text = "# Iconic settings. \"follow\" lets each Omarchy theme pick the icon theme;\n"
                   "# \"lock\" keeps `theme` across Omarchy theme switches.\n";
    text += QString("mode = \"%1\"\n").arg(lock ? "lock" : "follow");
    text += QString("theme = \"%1\"\n").arg(theme);
    return writeAtomically(file, text.toUtf8(), error);
}

DesktopSettings::DesktopSettings(const QString &gsettings) : m_gsettings(gsettings) {}

QString DesktopSettings::parseGsettingsString(const QString &output) {
    QString value = output.trimmed();
    if (value.size() >= 2 && value.startsWith('\'') && value.endsWith('\''))
        value = value.mid(1, value.size() - 2);
    return value;
}

QString DesktopSettings::currentTheme(QString *error) const {
    QProcess process;
    process.start(m_gsettings, {"get", "org.gnome.desktop.interface", "icon-theme"});
    if (!process.waitForFinished(3000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = QObject::tr("Could not read the icon theme from gsettings");
        return {};
    }
    return parseGsettingsString(QString::fromUtf8(process.readAllStandardOutput()));
}

bool DesktopSettings::apply(const QString &theme, QString *error) const {
    static const QRegularExpression safe("^[A-Za-z0-9._+-]+$");
    if (!safe.match(theme).hasMatch()) {
        if (error)
            *error = QObject::tr("Invalid theme name: %1").arg(theme);
        return false;
    }
    QProcess process;
    process.start(m_gsettings, {"set", "org.gnome.desktop.interface", "icon-theme", theme});
    if (!process.waitForFinished(3000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = QObject::tr("gsettings could not set the icon theme: %1")
                         .arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
        return false;
    }
    return syncFiles(theme, error);
}

bool DesktopSettings::syncFiles(const QString &theme, QString *error) const {
    const QString config = configHome();
    bool ok = true;
    auto set = [&](const QString &file, const QString &section, const QString &key) {
        if (IniFile::value(file, section, key) == theme)
            return;
        QString e;
        if (!IniFile::setValue(file, section, key, theme, &e)) {
            ok = false;
            if (error && error->isEmpty())
                *error = e;
        }
    };
    set(config + "/gtk-3.0/settings.ini", "Settings", "gtk-icon-theme-name");
    set(config + "/gtk-4.0/settings.ini", "Settings", "gtk-icon-theme-name");
    set(config + "/kdeglobals", "Icons", "Theme");
    // qt5ct/qt6ct only matter when the user set them up; don't create them.
    for (const QString &ct : {QString("qt5ct"), QString("qt6ct")}) {
        const QString file = config + "/" + ct + "/" + ct + ".conf";
        if (QFileInfo::exists(file))
            set(file, "Appearance", "icon_theme");
    }
    return ok;
}

QString ThemeSetHook::path() {
    return configHome() + "/omarchy/hooks/theme-set.d/iconic";
}

QString ThemeSetHook::script(const QString &binary) {
    QString quoted = binary;
    quoted.replace('\'', "'\\''");
    return QString(
               "#!/bin/bash\n"
               "# Installed by Iconic. After an Omarchy theme switch this keeps the icon\n"
               "# theme you locked in Iconic, or (in Follow mode) copies Omarchy's pick to\n"
               "# the GTK, KDE and Qt config files. Iconic rewrites this file when it moves.\n"
               "# The installed iconic wins; the path below is the build that wrote this.\n"
               "ICONIC=$(command -v iconic) || ICONIC='%1'\n"
               "[[ -x $ICONIC ]] || exit 0\n"
               "exec \"$ICONIC\" hook theme-set\n")
        .arg(quoted);
}

bool ThemeSetHook::ensure(const QString &binary, QString *error) {
    const QByteArray wanted = script(binary).toUtf8();
    QFile current(path());
    if (current.open(QIODevice::ReadOnly) && current.readAll() == wanted)
        return true;
    current.close();
    if (!writeAtomically(path(), wanted, error))
        return false;
    QFile::setPermissions(path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup |
                                      QFile::ReadOther);
    return true;
}
