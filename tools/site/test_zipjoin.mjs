// CI-Test fuer site/zipjoin.js: setzt aus einer gebauten Seite zwei Download-ZIPs
// zusammen wie der Browser (alle Looks, erste zwei Looks) und prueft den Inhalt.
//
//   node tools/site/test_zipjoin.mjs <gebaute Seite> <ausgabe>
//
// Danach: unzip -t <ausgabe>/*.zip und test_films_txt <ausgabe>/FILMS-all.TXT

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { assemble, readZip, filmsTxt } from "../../site/zipjoin.js";

const [siteDir, outDir] = process.argv.slice(2);
const site = JSON.parse(readFileSync(join(siteDir, "data/site.json"), "utf8"));
mkdirSync(outDir, { recursive: true });

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FEHLER: " + msg); } };

// Je Kamera und Firmware die Download-ZIP mit allen Looks, fuer die Hauptfirmware auch
// die mit zwei Looks. Namen wie im Browser (exccd-<version>-<plattform>-<sub>-...), daran
// erkennt der Installer (test_core) Kamera und Firmware. FILMS-*.TXT der ersten Kamera
// ohne Praefix, fuer test_films_txt in der CI.
site.cameras.forEach((cam, k) => {
  const pre = k === 0 ? "" : `${cam.id}-`;
  check(cam.firmwares.length > 0, `${cam.id}: keine Firmware`);
  cam.firmwares.forEach((fw, i) => {
    const base = readFileSync(join(siteDir, "data", fw.dir, "base.zip"));
    const sets = i === 0 ? [["all", cam.looks], ["two", cam.looks.slice(0, 2)]] : [["all", cam.looks]];
    for (const [tag, looks] of sets) {
      const zips = looks.map((l) => readFileSync(join(siteDir, "data", cam.id, "looks", l.id + ".zip")));
      const zip = assemble(base, zips, looks, site.version);
      const id = `${cam.id} ${fw.version} ${tag}`;
      writeFileSync(join(outDir, `exccd-${site.version}-${cam.id}-${fw.sub}-${tag}.zip`), zip);
      if (i === 0) writeFileSync(join(outDir, `${pre}FILMS-${tag}.TXT`), filmsTxt(looks, site.version));

      const names = new Set(readZip(zip).map((e) => e.name));
      for (const must of ["DISKBOOT.BIN", "CHDK/FILMS/FILMS.TXT", ...(cam.firm_update ? ["PS.FIR"] : [])]) check(names.has(must), `${id}: ${must} fehlt`);
      check(!names.has("CHDK/FILMS/EXAMPLE.TXT"), `${id}: EXAMPLE.TXT gehoert nicht in die Download-ZIP`);
      for (const l of cam.looks) {
        const want = looks.includes(l);
        check(names.has(`CHDK/FILMS/${l.id}.FLM`) === want, `${id}: ${l.id}.FLM ${want ? "fehlt" : "zu viel"}`);
        if (l.curve) {
          const stem = l.curve.replace(/\.CV$/, "");
          for (const s of ["", "1", "2"])
            check(names.has(`CHDK/CURVES/${stem}${s}.CV`) === want, `${id}: ${stem}${s}.CV ${want ? "fehlt" : "zu viel"}`);
        }
      }
      console.log(`${id}: ${looks.length} Looks, ${names.size} Eintraege, ${zip.length} Bytes`);
    }
  });
});

console.log(fails ? `FEHLGESCHLAGEN (${fails} Fehler)` : "OK");
process.exit(fails ? 1 : 0);
