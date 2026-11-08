QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = iconic_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/desktopsettings.h ../src/iconthemes.h ../src/shellicons.h ../src/theme.h ../src/translator.h
SOURCES += iconic_tests.cpp ../src/backend.cpp ../src/desktopsettings.cpp ../src/iconthemes.cpp ../src/shellicons.cpp ../src/theme.cpp ../src/translator.cpp
