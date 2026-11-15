#!/bin/sh
# EXCCD lokal bauen, jede Kamera und jede ihrer Firmwares fuer sich.
#
#   tools/filmsim/build.sh                 alle Kameras aus cameras.txt
#   tools/filmsim/build.sh ixus145_elph135 nur eine (alle ihre Firmwares)
#
#   ONLY_MISSING=1  Firmwares ueberspringen, deren ZIP schon in dist/<version>/ liegt
#   KEEP_GOING=1    nach einem Fehler weiterbauen, am Ende alle Fehler auflisten
#
# Die Objektdateien in core/, lib/ und modules/ teilen sich alle Kameras.
# Darum vor und nach jeder Firmware "clean": sonst landet z. B. ein 12-Bit-
# filmsim.o der IXUS 145 in der Firmware der IXUS 60.
# Firmwares, die in camera_list.csv eine SOURCE_FIRMWARE haben (Alias), sind
# bei CHDK eine Kopie dieses Builds (firzipsubcompletecopy); hier genauso.
# Ergebnis: dist/<version>/exccd-<version>-<plattform>-<sub>-full.zip, daneben
# die Build-Logs. Jede Version hat ihren eigenen Ordner, gleiche Version = ueberschreiben.
# Vorher: . ../../toolchain/env.sh

set -e
cd "$(dirname "$0")/../.."

# Kameraliste: tools/filmsim/cameras.txt (Plattform, Firmwares mit Komma, Status, Name)
CAMERAS=$(awk '!/^#/ && NF { print $1 ":" $2 }' tools/filmsim/cameras.txt)
[ $# -gt 0 ] && CAMERAS=$(for p in "$@"; do echo "$CAMERAS" | tr ' ' '\n' | grep "^$p:"; done)
[ -n "$CAMERAS" ] || { echo "Unbekannte Kamera: $*" >&2; exit 1; }

version=$(sed -n 's/^#define FILMSIM_VERSION "\(.*\)"/\1/p' core/filmsim.h)
out=dist/$version
mkdir -p bin $out
failed=""

# SOURCE_FIRMWARE aus camera_list.csv, leer = eigener Build
source_fw() {
    awk -F, -v p="$1" -v s="$2" '$1 == p && $2 == s { print $4 }' camera_list.csv
}

fail() {
    echo "    Fehler bei $1, Log: $2" >&2
    if [ -n "$KEEP_GOING" ]; then
        failed="$failed $1"
        make -s PLATFORM=${1%-*} PLATFORMSUB=${1#*-} clean >/dev/null 2>&1 || true
        return 0
    fi
    tail -20 $2
    exit 1
}

for cam in $CAMERAS; do
    p=${cam%%:*} subs=$(echo ${cam#*:} | tr , ' ')
    probe_done=""
    # erst die eigenen Builds, dann die Kopien
    for pass in own alias; do
        for s in $subs; do
            src=$(source_fw $p $s)
            if [ $pass = own ] && [ -n "$src" ]; then continue; fi
            if [ $pass = alias ] && [ -z "$src" ]; then continue; fi
            zipname=$out/exccd-$version-$p-$s-full.zip
            if [ -n "$ONLY_MISSING" ] && [ -f $zipname ] && [ -f tools/installer/probes/FWPROBE-$p.BIN ]; then
                echo "=== $p $s schon gebaut"
                probe_done=1
                continue
            fi
            if [ -n "$src" ]; then
                if [ -f $out/exccd-$version-$p-$src-full.zip ]; then
                    cp $out/exccd-$version-$p-$src-full.zip $zipname
                    echo "=== $p $s = Kopie von $src -> $zipname"
                else
                    fail $p-$s "(Quelle $src nicht gebaut)"
                fi
                continue
            fi
            echo "=== $p $s ($version)"
            # leerer Platzhalter: der Build sucht sonst einen Firmware-Dump (.gitignore)
            [ -e platform/$p/sub/$s/PRIMARY.BIN ] || touch platform/$p/sub/$s/PRIMARY.BIN
            make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null
            rm -f bin/$p-$s-*.zip
            log=$out/build-$p-$s.log
            if ! make -s PLATFORM=$p PLATFORMSUB=$s firzipsubcomplete >$log 2>&1; then
                fail $p-$s $log
                continue
            fi
            # CHDK im ARAM (SX110): Kern samt bss muss in ARAM_HEAP_SIZE passen. Was uebrig
            # bleibt, ist der ARAM-Heap; unter 4 KB legt core/memmgmt.c keinen an.
            mk=platform/$p/sub/$s/makefile.inc
            if grep -q '^override OPT_CHDK_IN_ARAM=1' $mk; then
                lim=$(sed -n 's/^ARAM_HEAP_SIZE[ \t]*=[ \t]*\(0x[0-9a-fA-F]*\).*/\1/p' $mk)
                if [ -z "$lim" ]; then
                    echo "    ARAM_HEAP_SIZE nicht in $mk gefunden" >&2
                    fail $p-$s $log
                    continue
                fi
                used=$(arm-none-eabi-size platform/$p/sub/$s/main.elf | awk 'NR==2 { print $4 }')
                free=$(( lim - used ))
                echo "    ARAM: $used von $((lim)) Byte belegt, $free frei"
                if [ $free -lt 0 ]; then
                    echo "    CHDK passt nicht ins ARAM ($p $s), $(( -free )) Byte zu gross" >&2
                    fail $p-$s $log
                    continue
                fi
                [ $free -ge 4096 ] || echo "    Hinweis: unter 4 KB frei, kein ARAM-Heap (malloc nutzt Canon-Speicher)"
            fi
            cp bin/$p-$s-*-full.zip $zipname
            # Firmware-Pruefprogramm fuer den Installer (tools/installer/probes/), eins je Kamera:
            # es blinkt die Version, die es im ROM findet, gilt also fuer alle Firmwares.
            # Ohne Pruefprogramm zeigt der Installer statt der Blink-Tabelle die vers.req-Anleitung,
            # deshalb ist ein Fehler hier nur eine Warnung (manche CHDK-Loader bauen es nicht).
            probe=""
            if [ -z "$probe_done" ]; then
                probe_done=1
                probe="tools/installer/probes/FWPROBE-$p.BIN"
                if make -s PLATFORM=$p PLATFORMSUB=$s fwprobe >>$log 2>&1; then
                    mkdir -p tools/installer/probes
                    cp bin/FWPROBE-$p.BIN tools/installer/probes/
                else
                    echo "    Warnung: fwprobe fuer $p nicht gebaut, siehe $log" >&2
                    probe="(kein Pruefprogramm)"
                    make -s -C loader clean >/dev/null 2>&1
                fi
                probe=", $probe"
            fi
            make -s PLATFORM=$p PLATFORMSUB=$s clean >/dev/null
            echo "    -> $zipname$probe"
        done
    done
done

if [ -n "$failed" ]; then
    echo "Nicht gebaut:$failed" >&2
    exit 1
fi
