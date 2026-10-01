# SailAuth

A native **TOTP 2FA authenticator** for Sailfish OS. It generates the
six-digit time-based one-time codes (RFC 6238) used by GitHub, Google,
Dropbox and friends — entirely on your phone, with no cloud, no accounts
and no tracking.

## Features

- **Live codes** with a per-code countdown ring, refreshed every second
- **Scan QR codes** straight from the screen or paper using the camera —
  powered by the system's zxing service, with a green reticle and an
  on-screen confirmation when a code is found
- **Manual entry** for secrets that come as text (base32, with strict
  validation)
- **Import** `otpauth://` URIs from the clipboard or a file — plain text
  lists as well as andOTP / Aegis / FreeOTP JSON exports
- **Tap a code to copy** it to the clipboard
- **Cover** shows the first account's current code and countdown right on
  the home screen, with a shortcut to add an account
- **Local-first storage**: accounts live in a SQLite database in the app's
  own data directory. Nothing is ever uploaded anywhere.
- **Encrypted backup**: export every account to an AES-256-GCM file in
  Documents, protected by a passphrase you choose, and restore it later —
  on this phone or another one
- **Export to other apps**: the same accounts as a plain-text file of
  `otpauth://` URIs, which GNOME Authenticator, FreeOTP+ and Aegis import
  directly
- **About page** with the installed version, source-code link and a tip
  button

## Requirements

- Sailfish OS **5.1 or later** (released for aarch64, armv7hl and i486;
  tested on a Jolla C2 running SFOS 5.2)
- Camera, for QR scanning (everything else works without one)

## Installing

Grab the RPM from the [releases](../../releases) (or build it yourself,
see below), copy it to the phone and install it with root rights — in the
phone's terminal app or over ssh:

```sh
scp harbour-sailauth-<version>-1.aarch64.rpm defaultuser@<phone>:~
ssh defaultuser@<phone>
devel-su -c 'rpm -Uvh ~/harbour-sailauth-<version>-1.aarch64.rpm'
```

Launch **SailAuth** from the app grid. The app asks for `Camera` (QR
scanning) and `UserDirs` (file import and backup) permissions on first
start — accept them or the app will not open.

## Using it

**Add an account**

- *Scan QR code* on the main page, point the camera at the QR code a
  service shows you (the scanner keeps trying until it locks on), fill in
  the label and save — or
- *Add manually*: enter a label and the base32 secret, pick the digits and
  period if the service deviates from the defaults (6 digits / 30 s), or
- *Import* a text snippet or an export file containing `otpauth://` links;
  you get a preview and import selectively.

**Get a code** — tap an entry in the list to copy the current code (the
countdown tells you how long it stays valid). The cover shows the same
code without opening the app.

**Manage accounts** — long-press an entry for edit and delete; deletion
asks for confirmation.

**Settings** lives in the main page's pull-down menu, alongside **About**
(which has the version, the GitHub link and the Ko-fi tip button).
Settings also has **Export backup** and **Import backup**: the export
writes an encrypted copy of every account to Documents under a passphrase
you choose — or, as a plain-text file, one `otpauth://` URI per line for
GNOME Authenticator and other apps; the import reads an encrypted backup
back — it shows what the file contains before anything is restored.

## Building from source

```sh
git clone https://github.com/kapitaali/harbour-sailauth
cd harbour-sailauth
sfdk build
```

The RPM lands in `RPMS/<target>/`. The Sailfish SDK (`sfdk`) with an
installed SailfishOS target is required; the project builds with
qmake/Qt 5.6-compatible APIs only.

## Privacy

SailAuth has no network access at all: no analytics, no crash reports, no
telemetry. Your secrets and codes stay in the device's app data
directory. The full privacy policy is in [PRIVACY.md](PRIVACY.md).

## License

GPL-3.0-only. If you find SailAuth useful, you can leave a tip on
[Ko-fi](https://ko-fi.com/kapitaali).
