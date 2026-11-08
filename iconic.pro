QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release
TARGET = iconic
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/desktopentries.h \
    src/desktopsettings.h \
    src/iconprovider.h \
    src/iconthemes.h \
    src/installer.h \
    src/portalfilepicker.h \
    src/previewprovider.h \
    src/shellicons.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/desktopentries.cpp \
    src/desktopsettings.cpp \
    src/iconthemes.cpp \
    src/installer.cpp \
    src/portalfilepicker.cpp \
    src/shellicons.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
