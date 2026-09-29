# harbour-sailotp.pro

TARGET = harbour-sailotp
CONFIG += sailfishapp c++11

QT += core gui qml quick sql multimedia

SOURCES += \
    src/harbour-sailotp.cpp \
    src/totp.cpp \
    src/accountmodel.cpp \
    src/database.cpp \
    src/clipboardhelper.cpp \
    src/importer.cpp

HEADERS += \
    src/totp.h \
    src/accountmodel.h \
    src/database.h \
    src/clipboardhelper.h \
    src/importer.h

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
SAILFISHAPP_ICONS = 86x86 108x108 128x128 256x256
