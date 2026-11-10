'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const lang = require('../src/language');

test('mask keeps UTF-16 positions and ignores all compiler comment forms', () => {
  const text = 'var x = "😀, )" -- note\n// )\n/* (, ) */\naudio_play("song")';
  const masked = lang.mask(text);
  assert.equal(masked.text.length, text.length);
  assert.equal(masked.text.split('\n').length, text.split('\n').length);
  assert.equal(masked.state, 'code');
  assert.equal(lang.symbols('/* var bad = 0 */\nvar good = "var fake"').length, 1);
});
test('members distinguish widgets, events and namespaces', () => {
  assert.ok(lang.members('roller').some(x => x.name === 'load_pairs'));
  assert.ok(!lang.members('slider').some(x => x.name === 'load_pairs'));
  assert.ok(lang.members('slider', true).some(x => x.name === 'released'));
  assert.ok(!lang.members('roller', true).some(x => x.name === 'released'));
  assert.deepEqual(lang.context('on btn_play.cl', 14), { kind: 'eventMember', receiver: 'btn_play', prefix: 'cl', start: 12 });
  assert.equal(lang.context('audio_play("roller.', 19).kind, 'none');
  assert.equal(lang.context('-- roller.', 10).kind, 'none');
});
test('signature help handles nested/multiline calls and comma inside strings', () => {
  const a = 'file_write(\n  "one,two",\n  text(round(12.5))';
  assert.equal(lang.callContext(a, a.length).name, 'file_write');
  assert.equal(lang.callContext(a, a.length).parameter, 1);
  const b = 'roller_files.load_dir_filtered("/music,files", "/music/files", "';
  assert.equal(lang.callContext(b, b.length).parameter, 2);
  assert.equal(lang.callContext('text(round(', 11).name, 'round');
  assert.equal(lang.callContext('// text(', 8), null);
});
test('UI parser respects literal text, CRLF, selection and offsets', () => {
  const ui = '# ignored\r\ntype=tabview;id=tabs;tab_height=50\r\ntype=tab;id=files;parent=tabs;text=File Player\r\ntype=roller;id=roller_files;parent=files;text=A\\nB\r\n';
  const rows = lang.parseUi(ui);
  assert.equal(rows.length, 3);
  assert.equal(rows[2].fields.text, 'A\\nB');
  const id = rows[2].locations.id;
  assert.equal(ui.slice(id.start, id.start + id.length), 'roller_files');
  assert.deepEqual(lang.uiIssues(ui), []);
});
test('UI key replacements preserve existing equals sign and value', () => {
  const ctx = lang.uiContext('type=button;id=btn_play', 2);
  assert.equal(ctx.kind, 'key'); assert.equal(ctx.end, 4); assert.equal(ctx.hasEquals, true);
  const value = lang.uiContext('type=button;bg=the', 17);
  assert.equal(value.key, 'bg'); assert.equal(value.start, 15);
});
test('UI validation permits forward keyboard targets but rejects forward parents', () => {
  assert.equal(lang.uiIssues('type=keyboard;id=kb;target=txt\ntype=textarea;id=txt').length, 0);
  const issues = lang.uiIssues('type=button;id=btn;parent=panel\ntype=panel;id=panel\ntype=button;id=btn');
  assert.equal(issues.length, 2);
  assert.match(issues[0].message, /declared before/);
  assert.match(issues[1].message, /Duplicate/);
});
test('block folding handles elseif/else without extra nesting', () => {
  assert.deepEqual(lang.folding('on app.start\n if true then\n elseif false then\n else\n end\nend\n'), [{ start: 1, end: 4 }, { start: 0, end: 5 }]);
});
test('user functions retain arguments for completions', () => {
  const functions = lang.symbols('function test(a, b)\n  var local = 1\nend\n');
  assert.equal(lang.signature(functions[0]), 'test(a: Any, b: Any) → Any');
  assert.equal(lang.snippet(functions[0]), 'test(${1:a}, ${2:b})');
});
test('compiler diagnostics preserve filenames containing spaces', () => {
  const result = lang.parseDiagnostics('Error FS512: Unknown UI object\nFile: /tmp/a b/main.fscript\nLine 3, Column 8\n', '');
  assert.deepEqual(result[0], { code: 'FS512', message: 'Unknown UI object', file: '/tmp/a b/main.fscript', line: 2, column: 7 });
});
test('catalog covers all native fOS 4.0.1 commands and safe decimal canvas defaults', () => {
  assert.equal(new Set(lang.catalog.builtins.map(x => x.name)).size, 35);
  for (const method of lang.catalog.methods) assert.ok(!method.params.some(p => /^0x/.test(p.default)), 'fScript numbers are decimal');
  assert.equal(lang.catalog.builtins.find(x => x.name === 'audio_stream').permission, 'audio, network');
  if (process.env.FOS_SOURCE_ROOT) {
    const bytecode = fs.readFileSync(path.join(process.env.FOS_SOURCE_ROOT, 'fScript_Development_Environment/fosc/bytecode/Bytecode.cpp'), 'utf8');
    const names = [...bytecode.matchAll(/case NativeFunction::\w+: return "([^"]+)";/g)].map(m => m[1]).sort();
    assert.deepEqual(lang.catalog.builtins.map(x => x.name).sort(), names);
  }
});
test('every bundled JSON parses and every grammar reference exists', () => {
  const root = path.join(__dirname, '..');
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'package.json')));
  for (const contribution of [...manifest.contributes.grammars, ...manifest.contributes.snippets, ...manifest.contributes.languages]) {
    const file = contribution.path || contribution.configuration;
    const data = JSON.parse(fs.readFileSync(path.join(root, file)));
    if (data.scopeName) for (const rule of data.patterns) for (const key of ['match', 'begin', 'end']) if (rule[key]) assert.doesNotThrow(() => new RegExp(rule[key]));
  }
});
