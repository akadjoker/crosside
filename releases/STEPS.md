# Packager Steps (Runner -> Web/APK/AAB)

This is the practical flow to export a release with the C++ `packager`.

## 0) Paths used in this guide

```bash
cd /media/projectos/projects/cpp/crosside
```

## 1) Build runner artifacts (Web + Android)

Build once, then rebuild only when C/C++ engine/runtime changes.

```bash
# Web runner artifacts
./builder/bin/builder build runner web --build-modules

# Android runner artifacts (both ABIs)
./builder/bin/builder build runner android --build-modules --abis arm7,arm64
```

Quick check:

```bash
ls projects/runner/Web/runner.html projects/runner/Web/runner.js projects/runner/Web/runner.wasm
ls projects/runner/Android/armeabi-v7a/librunner.so projects/runner/Android/arm64-v8a/librunner.so
```

## 2) Create production keystore (Play Store)

Create once and keep backups forever.

```bash
mkdir -p releases/piano/keys

keytool -genkeypair -v \
  -keystore releases/piano/keys/release.jks \
  -storetype PKCS12 \
  -alias release \
  -keyalg RSA -keysize 4096 -validity 10000
```

## 3) Configure signing in release JSON

Example (`releases/piano.json`):

```json
{
  "Android": {
    "PACKAGE": "com.tudominio.pianoteleprompt",
    "LABEL": "Piano Teleprompt",
    "CONTENT_ROOT": "releases/piano",
    "SIGNING": {
      "KEYSTORE": "releases/piano/keys/release.jks",
      "KEY_ALIAS": "release",
      "KEYSTORE_PASS_ENV": "BU_KS_PASS",
      "KEY_PASS_ENV": "BU_KEY_PASS"
    }
  }
}
```

Set env vars before packaging:

```bash
export BU_KS_PASS='your_store_password'
export BU_KEY_PASS='your_key_password'
```

## 4) Ensure bundletool for AAB

`aab` target needs `bundletool.jar`.

```bash
mkdir -p tools/android
curl -L --fail -o tools/android/bundletool-all-1.18.3.jar \
  https://github.com/google/bundletool/releases/download/1.18.3/bundletool-all-1.18.3.jar
ln -sf bundletool-all-1.18.3.jar tools/android/bundletool.jar
```

Alternative:

```bash
export BUNDLETOOL_JAR=/full/path/to/bundletool.jar
```

## 5) Package release

### One-click helper (recommended)

```bash
./release_one_click.sh piano web
./release_one_click.sh piano apk
./release_one_click.sh piano aab

# optional: rebuild runner first
./release_one_click.sh piano aab --build-runner
```

### Web

```bash
./packager/packager piano web \
  --repo /media/projectos/projects/cpp/crosside \
  --bugame /media/projectos/projects/cpp/crosside \
  --runner /media/projectos/projects/cpp/crosside/projects/runner
```

Output:

```text
export/piano/web/index.html
```

### Android APK (local install/testing)

```bash
./packager/packager piano android \
  --repo /media/projectos/projects/cpp/crosside \
  --bugame /media/projectos/projects/cpp/crosside \
  --runner /media/projectos/projects/cpp/crosside/projects/runner
```

Output:

```text
export/piano/android/piano.signed.apk
```

### Android App Bundle AAB (Google Play)

```bash
./packager/packager piano aab \
  --repo /media/projectos/projects/cpp/crosside \
  --bugame /media/projectos/projects/cpp/crosside \
  --runner /media/projectos/projects/cpp/crosside/projects/runner
```

Output:

```text
export/piano/android/piano.signed.aab
```

## 6) Useful flags

```bash
# force recompile scripts/main.bu -> assets/main.buc
--compile-bc

# install and run generated APK (android target only)
--install --run

# explicit adb and device
--adb /path/to/adb --device SERIAL
```

## 7) Notes

- Runtime bytecode is packed as `assets/main.buc`.
- If `assets/main.buc` is missing, packager can compile from `scripts/main.bu`.
- `all` target packs `web + android` (APK). Use `aab` explicitly for Play upload.
- Current Android toolchain in this repo is set to `build-tools 35.0.1` and `platform android-35`.
