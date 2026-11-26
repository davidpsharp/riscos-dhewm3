#!/bin/sh
# RISC OS port of dhewm3. Copyright (C) 2026 David Sharp. GPL-3.0 (see dhewm3/COPYING.txt).
# Makes the release zip, build/dhewm3-riscos-<version>.zip, on the Mac, after
#   riscos/sdk.sh riscos/build.sh   and   riscos/sdk.sh riscos/package.sh
# It's a RISC OS zip: file types travel in each entry's ARC0 extra field (tools/riscos_zip.py),
# so SparkFS, !SparkPlug or Info-ZIP's UnZip on RISC OS restore them. The name is
# <App>-riscos-<version>.zip, as packman/publish.py needs. Contents: the one !dhewm3 at the top.
set -e
cd "$(dirname "$0")/.."
V=$(cat VERSION)
OUT=build/dhewm3-riscos-$V.zip
M=build/release-manifest.txt
APP=build/riscos/app/!dhewm3
test -f "$APP/dhewm3,ff8" || { echo "no $APP: run riscos/package.sh first" >&2; exit 1; }

: > "$M"
# name,xxx -> "xxx  zip/name  source"; names with no suffix are data (ffd)
add() {   # add <source> <name in the zip>
    case "$1" in
    *,???) t=${1##*,}; n=${2%,???} ;;
    *) t=ffd; n=$2 ;;
    esac
    printf '%s  %s  %s\n' "$t" "$n" "$1" >> "$M"
}
find "$APP" -type f | sort | while read -r f; do
    add "$f" "!dhewm3/${f#$APP/}"
done
python3 -I tools/riscos_zip.py "$OUT" "$M"
rm -f "$M"
ls -l "$OUT"
