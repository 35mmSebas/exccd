#!/bin/sh
# Prueft, ob sich die Firmware einer Kamera gegenueber einem Git-Stand aendert.
#
#   tools/filmsim/compare_build.sh <git-ref> [plattform ...]
#   tools/filmsim/compare_build.sh v1.9.22 ixus60_sd600
#
# Baut <git-ref> in einem temporaeren Worktree und den aktuellen Stand
# (inklusive nicht committeter Aenderungen), beide mit derselben
# Versionsnummer und demselben Build-Zeitstempel (SOURCE_DATE_EPOCH).
# Vergleicht danach main.bin und jede Objektdatei der Kamera.
# Gedacht fuer Aenderungen am Kern, die eine andere Kamera nicht
# beruehren sollen: Ergebnis "identisch" heisst bytegleiche Firmware.
# Ohne Plattform: alle Kameras aus cameras.txt.
# Vorher: . ../../toolchain/env.sh

set -e
cd "$(dirname "$0")/../.."
[ $# -ge 1 ] || { sed -n 2,4p "$0"; exit 1; }
ref=$1; shift

CAMERAS=$(awk '!/^#/ && NF { print $1 ":" $2 }' tools/filmsim/cameras.txt)
[ $# -gt 0 ] && CAMERAS=$(for p in "$@"; do echo "$CAMERAS" | tr ' ' '\n' | grep "^$p:"; done)
[ -n "$CAMERAS" ] || { echo "Unbekannte Kamera: $*" >&2; exit 1; }

export SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
version=$(sed -n 's/^#define FILMSIM_VERSION "\(.*\)"/\1/p' core/filmsim.h)
wt=$(mktemp -d)
cleanup() { git worktree remove --force "$wt" 2>/dev/null || rm -rf "$wt"; git worktree prune; }
trap cleanup EXIT
git worktree add -q --detach "$wt" "$ref"
sed -i "s/^#define FILMSIM_VERSION .*/#define FILMSIM_VERSION \"$version\"/" "$wt/core/filmsim.h"
mkdir -p "$wt/bin" bin

dump() { arm-none-eabi-objdump -s -d "$1" 2>/dev/null | tail -n +3; }
result=0

for cam in $CAMERAS; do
    p=${cam%:*} s=${cam#*:}
    echo "=== $p $s: $ref gegen aktuellen Stand"
    for dir in "$wt" .; do
        (cd "$dir" && make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null &&
            make -s PLATFORM=$p PLATFORMSUB=$s firzipsubcomplete >build-compare.log 2>&1) ||
            { tail -20 "$dir/build-compare.log"; echo "Build in $dir fehlgeschlagen" >&2; exit 1; }
        rm -f "$dir/build-compare.log"
    done
    bad=0
    sub=platform/$p/sub/$s
    for f in $(cd "$wt" && find core lib modules platform/$p -name '*.o' -type f | sort); do
        if [ ! -f "$f" ]; then echo "    nur in $ref: $f"; bad=$((bad + 1)); continue; fi
        dump "$wt/$f" > "$wt/.a"; dump "$f" > "$wt/.b"
        cmp -s "$wt/.a" "$wt/.b" || { echo "    anders: $f"; bad=$((bad + 1)); }
    done
    if cmp -s "$wt/$sub/main.bin" "$sub/main.bin"; then
        echo "    main.bin identisch"
    else
        echo "    main.bin ANDERS"; bad=$((bad + 1))
    fi
    [ $bad -eq 0 ] && echo "    -> identisch" || { echo "    -> $bad Unterschiede"; result=1; }
    make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null
done
exit $result
