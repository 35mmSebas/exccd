#!/bin/sh
# EXCCD lokal bauen, jede Kamera fuer sich.
#
#   tools/filmsim/build.sh                 alle Kameras aus cameras.txt
#   tools/filmsim/build.sh ixus145_elph135 nur eine
#
# Die Objektdateien in core/, lib/ und modules/ teilen sich alle Kameras.
# Darum vor und nach jeder Kamera "clean": sonst landet z. B. ein 12-Bit-
# filmsim.o der IXUS 145 in der Firmware der IXUS 60.
# Ergebnis: dist/exccd-<version>-<plattform>-<sub>-full.zip (wie in der CI).
# Vorher: . ../../toolchain/env.sh

set -e
cd "$(dirname "$0")/../.."

# Kameraliste: tools/filmsim/cameras.txt (Plattform, Sub, Status, Name)
CAMERAS=$(awk '!/^#/ && NF { print $1 ":" $2 }' tools/filmsim/cameras.txt)
[ $# -gt 0 ] && CAMERAS=$(for p in "$@"; do echo "$CAMERAS" | tr ' ' '\n' | grep "^$p:"; done)
[ -n "$CAMERAS" ] || { echo "Unbekannte Kamera: $*" >&2; exit 1; }

version=$(sed -n 's/^#define FILMSIM_VERSION "\(.*\)"/\1/p' core/filmsim.h)
mkdir -p bin dist

for cam in $CAMERAS; do
    p=${cam%:*} s=${cam#*:}
    echo "=== $p $s ($version)"
    make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null
    rm -f bin/$p-$s-*.zip
    log=dist/build-$p-$s.log
    make -s PLATFORM=$p PLATFORMSUB=$s firzipsubcomplete >$log 2>&1 ||
        { tail -20 $log; echo "Fehler, ganzes Log: $log" >&2; exit 1; }
    cp bin/$p-$s-*-full.zip dist/exccd-$version-$p-$s-full.zip
    make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null
    echo "    -> dist/exccd-$version-$p-$s-full.zip"
done
