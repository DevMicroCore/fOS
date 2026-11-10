'use strict';
const catalog = require('./catalog.json');
const keywords = ['var', 'function', 'return', 'if', 'then', 'elseif', 'else', 'end', 'while', 'for', 'to', 'step', 'break', 'continue', 'on', 'true', 'false', 'nil', 'and', 'or', 'not'];
const types = catalog.uiFields.type.values;

function signature(item, name = item.name) {
  return `${name}(${item.params.map(p => `${p.name}: ${p.type}`).join(', ')}) → ${item.result}`;
}
function snippet(item, name = item.name) {
  const escape = s => s.replace(/\\/g, '\\\\').replace(/\$/g, '\\$').replace(/}/g, '\\}');
  return `${name}(${item.params.map((p, i) => `\${${i + 1}:${escape(p.default)}}`).join(', ')})`;
}
function supports(item, type) { return item.types.includes('*') || item.types.includes(type); }
function members(type, event = false) {
  if (event) return catalog.events.filter(e => supports(e, type));
  return [...catalog.properties.filter(p => supports(p, type)), ...catalog.methods.filter(m => supports(m, type))];
}

// Preserve offsets/newlines while masking comments and strings. The lexer also
// accepts // and /* ... */ in addition to the usual -- line comment.
function mask(source, strings = true) {
  const result = source.split('');
  let state = 'code', quote = '', escaped = false;
  for (let i = 0; i < source.length; i++) {
    const c = source[i], next = source[i + 1];
    if (state === 'line') {
      if (c === '\n') state = 'code'; else result[i] = ' ';
    } else if (state === 'block') {
      if (c !== '\n' && c !== '\r') result[i] = ' ';
      if (c === '*' && next === '/') { result[i + 1] = ' '; i++; state = 'code'; }
    } else if (state === 'string') {
      if (strings && c !== '\n' && c !== '\r') result[i] = ' ';
      if (c === '\n') { state = 'code'; escaped = false; }
      else if (escaped) escaped = false;
      else if (c === '\\') escaped = true;
      else if (c === quote) state = 'code';
    } else if ((c === '-' && next === '-') || (c === '/' && next === '/')) {
      result[i] = result[i + 1] = ' '; i++; state = 'line';
    } else if (c === '/' && next === '*') {
      result[i] = result[i + 1] = ' '; i++; state = 'block';
    } else if (c === '"' || c === "'") {
      quote = c; state = 'string'; escaped = false; if (strings) result[i] = ' ';
    }
  }
  return { text: result.join(''), state };
}
function context(source, offset) {
  const masked = mask(source.slice(0, offset));
  if (masked.state !== 'code') return { kind: 'none' };
  const line = masked.text.slice(masked.text.lastIndexOf('\n') + 1);
  const match = line.match(/([A-Za-z_]\w*)\.([A-Za-z_]\w*|)$/);
  if (match) return { kind: /^\s*on\s/.test(line) ? 'eventMember' : 'member', receiver: match[1], prefix: match[2], start: offset - match[2].length };
  if (/^\s*on\s+\w*$/.test(line)) return { kind: 'eventReceiver', prefix: line.match(/\w*$/)[0] };
  return { kind: 'global' };
}
function callContext(source, offset) {
  const result = mask(source.slice(0, offset));
  if (result.state === 'line' || result.state === 'block') return null;
  const stack = [];
  for (let i = 0; i < result.text.length; i++) {
    const c = result.text[i];
    if (c === '(') {
      const callee = result.text.slice(0, i).match(/([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?)\s*$/);
      stack.push({ name: callee ? callee[1] : '', parameter: 0 });
    } else if (c === ')') stack.pop();
    else if (c === ',' && stack.length) stack[stack.length - 1].parameter++;
  }
  return [...stack].reverse().find(frame => frame.name) || null;
}
function parseUi(source) {
  const rows = [];
  let offset = 0;
  source.split('\n').forEach((line, lineNumber) => {
    if (!line.trim() || line.trimStart().startsWith('#')) { offset += line.length + 1; return; }
    const fields = {}, locations = {};
    let first = 0;
    for (const field of line.split(';')) {
      const match = field.match(/^\s*([A-Za-z_]\w*)\s*=([\s\S]*)$/);
      if (match) {
        const key = match[1].toLowerCase(), value = match[2].trim();
        fields[key] = value;
        const equals = field.indexOf('=');
        const padding = field.slice(equals + 1).length - field.slice(equals + 1).trimStart().length;
        locations[key] = { start: offset + first + equals + 1 + padding, length: value.length, keyStart: offset + first + field.indexOf(match[1]) };
      }
      first += field.length + 1;
    }
    rows.push({ fields, locations, line: lineNumber, start: offset, length: line.length });
    offset += line.length + 1;
  });
  return rows;
}
function uiContext(line, character) {
  if (line.trimStart().startsWith('#')) return { kind: 'none' };
  const start = line.lastIndexOf(';', Math.max(0, character - 1)) + 1;
  const next = line.indexOf(';', character);
  const end = next < 0 ? line.length : next;
  const part = line.slice(start, character);
  const equals = part.indexOf('=');
  if (equals < 0) {
    const whole = line.slice(start, end), existingEquals = whole.indexOf('=');
    return { kind: 'key', start: start + (part.length - part.trimStart().length), end: existingEquals < 0 ? end : start + existingEquals, hasEquals: existingEquals >= 0, currentKey: whole.slice(0, existingEquals < 0 ? whole.length : existingEquals).trim(), prefix: part.trim() };
  }
  const tail = part.slice(equals + 1);
  return { kind: 'value', key: part.slice(0, equals).trim().toLowerCase(), start: start + equals + 1 + (tail.length - tail.trimStart().length), end, prefix: tail.trim() };
}
function symbols(source) {
  const masked = mask(source).text;
  const result = [];
  for (const match of masked.matchAll(/\b(function|var)\s+([A-Za-z_]\w*)/g)) {
    const name = match[2], start = match.index + match[0].lastIndexOf(name);
    let params = [];
    if (match[1] === 'function') {
      const argumentsMatch = masked.slice(start + name.length).match(/^\s*\(([^)]*)\)/);
      if (argumentsMatch) params = argumentsMatch[1].split(',').map(s => s.trim()).filter(Boolean).map(name => ({ name, type: 'Any', default: name }));
    }
    result.push({ name, kind: match[1], start, length: name.length, params, result: 'Any', doc: 'Defined in this fScript file.' });
  }
  return result;
}
function folding(source) {
  const stack = [], ranges = [];
  mask(source).text.split('\n').forEach((line, index) => {
    const keyword = line.trim().match(/^(function|on|if|while|for|end)\b/);
    if (!keyword) return;
    if (keyword[1] === 'end') { const start = stack.pop(); if (start !== undefined && index > start) ranges.push({ start, end: index }); }
    else stack.push(index);
  });
  return ranges;
}
function uiIssues(source) {
  const rows = parseUi(source), issues = [], known = new Map();
  const all = new Map(rows.filter(r => r.fields.id).map(r => [r.fields.id, r]));
  const issue = (row, key, message, warning = false) => {
    const location = row.locations[key] || { start: row.start, length: row.length };
    issues.push({ start: location.start, length: Math.max(1, location.length), message, warning });
  };
  for (const row of rows) {
    const f = row.fields;
    if (!f.type || !types.includes(f.type.toLowerCase())) issue(row, 'type', 'Unknown or missing fOS widget type.');
    if (f.id) {
      if (!/^[A-Za-z_]\w*$/.test(f.id)) issue(row, 'id', 'The ID must be a valid fScript identifier.');
      if (known.has(f.id)) issue(row, 'id', `Duplicate UI ID: ${f.id}`);
    }
    if (f.parent && !known.has(f.parent)) issue(row, 'parent', `Parent object ${f.parent} must be declared before this widget.`);
    if (f.type === 'tab' && (!f.parent || known.get(f.parent)?.fields.type !== 'tabview')) issue(row, 'parent', 'A tab requires parent=<tabview-id>.');
    if (f.target && (!all.has(f.target) || all.get(f.target).fields.type !== 'textarea')) issue(row, 'target', 'The keyboard target must be the ID of a text area.');
    for (const key of Object.keys(f)) if (!catalog.uiFields[key]) issue(row, key, `Field ${key} is not processed by the fOS 4.0.1 loader.`, true);
    if (f.id) known.set(f.id, row);
  }
  return issues;
}
function parseDiagnostics(output, defaultFile) {
  const diagnostics = [];
  const regex = /(?:Error|Warning)\s+(FS\d+):\s*([^\n]+)\nFile:\s*([^\n]+)\nLine\s+(\d+),\s*Column\s+(\d+)/g;
  for (const match of output.replace(/\r/g, '').matchAll(regex)) diagnostics.push({ code: match[1], message: match[2].trim(), file: match[3].trim() || defaultFile, line: Math.max(0, +match[4] - 1), column: Math.max(0, +match[5] - 1) });
  return diagnostics;
}
module.exports = { catalog, keywords, types, signature, snippet, supports, members, mask, context, callContext, parseUi, uiContext, symbols, folding, uiIssues, parseDiagnostics };
