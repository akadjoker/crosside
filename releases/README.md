# Releases (Runner Packager)

Each release is defined by `releases/<name>.json`.

For full practical workflow, see `releases/STEPS.md`.

Minimum example:

```json
{
  "Android": {
    "PACKAGE": "com.yourstudio.yourgame",
    "LABEL": "Your Game",
    "CONTENT_ROOT": "releases/yourgame"
  },
  "Web": {
    "CONTENT_ROOT": "releases/yourgame"
  }
}
```

## Android signing

Packager supports signing from release JSON.

You can define keys directly in `Android`:

```json
{
  "Android": {
    "KEYSTORE": "releases/yourgame/keys/release.jks",
    "KEY_ALIAS": "release",
    "KEYSTORE_PASS_ENV": "BU_KS_PASS",
    "KEY_PASS_ENV": "BU_KEY_PASS"
  }
}
```

Or inside a nested object:

```json
{
  "Android": {
    "SIGNING": {
      "KEYSTORE": "releases/yourgame/keys/release.jks",
      "KEY_ALIAS": "release",
      "KEYSTORE_PASS_ENV": "BU_KS_PASS",
      "KEY_PASS_ENV": "BU_KEY_PASS"
    }
  }
}
```

Supported signing fields:
- `KEYSTORE`
- `KEY_ALIAS`
- `KEYSTORE_PASS`
- `KEYSTORE_PASS_ENV`
- `KEY_PASS`
- `KEY_PASS_ENV`

If signing fields are missing, packager falls back to runner defaults.

## Build commands

Web:

```bash
packager/packager yourgame web --repo /path/to/crosside --bugame /path/to/crosside --runner /path/to/crosside/projects/runner
```

Android:

```bash
packager/packager yourgame android --repo /path/to/crosside --bugame /path/to/crosside --runner /path/to/crosside/projects/runner
```

Android App Bundle (`.aab`, Google Play):

```bash
packager/packager yourgame aab --repo /path/to/crosside --bugame /path/to/crosside --runner /path/to/crosside/projects/runner
```

Both targets:

```bash
packager/packager yourgame all --repo /path/to/crosside --bugame /path/to/crosside --runner /path/to/crosside/projects/runner
```

## bundletool

`aab` target needs `bundletool.jar`.

Option 1:
- set env: `BUNDLETOOL_JAR=/full/path/bundletool.jar`

Option 2:
- place file at: `tools/android/bundletool.jar`

Example install:

```bash
mkdir -p tools/android
curl -L --fail -o tools/android/bundletool-all-1.18.3.jar \
  https://github.com/google/bundletool/releases/download/1.18.3/bundletool-all-1.18.3.jar
ln -sf bundletool-all-1.18.3.jar tools/android/bundletool.jar
```
