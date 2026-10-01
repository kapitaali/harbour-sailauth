# Privacy Policy — SailAuth

*Last updated: 1 October 2026*

SailAuth is an offline two-factor authentication (TOTP) app for Sailfish
OS, distributed through the Jolla Store and from this repository. This
policy covers the app itself — the store, your device and the websites
you visit have their own policies.

## In short

The app has **no network access, no servers, no accounts, no analytics
and no tracking**. Everything it holds stays on your device — there is
nowhere it could send anything.

## What the app stores, and where

All data lives in the app's sandboxed directories on your device:

- **Accounts database** (`sailauth.db`): for each account — the issuer
  (e.g. "GitHub"), the account name, the shared secret key, the code
  length and time step, and your sort order.
- **Settings** (`settings.conf`): currently only whether the
  confirmation sound is on.
- **Diagnostic log** (`sailauth.log`): timestamped error and debug
  messages written when something goes wrong (failed queries, import
  errors, UI errors). It contains no secret keys and no codes.
- **Backups you create**: only if and where you export them yourself —
  see below.

Deleting an account in the app deletes it from the database. Nothing
listed here ever leaves the device.

## What the app does not do

- **No internet access.** The app is sandboxed without the Sailjail
  `Internet` permission and contains no network code. It cannot
  transmit data.
- **No analytics, telemetry, crash reporting, advertising or
  tracking**, in any form.
- **No third-party SDKs, libraries or services.**
- **No accounts, no login, no cloud.** There is no operator profile to
  build: the developer receives no account data, codes, device
  identifiers or usage statistics — there is no channel by which the
  developer could receive them.
- **No time servers.** Codes are computed from your device clock
  (RFC 6238); the time is never fetched online.
- The only links in the app (source code, donation page) open in your
  web browser when you tap them. The app itself never makes a request.

## Permissions and why they exist

| Permission | Used for |
|---|---|
| `Camera` | Scanning QR codes when you add an account. Frames are decoded **on the device** and are neither stored nor transmitted. |
| `Audio` | The short confirmation beep when you copy a code (optional, configurable in Settings). |
| `UserDirs` | Reading backup files you select and writing backups you export, through the system file picker. Only files you explicitly choose are touched. |

No `Internet` permission is requested, granted, or used.

## The clipboard

Tapping a code copies it to the system clipboard so you can paste it
where needed (with the optional beep). The clipboard is part of the
operating system and may be readable by other applications until it is
overwritten. This only happens when you tap a code.

## Backups and exports

SailAuth offers two export formats, both written as ordinary files that
you save yourself (device storage, computer, and so on):

- **Encrypted backup** (AES-256-GCM): readable only with the password
  you choose. If you lose the password it cannot be recovered.
- **Plain-text `otpauth://` export**: contains your secret keys in
  readable form so other authenticator apps can import them. Treat such
  a file like a password — anyone who obtains it can generate your
  codes.

How you store, sync or share export files is your choice and outside
the app's control.

## Your rights

Since the app and its developer process and hold no personal data —
everything is on your device, under your control — there is no stored
information to access, correct, port or delete on any server. Your data
is where you can see it: in the app, and in exports you create.

## Children

The app is not directed at children and collects no data from anyone,
children included — it collects no data at all.

## Changes to this policy

This policy lives in the project repository
(<https://github.com/kapitaali/harbour-sailauth>) and changes are
committed alongside the code; the date at the top reflects the current
version.

## Contact

Questions? Open an issue at
<https://github.com/kapitaali/harbour-sailauth/issues>.
