#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace {
const auto kKeySoundEnabled = QStringLiteral("soundEnabled");
}

/*
 * Sandbox-aware settings location.  Inside the Sailjail/firejail sandbox
 * the app can only write to directories the launch profile creates and
 * whitelists: for OrganizationName=harbour.sailauth and
 * ApplicationName=harbour-sailauth that is
 * ~/.config/harbour.sailauth/harbour-sailauth/.  The legacy
 * ~/.config/harbour-sailauth is whitelisted but never created, so a
 * QSettings("harbour-sailauth", ...) file there silently fails to persist.
 * The same absolute path is used outside the sandbox, so manual debug runs
 * share the state.  QSettings gets an explicit file name, so no org/app
 * path resolution happens at all.
 */
QString Settings::settingsFilePath()
{
    const QString path = QDir::homePath()
            + QStringLiteral("/.config/harbour.sailauth/harbour-sailauth/settings.conf");
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings(settingsFilePath(), QSettings::IniFormat)
    , m_soundEnabled(m_settings.value(kKeySoundEnabled, true).toBool())
{
    qDebug() << "settings file:" << m_settings.fileName();
}

bool Settings::soundEnabled() const
{
    return m_soundEnabled;
}

void Settings::setSoundEnabled(bool on)
{
    if (m_soundEnabled == on)
        return;
    m_soundEnabled = on;
    m_settings.setValue(kKeySoundEnabled, on);
    emit soundEnabledChanged();
}
