Name:       harbour-sailauth
Summary:    TOTP 2FA authenticator for Sailfish OS
Version: 0.2.9
Release:    1
Group:      Qt/Qt
License:    GPL-3.0-only
URL:        https://github.com/kapitaali/harbour-sailauth
Source0:    %{name}-%{version}.tar.bz2

# sailfishsilica + the QtMultimedia QML import behind the camera viewfinder.
# Deliberately no Requires for sailfish-components-pickers-qt5 or
# qt5-plugin-sqldriver-sqlite: the Harbour validator rejects both package
# names (allowed list does not contain them), and both are base-image
# packages — sailfish-browser/jolla-contacts pull in the pickers, lipstick/
# jolla-notes the SQLite driver — so they are always present anyway.
Requires:   sailfishsilica-qt5
Requires:   qt5-qtdeclarative-import-multimedia

BuildRequires: pkgconfig(sailfishapp) >= 1.0.2
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Sql)
BuildRequires: pkgconfig(Qt5Multimedia)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(openssl)
BuildRequires: desktop-file-utils

%description
A native TOTP (RFC 6238) authenticator for Sailfish OS: local-first account
storage, live codes with a countdown, clipboard copy, otpauth:// import and
a cover that shows the current code. No cloud, no tracking.

%prep
%setup -q -n %{name}-%{version}

%build
# Hand the git-derived version to the app so the About page shows the real
# thing instead of a copy that rots (APP_VERSION survives qmake untouched
# via the environment, see harbour-sailauth.pro).
export APP_VERSION=%{version}
%qmake5 harbour-sailauth.pro
%make_build

%install
rm -rf %{buildroot}
%qmake5_install
# The SDK invokes qmake with QMAKE_STRIP=: (no-op), and the build system's
# brp-strip only runs `strip -g`, which keeps .symtab — so the shipped binary
# still reads "not stripped" to file(1) and the Harbour validator. Full-strip
# it here instead.
%{__strip} %{buildroot}%{_bindir}/harbour-sailauth

%files
%defattr(-,root,root,-)
%{_bindir}/harbour-sailauth
%{_datadir}/harbour-sailauth
%{_datadir}/applications/harbour-sailauth.desktop
%{_datadir}/icons/hicolor/*/apps/harbour-sailauth.png
