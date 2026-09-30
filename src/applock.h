#ifndef APPLOCK_H
#define APPLOCK_H

/*
 * Start gate: requires the device security code (PIN/pattern) before the
 * app opens, when the user has enabled "Require device lock" in Settings.
 *
 * Harbour does not allow importing org.nemomobile.devicelock (the QML
 * wrapper Jolla's own apps use, e.g. sailfish-utilities), and
 * Qt.labs.settings is not on the allow-list either — so this talks to the
 * same system daemon (org.nemomobile.devicelock on the system bus) directly
 * over QtDBus and persists the toggle with QSettings.  Qt5DBus and Qt5Core
 * are both already linked (the QR scanner needs D-Bus), so the RPM
 * validator sees no new dependencies.
 *
 * Protocol (mirrors NemoDeviceLock::Authenticator in
 * nemo-qml-plugin-devicelock, which is the sanctioned client of this
 * daemon):
 *
 *   1. export a client object implementing
 *      org.nemomobile.devicelock.client.Authenticator
 *   2. call org.nemomobile.devicelock.Authenticator.RequestPermission(
 *          clientPath, message, {}, methods)
 *   3. the lock screen slides up and asks for the security code; on
 *      success the daemon calls back PermissionGranted() (or
 *      Authenticated()), on cancel Aborted()
 *
 * The daemon's D-Bus policy (org.nemomobile.devicelock.conf) explicitly
 * allows the default context to send to the service and receive the
 * client callbacks, so no privileges are needed.
 *
 * Sandbox note (Sailjail = firejail under the hood): the launch profile
 * enables "dbus-system filter" (default deny) from Base.permission, and
 * none of the Harbour-allowed Permissions values grants
 * org.nemomobile.devicelock — only the system-app profiles
 * (sailfish-browser.profile, voicecall-ui.profile) mention it.  Without a
 * matching profile the daemon is unreachable from the app: the
 * SecurityCodeSet query fails, the gate fails open, and Settings reports
 * lockQueryFailed instead of pretending no PIN is set.  On a device this
 * is fixed by installing /etc/sailjail/permissions/harbour-sailotp.profile
 * (sailjaild picks profiles up by desktop name and adds them to the
 * firejail command line); such a file cannot ship inside the RPM because
 * the Harbour validator rejects paths outside /usr.
 */

#include <QObject>
#include <QSettings>

#include <QDBusAbstractAdaptor>
#include <QDBusVariant>

class AppLock;

class AppLockAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nemomobile.devicelock.client.Authenticator")
public:
    explicit AppLockAdaptor(AppLock *owner);

public slots:
    Q_NOREPLY void Authenticated(const QDBusVariant &authenticationToken);
    Q_NOREPLY void PermissionGranted(uint method);
    Q_NOREPLY void Aborted();

private:
    AppLock *m_owner;
};

class AppLock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool requireLock READ requireLock WRITE setRequireLock NOTIFY requireLockChanged)
    Q_PROPERTY(bool securityCodeSet READ securityCodeSet NOTIFY securityCodeSetChanged)
    // True while the last SecurityCodeSet query failed (daemon unreachable,
    // most often a sandbox that does not grant org.nemomobile.devicelock).
    // Lets the UI distinguish "no PIN is set" from "cannot check".
    Q_PROPERTY(bool lockQueryFailed READ lockQueryFailed NOTIFY lockQueryFailedChanged)
public:
    explicit AppLock(QObject *parent = nullptr);

    bool requireLock() const;
    void setRequireLock(bool on);
    bool securityCodeSet() const;
    bool lockQueryFailed() const;

    // Re-read SecurityCodeSet from the daemon (async).
    Q_INVOKABLE void refresh();
    // Called once from the root QML at startup: decides whether the gate
    // applies and either authenticates immediately or emits gateFinished.
    Q_INVOKABLE void startGate();
    // Prompt for the security code now (initial attempt and the retry
    // button on the lock overlay).
    Q_INVOKABLE void authenticate();

signals:
    void requireLockChanged();
    void securityCodeSetChanged();
    void lockQueryFailedChanged();
    void gateFinished(bool passed);
    void authenticated();

private:
    friend class AppLockAdaptor;
    void handleAuthenticated();
    void handleAborted();
    void finishGate(bool passed);
    void setLockQueryFailed(bool failed);

    QSettings m_settings;
    bool m_requireLock;
    bool m_securityCodeSet;
    bool m_gatePending;
    bool m_authenticating;
    bool m_lockQueryFailed;
};

#endif
