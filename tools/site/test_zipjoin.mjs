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

// Je Kamera; die erste (Vorauswahl der Seite) behaelt die alten Dateinamen
// fuer die Pruefungen in der CI, die weiteren bekommen ihre ID davor
site.cameras.forEach((cam, k) => {
  const data = join(siteDir, "data", cam.id);
  const base = readFileSync(join(data, "base.zip"));
  const pre = k === 0 ? "" : `${cam.id}-`;
  for (const [tag, looks] of [["all", cam.looks], ["two", cam.looks.slice(0, 2)]]) {
    const zips = looks.map((l) => readFileSync(join(data, "looks", l.id + ".zip")));
    const zip = assemble(base, zips, looks, site.version);
    writeFileSync(join(outDir, `${pre}exccd-${tag}.zip`), zip);
    writeFileSync(join(outDir, `${pre}FILMS-${tag}.TXT`), filmsTxt(looks, site.version));

    const names = new Set(readZip(zip).map((e) => e.name));
    for (const must of ["DISKBOOT.BIN", "CHDK/FILMS/FILMS.TXT", ...(cam.firm_update ? ["PS.FIR"] : [])]) check(names.has(must), `${cam.id} ${tag}: ${must} fehlt`);
    check(!names.has("CHDK/FILMS/EXAMPLE.TXT"), `${cam.id} ${tag}: EXAMPLE.TXT gehoert nicht in die Download-ZIP`);
    for (const l of cam.looks) {
      const want = looks.includes(l);
      check(names.has(`CHDK/FILMS/${l.id}.FLM`) === want, `${cam.id} ${tag}: ${l.id}.FLM ${want ? "fehlt" : "zu viel"}`);
      if (l.curve) {
        const stem = l.curve.replace(/\.CV$/, "");
        for (const s of ["", "1", "2"])
          check(names.has(`CHDK/CURVES/${stem}${s}.CV`) === want, `${cam.id} ${tag}: ${stem}${s}.CV ${want ? "fehlt" : "zu viel"}`);
      }
    }
    console.log(`${cam.id} ${tag}: ${looks.length} Looks, ${names.size} Eintraege, ${zip.length} Bytes`);
  }

});

console.log(fails ? `FEHLGESCHLAGEN (${fails} Fehler)` : "OK");
process.exit(fails ? 1 : 0);
