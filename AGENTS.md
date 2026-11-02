# AGENTS.md — harbour-sailauth (SailAuth)

Instructions for any agent working in this repository. Read this first, then `README.md`.

## 1. What this repo is

SailAuth is a native Sailfish OS TOTP 2FA authenticator (RFC 6238). Accounts live in a local SQLite database; supports QR scanning via the system zxing service, manual entry, import/export (otpauth URIs, andOTP/Aegis/FreeOTP JSON), encrypted AES-256-GCM backups, and a cover showing the first account's code. qmake/Sailfish, built with sfdk.

## 2. The one command that decides whether you are done

```
sfdk -c target=SailfishOS-5.1.0.11-aarch64 build
```

Builds the RPM. There is no automated test suite in this repo; verification is manual on device/emulator plus the Harbour static validator:

```
sfdk -c device= check -s harbour RPMS/SailfishOS-5.1.0.11-aarch64/harbour-sailauth-*.aarch64.rpm
```

## 3. Layout

```
src/            C++ backend (database, account model/filter, backup, importer, clipboard helper, qr filter, settings)
qml/            Silica UI pages
icons/          app icons
rpm/            Harbour spec + changelog
META-INF/       progress logs + submission dossier
PRIVACY.md      privacy policy
```

## 4. Laws. Breaking one is a bug even if the tests pass.

1. **Clean before switching targets.** `sfdk build` compiles in place; switching arch without cleaning links wrong objects. Remove generated artifacts first.
2. **Harbour rules.** `Exec=harbour-sailauth`; no `%post`/`%postun` scriptlets; `License: GPL-3.0-only` with repo-root `LICENSE`; minimal `[X-Sailjail]` permissions (`Audio;Camera;UserDirs`).
3. **No network.** SailAuth has no `Internet` permission and no network code. Do not add analytics, crash reporting, or telemetry.
4. **Local-first storage.** Accounts live in the app's own data directory (Sailjail-provisioned path via `QStandardPaths::AppDataLocation`). Do not write to `~/.local/share/harbour-sailauth` directly.
5. **No secrets in committed files.** Never record keys, tokens, or user data in docs, reports, or screenshots.

## 5. Verification culture

- Build on both aarch64 and armv7hl (and i486 for emulator); verify the binary arch with `file harbour-sailauth`.
- Run the Harbour static validator on both architecture RPMs before submission.
- Prefer measured numbers over adjectives. No marketing language.

## 6. Recording work

- Progress goes in `META-INF/PROGRESS-YYYYMMDD.md`.
- Submission dossier in `META-INF/SUBMIT.md`.

## 7. Git and secrets

- Never push. The owner pushes.
- Never commit credentials, tokens, `.env` files, or build artifacts (`RPMS/`, `*.o`, `moc_*.cpp`, `Makefile`, `.qmake.stash`).
- Commits are attributed `kapitaali <teppo@kapitaali.com>`.
- Do not stage another agent's work in progress — check `git status` and `git log --oneline -5` first.

## 8. Test commands

```
# build (phone)
sfdk -c target=SailfishOS-5.1.0.11-aarch64 build
# build (emulator)
sfdk -c target=SailfishOS-5.1.0.11-i486 build
# Harbour validator
sfdk -c device= check -s harbour RPMS/SailfishOS-5.1.0.11-aarch64/harbour-sailauth-*.aarch64.rpm
```

## 9. Current state

- Renamed from SailOTP to SailAuth (store name was taken): package, binary, desktop file, Sailjail identity, QML, icons, spec and docs all follow.
- Encrypted backup export/import implemented.
- No automated tests; verification is manual + Harbour validator.
