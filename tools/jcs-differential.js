// SPDX-License-Identifier: MPL-2.0
// Differential test of Maelys Canonical JSON v1 against RFC 8785 (JCS) on
// the domain where the two formats coincide: integers within the JavaScript
// safe range, strings without U+0000. The JCS reference below is the
// ECMAScript definition itself: JSON.stringify for strings and numbers, and
// property names sorted by UTF-16 code units (the default string order).
//
//   node tools/jcs-differential.js CANON_BINARY [COUNT] [SEED]
'use strict';
const { spawnSync } = require('child_process');

const binary = process.argv[2];
const count = Number(process.argv[3] || 500);
const seedArgument = process.argv[4] || '20260905';
let seed = Number(seedArgument) >>> 0;
if (!binary) {
  console.error('usage: jcs-differential.js CANON_BINARY [COUNT] [SEED]');
  process.exit(4);
}

function random() {
  // xorshift32, deterministic across runs
  seed ^= seed << 13; seed >>>= 0;
  seed ^= seed >>> 17;
  seed ^= seed << 5; seed >>>= 0;
  return seed / 4294967296;
}
const pick = (array) => array[Math.floor(random() * array.length)];
const chance = (p) => random() < p;

function jcs(value) {
  if (value === null || typeof value !== 'object') return JSON.stringify(value);
  if (Array.isArray(value)) return '[' + value.map(jcs).join(',') + ']';
  const keys = Object.keys(value).sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
  return '{' + keys.map((k) => JSON.stringify(k) + ':' + jcs(value[k])).join(',') + '}';
}

// Characters chosen to probe escaping and ordering: quotes, backslash,
// solidus, controls, DEL, BMP and supplementary code points, private use,
// noncharacter, line separators, and the UTF-16 versus code point boundary.
// Built from code point numbers so this file stays pure ASCII.
const pool = [
  'a', 'z', 'A', ' ', '0', '"', '\\', '/', '\b', '\f', '\n', '\r', '\t',
  '~', 'ab', 'ba',
  ...[0x01, 0x1f, 0x7f, 0xe9, 0x20ac, 0x1f600, 0x2028, 0x2029, 0xfffd,
    0xd7ff, 0xe000, 0xff61, 0x10000, 0x10ffff].map((cp) => String.fromCodePoint(cp)),
];
function genString() {
  const length = Math.floor(random() * 6);
  let s = '';
  for (let i = 0; i < length; ++i) s += pick(pool);
  return s;
}
function genInteger() {
  if (chance(0.2)) return 0;
  const magnitude = chance(0.3) ? Number.MAX_SAFE_INTEGER : Math.floor(random() * 1e6);
  return chance(0.5) ? -magnitude : magnitude;
}
function genValue(depth) {
  const r = random();
  if (depth > 4 || r < 0.15) return genInteger();
  if (r < 0.35) return genString();
  if (r < 0.45) return chance(0.5);
  if (r < 0.5) return null;
  if (r < 0.75) {
    const array = [];
    const n = Math.floor(random() * 4);
    for (let i = 0; i < n; ++i) array.push(genValue(depth + 1));
    return array;
  }
  const object = {};
  const n = Math.floor(random() * 5);
  for (let i = 0; i < n; ++i) {
    let key = genString();
    while (Object.prototype.hasOwnProperty.call(object, key)) key += 'x';
    object[key] = genValue(depth + 1);
  }
  return object;
}

// Input serializer: same value, randomized presentation (whitespace, escape
// spellings, hex case, "-0") so the parser is exercised, not just the writer.
function ws() { return chance(0.3) ? pick([' ', '\n', '\t', '\r\n', '  ']) : ''; }
function hex4(unit) {
  const h = unit.toString(16).padStart(4, '0');
  return chance(0.5) ? h.toUpperCase() : h;
}
const shortEscapes = { '\b': '\\b', '\f': '\\f', '\n': '\\n', '\r': '\\r', '\t': '\\t' };
function presentString(s) {
  let out = '"';
  for (const ch of s) {
    const cp = ch.codePointAt(0);
    if (ch === '"') out += chance(0.5) ? '\\"' : '\\u0022';
    else if (ch === '\\') out += '\\\\';
    else if (ch === '/') out += chance(0.5) ? '\\/' : '/';
    else if (cp < 0x20) out += shortEscapes[ch] && chance(0.5) ? shortEscapes[ch] : '\\u' + hex4(cp);
    else if (cp >= 0x80 && chance(0.5)) {
      if (cp >= 0x10000) {
        const v = cp - 0x10000;
        out += '\\u' + hex4(0xd800 + (v >> 10)) + '\\u' + hex4(0xdc00 + (v & 0x3ff));
      } else out += '\\u' + hex4(cp);
    } else out += ch;
  }
  return out + '"';
}
function present(value) {
  if (value === null || typeof value === 'boolean') return String(value);
  if (typeof value === 'number') return value === 0 && chance(0.5) ? '-0' : String(value);
  if (typeof value === 'string') return presentString(value);
  if (Array.isArray(value)) return '[' + ws() + value.map(present).join(',' + ws()) + ws() + ']';
  const keys = Object.keys(value);
  return '{' + ws() + keys.map((k) => presentString(k) + ws() + ':' + ws() + present(value[k])).join(',' + ws()) + ws() + '}';
}

let failures = 0;
for (let i = 0; i < count; ++i) {
  const value = genValue(0);
  const input = present(value);
  const expected = jcs(value);
  const run = spawnSync(binary, [], { input, encoding: 'utf8' });
  if (run.status !== 0 || run.stdout !== expected) {
    failures += 1;
    if (failures <= 5) {
      console.error(`mismatch #${i} (exit ${run.status})\n  input:    ${JSON.stringify(input)}\n  expected: ${expected}\n  got:      ${run.stdout}${run.stderr}`);
    }
  }
}
console.log(`jcs-differential: ${count} documents, ${failures} mismatches (seed ${seedArgument})`);
process.exit(failures ? 1 : 0);
