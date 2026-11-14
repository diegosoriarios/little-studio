# macOS signing and notarization

This is the deferred half of macOS release engineering. The build script
(`build-dmg.sh`) already produces a working unsigned `.dmg`; without an Apple
Developer ID and a notarization keychain profile, macOS Gatekeeper will warn on
first launch. Complete these steps **on a Mac** before publishing a build that
claims to be signed.

## 1. Prerequisites

- Apple Developer Program membership.
- A **Developer ID Application** certificate imported into the login keychain.
- Xcode command line tools (`xcrun`) and an App Store Connect API key or an
  Apple ID app-specific password for `notarytool`.

Verify the identity:

```bash
security find-identity -v -p codesigning
```

## 2. Store notarytool credentials once

```bash
xcrun notarytool store-credentials lite-studio \
  --apple-id "you@example.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"
# or, with an App Store Connect API key:
# xcrun notarytool store-credentials lite-studio \
#   --key path/to/AuthKey_XXX.p8 --key-id XXX --issuer UUID
```

The profile name defaults to `lite-studio`; override with `NOTARY_PROFILE`.

## 3. Build, sign, notarize, staple

```bash
CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
NOTARIZE=1 \
packaging/macos/build-dmg.sh dist
```

The script signs the embedded frameworks, dylibs and the `.app` with the
hardened runtime and a secure timestamp, submits the `.dmg` with
`notarytool --wait`, and staples the ticket to both the `.app` and the `.dmg`.

## 4. Verify

```bash
spctl --assess --type execute --verbose=4 build-macos/lite-studio.app
spctl --assess --type open --context context:primary-signature --verbose=4 dist/*.dmg
codesign --verify --deep --strict --verbose=2 build-macos/lite-studio.app
xcrun stapler validate dist/*.dmg
```

## CI secrets (when signing is enabled in the release workflow)

| Secret | Purpose |
| --- | --- |
| `MACOS_CERTIFICATE` | base64 of the exported `.p12` Developer ID certificate |
| `MACOS_CERTIFICATE_PASSWORD` | password for that `.p12` |
| `KEYCHAIN_PASSWORD` | temporary keychain password used in CI |
| `NOTARY_APPLE_ID` / `NOTARY_TEAM_ID` / `NOTARY_PASSWORD` | notarytool credentials |

The release workflow currently builds an **unsigned** DMG. Add a signing step
that imports the certificate into a temporary keychain, then pass
`CODESIGN_IDENTITY` and `NOTARIZE=1` to `build-dmg.sh`.
