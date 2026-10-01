#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>

/*
 * Persistent app settings.
 *
 * Backed by an explicitly named INI file at the one config location that is
 * both whitelisted and actually writable inside the Sailjail/firejail
 * sandbox (see settingsFilePath() in settings.cpp).  QSettings' usual
 * org/app form would resolve to ~/.config/harbour-sailauth, which the launch
 * profile whitelists but never creates — writes there fail silently.
 */
class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    bool soundEnabled() const;
    void setSoundEnabled(bool on);

signals:
    void soundEnabledChanged();

private:
    static QString settingsFilePath();

    QSettings m_settings;
    bool m_soundEnabled;
};

#endif // SETTINGS_H
