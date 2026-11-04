// CI-Test fuer site/curves.js: ohne WB-Verschiebung muessen die Kurven
// bytegleich mit make_curves.py sein, mit Verschiebung plausibel.
//
//   python3 tools/filmsim/make_curves.py <kurven>
//   node tools/site/test_curves.mjs <gebaute Seite> <kurven>

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { makeCurve, curveFiles, wbGains } from "../../site/curves.js";
import { assemble, readZip } from "../../site/zipjoin.js";

const [siteDir, cvDir] = process.argv.slice(2);
const site = JSON.parse(readFileSync(join(siteDir, "data/site.json"), "utf8"));
let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FEHLER: " + msg); } };
const u16 = (b) => new Uint16Array(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength));

function same(name, got) {
  const want = readFileSync(join(cvDir, name));
  const a = u16(got), b = u16(want);
  let diff = 0, max = 0;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) { diff++; max = Math.max(max, Math.abs(a[i] - b[i])); }
  check(diff === 0, `${name}: ${diff} Werte weichen ab (max. ${max})`);
}

let n = 0;
for (const cam of site.cameras) {
  const p = cam.curve;

  // 1. Bytegleich mit Python
  for (const look of cam.looks) {
    if (!look.recipe) continue;
    if (look.curve) {
      const stem = look.curve.replace(/\.CV$/, "");
      for (const f of curveFiles(p, stem, look.recipe, [0, 0])) { same(f.name.split("/").pop(), f.bytes); n++; }
    } else {
      // Look ohne Kurve: neutrales Rezept = DR1/DR2 aus make_curves.py
      same("DR1.CV", makeCurve(p, look.recipe, 2.0)); same("DR2.CV", makeCurve(p, look.recipe, 4.0)); n += 2;
    }
  }

  // 2. Verschiebung: Faktoren nie ueber 1, Richtung stimmt, Kurven monoton
  check(wbGains([0, 0]).every((k) => k === 1), "ohne Verschiebung muessen alle Faktoren 1 sein");
  for (const shift of [[9, 0], [-9, 0], [0, 9], [0, -9], [2, -4], [9, 9]]) {
    const k = wbGains(shift);
    check(Math.max(...k) === 1 && Math.min(...k) > 0, `${shift}: Faktoren ${k}`);
    check(Math.sign(k[0] - k[1]) === Math.sign(shift[0]) || shift[0] === shift[1], `${shift}: R-Richtung`);
    check(Math.sign(k[3] - k[1]) === Math.sign(shift[1]) || shift[0] === shift[1], `${shift}: B-Richtung`);
    for (const look of cam.looks.filter((l) => l.recipe)) {
      const c = u16(makeCurve(p, look.recipe, 1, shift));
      let mono = true;
      for (let i = 1; i < c.length; i++) if (i % 1024 && c[i] < c[i - 1]) mono = false;
      check(mono && c[0] <= 1023, `${look.id} ${shift}: Kurve nicht monoton oder curve0[0] > 1023`);
    }
  }

  // 3. Download-ZIP: verschobene Kurven ersetzen die aus der Look-ZIP
  const look = cam.looks.find((l) => l.curve && l.recipe);
  const files = curveFiles(p, look.curve.replace(/\.CV$/, ""), look.recipe, [2, -4]);
  const base = readFileSync(join(siteDir, "data", cam.id, "base.zip"));
  const zip = assemble(base, [readFileSync(join(siteDir, "data", cam.id, "looks", look.id + ".zip"))],
    [{ ...look, wb: [2, -4] }], site.version, new Date(), files);
  const entries = new Map(readZip(zip).map((e) => [e.name, e]));
  for (const f of files) {
    const e = entries.get(f.name);
    check(e && e.method === 0 && Buffer.compare(Buffer.from(e.data), Buffer.from(f.bytes)) === 0, `${cam.id} ZIP: ${f.name} nicht ersetzt`);
  }
  const txt = new TextDecoder().decode(entries.get("CHDK/FILMS/FILMS.TXT").data);
  check(txt.includes(`# WB shift ${look.name}: R+2 B-4`), "FILMS.TXT: Kommentar zur WB-Verschiebung fehlt");
}

console.log(`${n} Kurven verglichen`);
console.log(fails ? `FEHLGESCHLAGEN (${fails} Fehler)` : "OK");
process.exit(fails ? 1 : 0);
