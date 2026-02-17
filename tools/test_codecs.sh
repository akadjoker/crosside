#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILDER="$ROOT_DIR/builder/bin/builder"

if [[ ! -x "$BUILDER" ]]; then
    echo "[error] builder not found at $BUILDER"
    echo "Build it first: make -C \"$ROOT_DIR/builder\" -j4"
    exit 1
fi

TARGET="${1:-desktop}"
if [[ "$TARGET" != "desktop" && "$TARGET" != "android" ]]; then
    echo "Usage: $0 [desktop|android]"
    exit 1
fi

MODULES=(zlib jpeg png)
WEBP_MODULE_FILE="$ROOT_DIR/modules/webp/module.json"
if [[ -f "$WEBP_MODULE_FILE" ]]; then
    MODULES+=(webp)
    HAS_WEBP=1
else
    HAS_WEBP=0
fi

echo "[info] target: $TARGET"
echo "[info] modules: ${MODULES[*]}"

for module in "${MODULES[@]}"; do
    echo "[info] building module $module ($TARGET)"
    "$BUILDER" build module "$module" "$TARGET" --full
done

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/codec_smoke.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

cat > "$TMP_DIR/main.c" <<'EOF'
#include <stdio.h>
#include <zlib.h>
#include "jpeglib.h"
#include "png.h"
#ifdef HAS_WEBP
#include <webp/decode.h>
#endif

int main(void)
{
    printf("zlib: %s\n", zlibVersion());

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "png_create_read_struct failed\n");
        return 2;
    }
    png_destroy_read_struct(&png_ptr, NULL, NULL);

#ifdef HAS_WEBP
    printf("webp decoder version: %d\n", WebPGetDecoderVersion());
#endif

    puts("codec smoke ok");
    return 0;
}
EOF

MODULE_JSON='"zlib", "jpeg", "png"'
MAIN_CC='[]'
if [[ "$HAS_WEBP" -eq 1 ]]; then
    MODULE_JSON='"zlib", "jpeg", "png", "webp"'
    MAIN_CC='["-DHAS_WEBP"]'
fi

cat > "$TMP_DIR/main.mk" <<EOF
{
  "Path": "$TMP_DIR",
  "Name": "codec_smoke",
  "Modules": [ $MODULE_JSON ],
  "Src": ["main.c"],
  "Include": [],
  "Main": {
    "CPP": [],
    "CC": $MAIN_CC,
    "LD": []
  },
  "Desktop": {
    "CPP": [],
    "CC": [],
    "LD": []
  },
  "Android": {
    "PACKAGE": "com.djokersoft.codecsmoke",
    "ACTIVITY": "MainActivity",
    "LABEL": "CodecSmoke",
    "CPP": [],
    "CC": [],
    "LD": []
  },
  "Web": {
    "CPP": [],
    "CC": [],
    "LD": []
  }
}
EOF

echo "[info] building smoke app ($TARGET)"
"$BUILDER" build app codec_smoke "$TARGET" --project-file "$TMP_DIR/main.mk" --full

if [[ "$TARGET" == "desktop" ]]; then
    echo "[info] running smoke app"
    "$TMP_DIR/codec_smoke"
fi

echo "[ok] codec smoke test passed for $TARGET"
