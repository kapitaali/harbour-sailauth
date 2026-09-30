#include "applock.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

namespace {

// The devicelock daemon (system bus).
const auto kService = QStringLiteral("org.nemomobile.devicelock");
const auto kPath = QStringLiteral("/authenticator");
const auto kAuthInterface = QStringLiteral("org.nemomobile.devicelock.Authenticator");
const auto kCodeSettingsInterface = QStringLiteral("org.nemomobile.devicelock.SecurityCodeSettings");

// Our side of the protocol: a unique object path on our own connection and
// the interface the daemon calls back on (keep in sync with the adaptor's
// Q_CLASSINFO in applock.h).
const auto kClientPath = QStringLiteral("/org/harbour_sailotp/authenticator");

// Authenticator::AllAvailable from nemo-qml-plugin-devicelock — the same
// value the QML Authenticator type passes by default.  The daemon
// intersects this with what the device can actually do, so if only a PIN
// is available only the PIN is asked for.
const uint kAllAvailableMethods = 0x101F;

const auto kKeyRequireLock = QStringLiteral("requireLock");

// Sandbox-aware settings location.  Inside the Sailjail/firejail sandbox
// the app can only write to directories the launch profile creates and
// whitelists: for OrganizationName=harbour.sailotp and
// ApplicationName=harbour-sailotp that is
// ~/.config/harbour.sailotp/harbour-sailotp/.  The legacy
// ~/.config/harbour-sailotp is whitelisted but never created, so a
// QSettings("harbour-sailotp", ...) file there silently fails to persist.
// The same absolute path is used outside the sandbox, so manual debug runs
// share the state.  (QSettings is constructed with an explicit file name,
// so the org/app strings below only name the file, not the directory.)
QString settingsFilePath()
{
    const QString path = QDir::homePath()
            + QStringLiteral("/.config/harbour.sailotp/harbour-sailotp/settings.conf");
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
}

} // namespace

AppLockAdaptor::AppLockAdaptor(AppLock *owner)
    : QDBusAbstractAdaptor(owner)
    , m_owner(owner)
{
}

void AppLockAdaptor::Authenticated(const QDBusVariant &)
{
    m_owner->handleAuthenticated();
}

void AppLockAdaptor::PermissionGranted(uint)
{
    m_owner->handleAuthenticated();
}

void AppLockAdaptor::Aborted()
{
    m_owner->handleAborted();
}

AppLock::AppLock(QObject *parent)
    : QObject(parent)
    , m_settings(settingsFilePath(), QSettings::IniFormat)
    , m_requireLock(m_settings.value(kKeyRequireLock, false).toBool())
    , m_securityCodeSet(false)
    , m_gatePending(false)
    , m_authenticating(false)
    , m_lockQueryFailed(false)
{
    new AppLockAdaptor(this);

    if (!QDBusConnection::systemBus().registerObject(
                kClientPath, this, QDBusConnection::ExportAdaptors)) {
        qWarning() << "lock gate: could not register D-Bus client object";
    }

    qDebug() << "lock gate: started, requireLock =" << m_requireLock;
    refresh();
}

bool AppLock::requireLock() const
{
    return m_requireLock;
}

void AppLock::setRequireLock(bool on)
{
    if (m_requireLock == on)
        return;

    m_requireLock = on;
    m_settings.setValue(kKeyRequireLock, on);
    m_settings.sync();
    qDebug() << "lock gate: requireLock =" << on;
    emit requireLockChanged();
}

bool AppLock::securityCodeSet() const
{
    return m_securityCodeSet;
}

bool AppLock::lockQueryFailed() const
{
    return m_lockQueryFailed;
}

void AppLock::setLockQueryFailed(bool failed)
{
    if (m_lockQueryFailed == failed)
        return;
    m_lockQueryFailed = failed;
    emit lockQueryFailedChanged();
}

void AppLock::refresh()
{
    // Properties.Get — written out by hand because
    // QDBusMessage::createPropertyCall does not exist in Qt 5.6.
    QDBusMessage msg = QDBusMessage::createMethodCall(
                kService, kPath,
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("Get"));
    msg.setArguments({ kCodeSettingsInterface, QStringLiteral("SecurityCodeSet") });

    QDBusPendingCallWatcher *watcher =
            new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();

        if (w->isError()) {
            // Fail open: if the daemon cannot be reached there is no device
            // lock to enforce, and gating startup on a dead service would
            // trap the user behind an overlay that can never be dismissed.
            qWarning() << "lock gate: SecurityCodeSet query failed:"
                       << w->error().message();
            setLockQueryFailed(true);
        } else {
            // Get returns 'v'; depending on Qt the argument arrives wrapped
            // in a QDBusVariant or already unwrapped — accept both.
            const QVariant first = w->reply().arguments().value(0);
            const QVariant inner = first.userType() == qMetaTypeId<QDBusVariant>()
                    ? first.value<QDBusVariant>().variant()
                    : first;
            const bool set = inner.toBool();
            setLockQueryFailed(false);
            if (set != m_securityCodeSet) {
                m_securityCodeSet = set;
                emit securityCodeSetChanged();
            }
            qDebug() << "lock gate: securityCodeSet =" << set;
        }

        if (m_gatePending) {
            m_gatePending = false;
            if (m_requireLock && m_securityCodeSet)
                authenticate();
            else
                emit gateFinished(true);
        }
    });
}

void AppLock::startGate()
{
    if (m_gatePending || m_authenticating)
        return;

    if (!m_requireLock) {
        emit gateFinished(true);
        return;
    }

    m_gatePending = true;
    refresh();
}

void AppLock::authenticate()
{
    if (m_authenticating)
        return;

    m_authenticating = true;
    qDebug() << "lock gate: requesting security code";

    QDBusMessage call = QDBusMessage::createMethodCall(
                kService, kPath, kAuthInterface,
                QStringLiteral("RequestPermission"));
    // Qt 5.6's QDBusMessage has no operator<< for QDBusObjectPath; passing
    // the whole argument list keeps the D-Bus types exact (o, s, a{sv}, u).
    call.setArguments({ QVariant::fromValue(QDBusObjectPath(kClientPath)),
                        QStringLiteral("Unlock SailOTP"),
                        QVariantMap(),
                        QVariant(kAllAvailableMethods) });

    QDBusPendingCallWatcher *watcher =
            new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();

        if (w->isError()) {
            // The request never got off the ground (daemon unreachable or
            // refused) — there will be no callback, so release the flag and
            // let the gate through rather than wedging startup.
            qWarning() << "lock gate: RequestPermission failed:"
                       << w->error().message();
            m_authenticating = false;
            finishGate(true);
        }
    });
}

void AppLock::handleAuthenticated()
{
    if (!m_authenticating)
        return;

    m_authenticating = false;
    qDebug() << "lock gate: authenticated";
    finishGate(true);
    emit authenticated();
}

void AppLock::handleAborted()
{
    if (!m_authenticating)
        return;

    m_authenticating = false;
    qDebug() << "lock gate: authentication aborted";
    finishGate(false);
}

void AppLock::finishGate(bool passed)
{
    if (!m_gatePending)
        return;

    m_gatePending = false;
    emit gateFinished(passed);
}
