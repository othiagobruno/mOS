// mOS — WOFF2 de sabor CFF (sem transformação de glyf/loca) -> OTF, para o tools/cfftest.c: node tools/woff2otf.js entrada.woff2 saida.otf
const fs = require('fs'), zlib = require('zlib');
const TAGS = ["cmap","head","hhea","hmtx","maxp","name","OS/2","post","cvt ","fpgm","glyf","loca","prep","CFF ","VORG","EBDT","EBLC","gasp","hdmx","kern","LTSH","PCLT","VDMX","vhea","vmtx","BASE","GDEF","GPOS","GSUB","EBSC","JSTF","MATH","CBDT","CBLC","COLR","CPAL","SVG ","sbix","acnt","avar","bdat","bloc","bsln","cvar","fdsc","feat","fmtx","fvar","gvar","hsty","just","lcar","mort","morx","opbd","prop","trak","Zapf","Silf","Glat","Gloc","Feat","Sill"];
const b = fs.readFileSync(process.argv[2]);
if (b.toString('latin1', 0, 4) !== 'wOF2') throw new Error('não é WOFF2');
const flavor = b.readUInt32BE(4), numTables = b.readUInt16BE(12), compLen = b.readUInt32BE(20);
let p = 48;
function base128() { let v = 0; for (let i = 0; i < 5; i++) { const c = b[p++]; v = v * 128 + (c & 127); if (!(c & 128)) return v; } throw new Error('base128'); }
const tabs = [];
for (let i = 0; i < numTables; i++) {
  const fl = b[p++]; let tag;
  if ((fl & 63) === 63) { tag = b.toString('latin1', p, p + 4); p += 4; } else tag = TAGS[fl & 63];
  const ver = fl >> 6, orig = base128();
  const transformed = (tag === 'glyf' || tag === 'loca') ? ver === 0 : (tag === 'hmtx' ? ver === 1 : false);
  const tlen = transformed ? base128() : orig;
  if (transformed) throw new Error('tabela transformada: ' + tag);
  tabs.push({ tag, len: orig, clen: tlen });
}
const data = zlib.brotliDecompressSync(b.subarray(p, p + compLen));
let off = 0; for (const t of tabs) { t.data = data.subarray(off, off + t.clen); off += t.clen; }
tabs.sort((a, c) => a.tag < c.tag ? -1 : 1);
const hdr = 12 + 16 * tabs.length; let size = hdr; for (const t of tabs) size += (t.len + 3) & ~3;
const out = Buffer.alloc(size); out.writeUInt32BE(flavor, 0); out.writeUInt16BE(tabs.length, 4);
let es = 0; while ((2 << es) <= tabs.length) es++;
out.writeUInt16BE(16 << es, 6); out.writeUInt16BE(es, 8); out.writeUInt16BE(tabs.length * 16 - (16 << es), 10);
let at = hdr;
tabs.forEach((t, i) => { const r = 12 + 16 * i; out.write(t.tag, r, 'latin1'); out.writeUInt32BE(0, r + 4); out.writeUInt32BE(at, r + 8); out.writeUInt32BE(t.len, r + 12); t.data.copy(out, at); at += (t.len + 3) & ~3; });
fs.writeFileSync(process.argv[3], out);
console.log(process.argv[3], out.length, 'bytes,', tabs.map(t => t.tag.trim()).join(' '));
