# Harbour SailAuth — Native TOTP Authenticator for Sailfish OS

## App Name

Harbour SailAuth (or "SailAuth")

## Core Concept

A native Sailfish OS TOTP (RFC 6238) authenticator with a clean Silica UI, QR code scanning, and local-first storage. No cloud, no tracking, no Google dependencies.

---

## Feature Set (MVP)

1. **TOTP code generation** — RFC 6238 compliant, 30-second window, SHA1/SHA256/SHA512 support, 6 or 8 digit codes
2. **Add accounts** — Manual entry (secret key, issuer, account name) + QR code scanning via camera
3. **Account list** — Clean list view showing issuer, account name, current TOTP code with countdown ring/bar
4. **Copy to clipboard** — Tap a code to copy it
5. **Edit/Delete accounts** — Long-press or pulley menu for management
6. **Local storage** — SQLite or JSON file in app data directory, no cloud sync
7. **Search/filter** — Filter accounts by name as you type
8. **Cover page** — Show current code for a selected account on the app cover (Sailfish OS feature)

## Feature Set (v2 — monetization)

9. **Encrypted backup/restore** — Export encrypted backup file (password-protected), import on another device
10. **Biometric unlock** — Require fingerprint/PIN to open the app (using Sailfish Secrets)
11. **Widget** — Home screen widget showing a selected account's code
12. **Import from other authenticators** — Scan QR codes from Google Authenticator, Authy, etc.
13. **Dark/light theme** — Follow system theme or manual toggle
14. **Categories/tags** — Group accounts (Work, Personal, Finance, etc.)

---

## Technical Architecture

- **Language:** QML + C++ (standard Sailfish OS app pattern)
- **TOTP logic:** C++ backend using Qt's QCryptographicHash for HMAC-SHA1/256/512
- **QR scanning:** Qt Multimedia camera + ZXing or custom QR decoder (or use an existing Sailfish OS QR scanning library)
- **Storage:** SQLite via QtSql, or simple JSON file with QSettings
- **UI:** Sailfish Silica components (Page, ListItem, Cover, PulleyMenu, etc.)
- **Build system:** RPM packaging for Harbour/OpenRepos/Chum

---

## UI/UX Design

- **Main page:** Scrollable list of accounts, each showing issuer name, account name, large TOTP code, and a circular countdown indicator
- **Add page:** Two tabs — "Scan QR" (camera view) and "Manual" (form fields)
- **Edit page:** Pre-filled form for editing account details
- **Cover:** Shows selected account's code with countdown, tap to open app
- **Pull-up menu:** Add account, Settings, About, Export backup

---

## Monetization Strategy

- **Tipping/donations only** — Harbour does not allow paid apps; the app must be free with a tip jar
- **All features free** — No feature gating; everyone gets the full app
- **Tip prompts** — Subtle "Tip the developer" link in the About page and pull-up menu, linking to a payment page (e.g. PayPal, Ko-fi, or OpenCollective)
- **Distribution:** Harbour (Jolla Store — primary), OpenRepos, Chum

---

## Development Phases

| Phase | Scope | Timeline |
|-------|-------|----------|
| Phase 1 | TOTP core logic + manual entry + list view + copy | 1-2 weeks |
| Phase 2 | QR scanning + edit/delete + search + cover | 1 week |
| Phase 3 | Encrypted backup/restore + biometric unlock | 1 week |
| Phase 4 | Widget + categories + polish + packaging | 1 week |

---

## Risks & Challenges

- QR scanning on Sailfish OS can be finicky — camera access and decoding need testing
- Sailfish Secrets API for biometric unlock is relatively new and may have quirks
- Getting into Harbour (Jolla Store) is difficult; OpenRepos is the realistic path
- Small market size — but proven willingness to pay for quality apps

---

## Competitive Landscape

- No native TOTP authenticator currently exists on Sailfish OS
- Android apps (Google Authenticator, Aegis, andOTP) don't run well on AppSupport
- Web-based TOTP exists but is inconvenient
- This fills a genuine gap in the ecosystem
