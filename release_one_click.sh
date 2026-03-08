#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"

PACKAGER="$REPO_ROOT/packager/packager"
BUILDER="$REPO_ROOT/builder/bin/builder"

usage() {
  cat <<'EOF'
Usage:
  ./release_one_click.sh <release> <mode> [options]

Modes:
  web        -> packager target web
  apk        -> packager target android
  android    -> packager target android
  aab        -> packager target aab
  all        -> packager target all (web + apk)

Options:
  --build-runner     Build runner artifacts before packaging (default: ON)
  --no-build-runner  Skip runner build (use existing binaries)
  --compile-bc       Force compile scripts/main.bu -> assets/main.buc (default: ON)
  --no-compile-bc    Disable forced bytecode compile
  --zip-web          Create export/<release>/<release>_web.zip after web/all (default: ON)
  --no-zip-web       Disable web zip creation
  --install          Install APK after packaging (android/apk/all)
  --run              Run app after install (android/apk/all)
  --adb <path>       Explicit adb path (forwarded to packager)
  --device <serial>  ADB serial (forwarded to packager)
  -h, --help         Show this help

Examples:
  ./release_one_click.sh piano web
  ./release_one_click.sh piano aab --build-runner
  ./release_one_click.sh piano apk --install --run
EOF
}

if [[ $# -lt 2 ]]; then
  usage
  exit 1
fi

RELEASE="$1"
MODE_RAW="$2"
shift 2

BUILD_RUNNER=1
COMPILE_BC=1
ZIP_WEB=1
FORWARD_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-runner)
      BUILD_RUNNER=1
      shift
      ;;
    --no-build-runner)
      BUILD_RUNNER=0
      shift
      ;;
    --compile-bc)
      COMPILE_BC=1
      shift
      ;;
    --no-compile-bc)
      COMPILE_BC=0
      shift
      ;;
    --zip-web)
      ZIP_WEB=1
      shift
      ;;
    --no-zip-web)
      ZIP_WEB=0
      shift
      ;;
    --install|--run)
      FORWARD_ARGS+=("$1")
      shift
      ;;
    --adb|--device)
      if [[ $# -lt 2 ]]; then
        echo "[one-click][error] Missing value for $1" >&2
        exit 1
      fi
      FORWARD_ARGS+=("$1" "$2")
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[one-click][error] Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

case "${MODE_RAW,,}" in
  web) TARGET="web" ;;
  apk|android) TARGET="android" ;;
  aab) TARGET="aab" ;;
  all) TARGET="all" ;;
  *)
    echo "[one-click][error] Unknown mode: $MODE_RAW" >&2
    usage
    exit 1
    ;;
esac

RELEASE_DIR="$REPO_ROOT/releases/$RELEASE"
RELEASE_JSON="$REPO_ROOT/releases/$RELEASE.json"
RUNNER_RELEASE_JSON="$REPO_ROOT/projects/runner/releases/$RELEASE.json"

if [[ ! -d "$RELEASE_DIR" ]]; then
  echo "[one-click][error] Release folder not found: $RELEASE_DIR" >&2
  exit 1
fi

if [[ ! -f "$RELEASE_JSON" ]]; then
  echo "[one-click][error] Release JSON not found: $RELEASE_JSON" >&2
  exit 1
fi

if [[ ! -x "$PACKAGER" ]]; then
  echo "[one-click][error] packager not found/executable: $PACKAGER" >&2
  echo "Build it first: make -C \"$REPO_ROOT/packager\"" >&2
  exit 1
fi

if [[ $BUILD_RUNNER -eq 1 ]]; then
  if [[ ! -x "$BUILDER" ]]; then
    echo "[one-click][error] builder not found/executable: $BUILDER" >&2
    echo "Build it first: make -C \"$REPO_ROOT/builder\" release" >&2
    exit 1
  fi

  RUNNER_RELEASE_ARGS=()
  if [[ -f "$RUNNER_RELEASE_JSON" ]]; then
    RUNNER_RELEASE_ARGS=(--release "$RUNNER_RELEASE_JSON")
  else
    echo "[one-click][warn] Runner release profile not found: $RUNNER_RELEASE_JSON" >&2
    echo "[one-click][warn] Falling back to runner default profile." >&2
  fi

  if [[ "$TARGET" == "web" || "$TARGET" == "all" ]]; then
    echo "[one-click] building runner web..."
    "$BUILDER" build "$REPO_ROOT/projects/runner" web --build-modules "${RUNNER_RELEASE_ARGS[@]}"
  fi
  if [[ "$TARGET" == "android" || "$TARGET" == "aab" || "$TARGET" == "all" ]]; then
    echo "[one-click] building runner android..."
    "$BUILDER" build "$REPO_ROOT/projects/runner" android --build-modules --abis arm7,arm64 "${RUNNER_RELEASE_ARGS[@]}"
  fi
fi

if [[ "$TARGET" == "aab" ]]; then
  if [[ -z "${BUNDLETOOL_JAR:-}" && ! -f "$REPO_ROOT/tools/android/bundletool.jar" ]]; then
    echo "[one-click][error] Missing bundletool.jar for AAB." >&2
    echo "Set BUNDLETOOL_JAR or place it at: $REPO_ROOT/tools/android/bundletool.jar" >&2
    exit 1
  fi
fi

CMD=(
  "$PACKAGER" "$RELEASE" "$TARGET"
  "--repo" "$REPO_ROOT"
  "--bugame" "$REPO_ROOT"
  "--runner" "$REPO_ROOT/projects/runner"
)

if [[ $COMPILE_BC -eq 1 ]]; then
  CMD+=("--compile-bc")
fi
CMD+=("${FORWARD_ARGS[@]}")

echo "[one-click] release=$RELEASE mode=$MODE_RAW target=$TARGET"
echo "[one-click] running: ${CMD[*]}"
"${CMD[@]}"

if [[ $ZIP_WEB -eq 1 && ( "$TARGET" == "web" || "$TARGET" == "all" ) ]]; then
  WEB_EXPORT_DIR="$REPO_ROOT/export/$RELEASE/web"
  WEB_ZIP="$REPO_ROOT/export/$RELEASE/${RELEASE}_web.zip"

  if [[ -d "$WEB_EXPORT_DIR" ]]; then
    if command -v zip >/dev/null 2>&1; then
      echo "[one-click] creating web zip: $WEB_ZIP"
      rm -f "$WEB_ZIP"
      (
        cd "$WEB_EXPORT_DIR"
        zip -r "$WEB_ZIP" . >/dev/null
      )
    else
      echo "[one-click][warn] zip command not found, skipping web zip." >&2
    fi
  else
    echo "[one-click][warn] Web export folder not found, skipping zip: $WEB_EXPORT_DIR" >&2
  fi
fi

echo "[one-click] done."
