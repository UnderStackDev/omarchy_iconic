#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // The rest of the Omarchy palette from colors.toml, so the window looks
    // like the rest of the desktop, not just its accent.
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QString darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QString lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString dimForeground READ dimForeground NOTIFY changed)
    Q_PROPERTY(QString brightForeground READ brightForeground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(bool light READ light NOTIFY changed)
    // Omarchy's font (Setup > Font), the monospace the whole desktop uses.
    Q_PROPERTY(QString font READ font NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }

    QString background() const { return color("background"); }
    QString darkBackground() const { return color("dark_background"); }
    QString darkerBackground() const { return color("darker_background"); }
    QString lighterBackground() const { return color("lighter_background"); }
    QString foreground() const { return color("foreground"); }
    QString dimForeground() const { return color("dark_foreground"); }
    QString brightForeground() const { return color("bright_foreground"); }
    QString muted() const { return color("muted"); }
    QString selection() const { return color("selection"); }
    QString danger() const { return color("red"); }
    bool light() const { return m_light; }
    QString font() const { return m_font; }

    static QString readAccent(const QString &path);
    // Every `key = "#rrggbb"` line of colors.toml, plus `mode`.
    static QHash<QString, QString> readColors(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString color(const QString &key) const;

    QString m_accent = "#FFD60A";
    QHash<QString, QString> m_colors;
    bool m_light = false;
    QString m_font = "monospace";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
