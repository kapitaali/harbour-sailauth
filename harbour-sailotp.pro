# harbour-sailotp.pro

TARGET = harbour-sailotp
CONFIG += sailfishapp c++11

# Version shown on the About page: the RPM build exports APP_VERSION (the
# sfdk/git-tag version) in %build, manual qmake runs may pass it as an
# argument, and anything else is a hand-built binary.
isEmpty(APP_VERSION): APP_VERSION = $$getenv(APP_VERSION)
isEmpty(APP_VERSION): APP_VERSION = dev
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"

# make cannot tell that -DAPP_VERSION changed (compiler flags are invisible
# to it), so an incremental build would keep the previous version baked into
# the binary — it did, once. Bump main()'s timestamp so it recompiles with
# the current define; it is a single translation unit, so this costs a
# second at most.
_version_touch = $$system(touch $$PWD/src/harbour-sailotp.cpp)

QT += core gui qml quick sql multimedia dbus

SOURCES += \
    src/harbour-sailotp.cpp \
    src/totp.cpp \
    src/accountmodel.cpp \
    src/accountfilter.cpp \
    src/database.cpp \
    src/clipboardhelper.cpp \
    src/importer.cpp \
    src/qrfilter.cpp \
    src/settings.cpp

HEADERS += \
    src/totp.h \
    src/accountmodel.h \
    src/accountfilter.h \
    src/database.h \
    src/clipboardhelper.h \
    src/importer.h \
    src/qrfilter.h \
    src/settings.h

# sailfishapp.prf installs the whole qml/ tree; this list exists so the IDE
# and qmake know about the files (and so OTHER_FILES below is complete).
QML_FILES = \
    qml/harbour-sailotp.qml \
    qml/cover/CoverPage.qml \
    qml/components/Toast.qml \
    qml/pages/MainPage.qml \
    qml/pages/AddAccountPage.qml \
    qml/pages/EditAccountPage.qml \
    qml/pages/ConfirmDeletePage.qml \
    qml/pages/ImportPage.qml \
    qml/pages/ScanPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/AboutPage.qml

OTHER_FILES += \
    harbour-sailotp.desktop \
    rpm/harbour-sailotp.spec \
    qml/img/harbour-sailotp.png \
    tests/tst_totp.cpp \
    $$QML_FILES

# Names only, not paths: sailfishapp.prf expands each entry to
# icons/<size>/<TARGET>.png and installs it into icons/hicolor/<size>/apps.
SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256
