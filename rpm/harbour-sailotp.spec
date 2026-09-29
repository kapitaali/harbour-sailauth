Name:       harbour-sailotp
Summary:    TOTP 2FA authenticator for Sailfish OS
Version:    0.1.0
Release:    1
Group:      Qt/Qt
License:    GPL-3.0-only
URL:        https://github.com/kapitaali/harbour-sailotp
Source0:    %{name}-%{version}.tar.bz2

# sailfishsilica + the Silica add-on module used for the file picker, the
# QtMultimedia QML import behind the camera viewfinder, and the SQLite
# driver (loaded dynamically by name, so nothing auto-detects it).
Requires:   sailfishsilica-qt5
Requires:   sailfish-components-pickers-qt5
Requires:   qt5-qtdeclarative-import-multimedia
Requires:   qt5-plugin-sqldriver-sqlite

BuildRequires: pkgconfig(sailfishapp) >= 1.0.2
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Sql)
BuildRequires: pkgconfig(Qt5Multimedia)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: desktop-file-utils

%description
A native TOTP (RFC 6238) authenticator for Sailfish OS: local-first account
storage, live codes with a countdown, clipboard copy, otpauth:// import and
a cover that shows the current code. No cloud, no tracking.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 harbour-sailotp.pro
%make_build

%install
rm -rf %{buildroot}
%qmake5_install

%files
%defattr(-,root,root,-)
%{_bindir}/harbour-sailotp
%{_datadir}/harbour-sailotp
%{_datadir}/applications/harbour-sailotp.desktop
%{_datadir}/icons/hicolor/*/apps/harbour-sailotp.png
