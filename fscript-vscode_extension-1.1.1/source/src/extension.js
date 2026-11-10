'use strict';
const vscode = require('vscode');
const path = require('node:path');
const os = require('node:os');
const { execFile } = require('node:child_process');
const lang = require('./language');
const { Toolchain, resolveSetting } = require('./toolchain');
const { Runner, webviewHtml } = require('./runner');
const scriptSelector = { language: 'fscript' };
const uiSelector = { language: 'fscript-ui' };
let diagnostics, compilerDiagnostics, output, tools, runner;
let runPending = false;
let runRequest = 0;
const checking = new Map();

function docs(item, name = item.name) {
  const md = new vscode.MarkdownString();
  if (item.params) md.appendCodeblock(lang.signature(item, name), 'fscript');
  else md.appendCodeblock(`${name}${item.result ? ': ' + item.result : ''}`, 'fscript');
  md.appendText(item.doc || '');
  if (item.permission) md.appendMarkdown(`\n\n**app.json:** \`${item.permission}\``);
  if (item.readonly) md.appendMarkdown('\n\n**Read-only.**');
  return md;
}
function range(document, start, length) {
  return new vscode.Range(document.positionAt(start), document.positionAt(start + length));
}
async function readDocument(uri) {
  const open = vscode.workspace.textDocuments.find(d => d.uri.toString() === uri.toString());
  if (open) return { uri, text: open.getText(), document: open };
  try { return { uri, text: Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8') }; }
  catch { return null; }
}
function relativeFile(value) {
  return typeof value === 'string' && value.length && !value.startsWith('/') && !value.includes('\\') && !value.split('/').some(p => !p || p === '.' || p === '..');
}
async function appFiles(document) {
  if (document.uri.scheme === 'untitled') return { layout: null, manifest: {} };
  const directory = vscode.Uri.joinPath(document.uri, '..');
  const manifestFile = await readDocument(vscode.Uri.joinPath(directory, 'app.json'));
  let manifest = {};
  try { if (manifestFile) manifest = JSON.parse(manifestFile.text); } catch { /* editing a manifest */ }
  if (document.languageId === 'fscript-ui') return { layout: { uri: document.uri, text: document.getText(), document }, manifest };
  const basename = path.basename(document.uri.path).replace(/\.fscript$/i, '.ui');
  const candidates = [...new Set([...(relativeFile(manifest.layout) ? [manifest.layout] : []), basename, 'layout.ui'])];
  for (const candidate of candidates) {
    const layout = await readDocument(vscode.Uri.joinPath(directory, candidate));
    if (layout) return { layout, manifest };
  }
  return { layout: null, manifest };
}
async function uiObjects(document) {
  const { layout } = await appFiles(document);
  return layout ? lang.parseUi(layout.text).filter(r => r.fields.id).map(r => ({ name: r.fields.id, type: r.fields.type?.toLowerCase(), row: r, uri: layout.uri })) : [];
}
function itemFor(info, name = info.name, replacement) {
  const item = new vscode.CompletionItem(name, info.params ? vscode.CompletionItemKind.Method : vscode.CompletionItemKind.Property);
  item.detail = info.params ? lang.signature(info, name) : `${info.result || 'Event'}${info.readonly ? ' (read-only)' : ''}`;
  item.documentation = docs(info, name);
  item.insertText = info.params ? new vscode.SnippetString(lang.snippet(info, name)) : name;
  if (replacement) item.range = replacement;
  return item;
}
function completionRange(document, position, start) {
  const line = document.lineAt(position.line).text;
  const remainder = line.slice(position.character).match(/^\w*/)[0];
  return new vscode.Range(new vscode.Position(position.line, start), new vscode.Position(position.line, position.character + remainder.length));
}
async function scriptCompletions(document, position) {
  const source = document.getText(), offset = document.offsetAt(position);
  const context = lang.context(source, offset);
  if (context.kind === 'none') return [];
  const objects = await uiObjects(document);
  if (context.kind === 'member' || context.kind === 'eventMember') {
    const replacement = completionRange(document, position, position.character - context.prefix.length);
    if (context.receiver === 'app' && context.kind === 'eventMember') return lang.catalog.systemEvents.map(name => itemFor({ name, doc: 'App lifecycle event.' }, name, replacement));
    const object = objects.find(o => o.name === context.receiver);
    if (object) return lang.members(object.type, context.kind === 'eventMember').map(m => itemFor(m, m.name, replacement));
    if (context.kind === 'eventMember') return [];
    return lang.catalog.builtins.filter(f => f.name.toLowerCase().startsWith(context.receiver.toLowerCase() + '.')).map(f => itemFor(f, f.name.split('.')[1], replacement));
  }
  const currentWord = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
  const items = [];
  if (context.kind === 'eventReceiver') {
    for (const name of ['app', ...objects.map(o => o.name)]) {
      const i = new vscode.CompletionItem(name, vscode.CompletionItemKind.Variable); i.range = currentWord; i.insertText = name; items.push(i);
    }
    return items;
  }
  for (const builtin of lang.catalog.builtins) {
    const item = itemFor(builtin); item.kind = vscode.CompletionItemKind.Function; item.range = currentWord; items.push(item);
  }
  for (const name of lang.keywords) { const i = new vscode.CompletionItem(name, vscode.CompletionItemKind.Keyword); i.range = currentWord; items.push(i); }
  for (const object of objects) {
    const i = new vscode.CompletionItem(object.name, vscode.CompletionItemKind.Variable); i.detail = `UI: ${object.type}`; i.range = currentWord; items.push(i);
  }
  for (const symbol of lang.symbols(source)) {
    const i = symbol.kind === 'function' ? itemFor(symbol) : new vscode.CompletionItem(symbol.name, vscode.CompletionItemKind.Variable);
    i.kind = symbol.kind === 'function' ? vscode.CompletionItemKind.Function : vscode.CompletionItemKind.Variable;
    i.range = currentWord; items.push(i);
  }
  return items;
}
async function uiCompletions(document, position) {
  const line = document.lineAt(position.line).text;
  const context = lang.uiContext(line, position.character);
  if (context.kind === 'none') return [];
  const rows = lang.parseUi(document.getText());
  const row = rows.find(r => r.line === position.line);
  const type = row?.fields.type?.toLowerCase();
  const replacement = new vscode.Range(position.line, context.start, position.line, context.end);
  if (context.kind === 'key') return Object.entries(lang.catalog.uiFields).filter(([key, field]) => (!field.types || !type || field.types.includes(type)) && (!row?.fields[key] || key === context.currentKey)).map(([name, field]) => {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Property);
    item.insertText = name + (context.hasEquals ? '' : '='); item.range = replacement; item.documentation = field.doc;
    item.command = { command: 'editor.action.triggerSuggest', title: 'Suggest Values' };
    return item;
  });
  let values = lang.catalog.uiFields[context.key]?.values || [];
  if (context.key === 'parent') values = rows.filter(r => r.line < position.line && r.fields.id && (type !== 'tab' || r.fields.type === 'tabview')).map(r => r.fields.id);
  if (context.key === 'target') values = rows.filter(r => r.fields.type === 'textarea' && r.fields.id).map(r => r.fields.id);
  if (context.key === 'value') values = type === 'switch' || type === 'checkbox' ? ['true', 'false'] : ['0', '15', '100'];
  return values.map(value => { const i = new vscode.CompletionItem(value, vscode.CompletionItemKind.Value); i.insertText = value; i.range = replacement; i.documentation = lang.catalog.uiFields[context.key]?.doc; return i; });
}
async function lookup(document, name) {
  const builtin = lang.catalog.builtins.find(f => f.name.toLowerCase() === name.toLowerCase());
  if (builtin) return builtin;
  if (name.includes('.')) {
    const [receiver, member] = name.split('.');
    if (receiver === 'app' && lang.catalog.systemEvents.includes(member)) return { name, doc: 'Lifecycle event: on ' + name };
    const object = (await uiObjects(document)).find(o => o.name === receiver);
    if (object) return [...lang.members(object.type), ...lang.members(object.type, true)].find(m => m.name === member);
  }
  return lang.symbols(document.getText()).find(s => s.name === name);
}
async function scriptHover(document, position) {
  const word = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?/);
  if (!word || lang.mask(document.getText().slice(0, document.offsetAt(position))).state !== 'code') return undefined;
  const name = document.getText(word), info = await lookup(document, name);
  if (info) return new vscode.Hover(docs(info, name), word);
  const object = (await uiObjects(document)).find(o => o.name === name);
  if (object) return new vscode.Hover(`UI object **${name}** (${object.type}) from ${path.basename(object.uri.path)}.`, word);
}
async function signatureHelp(document, position) {
  const call = lang.callContext(document.getText(), document.offsetAt(position));
  if (!call) return undefined;
  const item = await lookup(document, call.name);
  if (!item?.params) return undefined;
  const info = new vscode.SignatureInformation(lang.signature(item, call.name), docs(item, call.name));
  info.parameters = item.params.map(p => new vscode.ParameterInformation(`${p.name}: ${p.type}`));
  const result = new vscode.SignatureHelp(); result.signatures = [info]; result.activeSignature = 0;
  result.activeParameter = Math.min(call.parameter, Math.max(0, item.params.length - 1));
  return result;
}
async function definition(document, position) {
  const word = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
  if (!word) return undefined;
  const name = document.getText(word);
  const object = (await uiObjects(document)).find(o => o.name === name);
  if (object) {
    const target = await vscode.workspace.openTextDocument(object.uri);
    const loc = object.row.locations.id;
    return new vscode.Location(object.uri, range(target, loc.start, loc.length));
  }
  const symbol = lang.symbols(document.getText()).find(s => s.name === name);
  return symbol ? new vscode.Location(document.uri, range(document, symbol.start, symbol.length)) : undefined;
}
function updateUiDiagnostics(document) {
  if (document.languageId !== 'fscript-ui' || !diagnostics) return;
  diagnostics.set(document.uri, lang.uiIssues(document.getText()).map(issue => {
    const diagnostic = new vscode.Diagnostic(range(document, issue.start, issue.length), issue.message, issue.warning ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error);
    diagnostic.source = 'fOS UI'; return diagnostic;
  }));
}
function uiHover(document, position) {
  const row = lang.parseUi(document.getText()).find(r => r.line === position.line);
  const word = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
  if (!row || !word) return undefined;
  const key = document.getText(word).toLowerCase();
  if (lang.catalog.uiFields[key]) return new vscode.Hover(lang.catalog.uiFields[key].doc, word);
}
function documentSymbols(document) {
  if (document.languageId === 'fscript-ui') return lang.parseUi(document.getText()).filter(r => r.fields.id).map(row => new vscode.DocumentSymbol(row.fields.id, row.fields.type || 'UI', vscode.SymbolKind.Object, range(document, row.start, row.length), range(document, row.locations.id.start, row.locations.id.length)));
  return lang.symbols(document.getText()).map(s => { const r = range(document, s.start, s.length); return new vscode.DocumentSymbol(s.name, s.kind, s.kind === 'function' ? vscode.SymbolKind.Function : vscode.SymbolKind.Variable, r, r); });
}
function colors(document) {
  const result = [];
  for (const row of lang.parseUi(document.getText())) for (const key of ['bg', 'fg']) {
    const value = row.fields[key];
    if (!value || !/^(?:0[xX])?[0-9a-fA-F]{6}$/.test(value)) continue;
    const hex = value.replace(/^0[xX]/, ''), rgb = parseInt(hex, 16), loc = row.locations[key];
    result.push(new vscode.ColorInformation(range(document, loc.start, loc.length), new vscode.Color((rgb >> 16) / 255, ((rgb >> 8) & 255) / 255, (rgb & 255) / 255, 1)));
  }
  return result;
}
async function currentScript() {
  let document = vscode.window.activeTextEditor?.document;
  if (!document) throw new Error('Open a fScript or UI file.');
  if (document.uri.scheme !== 'file') throw new Error('Save the file locally first.');
  if (document.languageId === 'fscript-ui') {
    const { manifest } = await appFiles(document);
    const source = relativeFile(manifest.source) ? manifest.source : 'main.fscript';
    document = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(document.uri, '..', source));
  }
  if (document.languageId !== 'fscript') throw new Error('Open a .fscript file.');
  return document;
}
async function compile(document, build = false, automatic = false) {
  if (!vscode.workspace.isTrusted) throw new Error('fosc commands require a trusted workspace.');
  if (document.uri.scheme !== 'file') throw new Error('Save the file locally first.');
  const key = document.uri.toString();
  if (checking.has(key)) { if (!automatic) vscode.window.showInformationMessage('fosc is already running for this file.'); return; }
  checking.set(key, true);
  try {
    if (document.isDirty) await document.save();
    const { layout, manifest } = await appFiles(document);
    if (layout?.document?.isDirty) await layout.document.save();
    const config = vscode.workspace.getConfiguration('fscript', document.uri);
    const base = vscode.workspace.getWorkspaceFolder(document.uri)?.uri.fsPath || path.dirname(document.uri.fsPath);
    tools.cxx = config.get('cxxPath', '') ? resolveSetting(config.get('cxxPath'), base) : '';
    const executable = await tools.resolve('fosc', config.get('compilerPath', ''), base, automatic);
    const args = [build ? 'build' : 'check', document.uri.fsPath];
    if (layout) args.push('--ui', layout.uri.fsPath);
    const target = path.join(path.dirname(document.uri.fsPath), relativeFile(manifest.executable) ? manifest.executable : 'main.fapp');
    if (build) args.push('-o', target);
    output.appendLine(`\n${build ? 'Build' : 'Check'}: ${document.uri.fsPath}`);
    if (!automatic) output.show(true);
    const result = await new Promise(resolve => execFile(executable, args, { cwd: path.dirname(document.uri.fsPath), timeout: 30000, maxBuffer: 2 * 1024 * 1024, windowsHide: true }, (error, stdout, stderr) => resolve({ error, text: stdout + stderr })));
    output.append(result.text);
    const grouped = new Map();
    for (const error of lang.parseDiagnostics(result.text, document.uri.fsPath)) {
      const uri = vscode.Uri.file(path.resolve(path.dirname(document.uri.fsPath), error.file));
      const position = new vscode.Position(error.line, error.column);
      const diagnostic = new vscode.Diagnostic(new vscode.Range(position, position.translate(0, 1)), error.message, vscode.DiagnosticSeverity.Error);
      diagnostic.source = 'fosc'; diagnostic.code = error.code;
      if (!grouped.has(uri.toString())) grouped.set(uri.toString(), { uri, items: [] });
      grouped.get(uri.toString()).items.push(diagnostic);
    }
    compilerDiagnostics.delete(document.uri); if (layout) compilerDiagnostics.delete(layout.uri);
    for (const group of grouped.values()) compilerDiagnostics.set(group.uri, group.items);
    if (result.error) {
      if (result.error.code === 'ENOENT') throw new Error('fosc was not found. Set fscript.compilerPath in Settings to the path of your locally built fosc.');
      if (!grouped.size) throw new Error(`Could not run fosc: ${result.error.message}`);
      if (!automatic) vscode.window.showErrorMessage('fosc reported errors. See Problems and Output.');
      return null;
    } else if (!automatic) vscode.window.showInformationMessage(build ? `fAPP created: ${path.basename(target)}` : 'fScript check passed.');
    return { image: target, layout, document, manifest };
  } finally { checking.delete(key); }
}
function activate(context) {
  diagnostics = vscode.languages.createDiagnosticCollection('fOS UI');
  compilerDiagnostics = vscode.languages.createDiagnosticCollection('fosc');
  output = vscode.window.createOutputChannel('fScript / fosc');
  const extensionPath = context.extensionPath || path.resolve(__dirname, '..');
  const storagePath = context.globalStorageUri?.fsPath || path.join(os.tmpdir(), 'fscript-extension-tests');
  tools = new Toolchain({ extensionPath, storagePath, log: s => output.appendLine(s), progress: work =>
    vscode.window.withProgress({ location: vscode.ProgressLocation.Notification, title: 'fScript: Prepare Tools', cancellable: false }, p => work(message => p.report({ message }))) });
  runner = new Runner(s => output.append(s));
  context.subscriptions.push(tools, runner);
  context.subscriptions.push(diagnostics, compilerDiagnostics, output);
  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(scriptSelector, { provideCompletionItems: scriptCompletions }, '.', ' '),
    vscode.languages.registerCompletionItemProvider(uiSelector, { provideCompletionItems: uiCompletions }, '=', ';'),
    vscode.languages.registerHoverProvider(scriptSelector, { provideHover: scriptHover }),
    vscode.languages.registerHoverProvider(uiSelector, { provideHover: uiHover }),
    vscode.languages.registerSignatureHelpProvider(scriptSelector, { provideSignatureHelp: signatureHelp }, '(', ','),
    vscode.languages.registerDefinitionProvider(scriptSelector, { provideDefinition: definition }),
    vscode.languages.registerDefinitionProvider(uiSelector, { provideDefinition: definition }),
    vscode.languages.registerDocumentSymbolProvider([scriptSelector, uiSelector], { provideDocumentSymbols: documentSymbols }),
    vscode.languages.registerFoldingRangeProvider(scriptSelector, { provideFoldingRanges: d => lang.folding(d.getText()).map(r => new vscode.FoldingRange(r.start, r.end)) }),
    vscode.languages.registerColorProvider(uiSelector, {
      provideDocumentColors: colors,
      provideColorPresentations: (color, info) => {
        const hex = '0x' + [color.red, color.green, color.blue].map(v => Math.round(v * 255).toString(16).padStart(2, '0')).join('').toUpperCase();
        const presentation = new vscode.ColorPresentation(hex); presentation.textEdit = vscode.TextEdit.replace(info.range, hex); return [presentation];
      }
    }),
    vscode.workspace.onDidOpenTextDocument(updateUiDiagnostics),
    vscode.workspace.onDidChangeTextDocument(e => updateUiDiagnostics(e.document)),
    vscode.workspace.onDidCloseTextDocument(d => { diagnostics.delete(d.uri); compilerDiagnostics.delete(d.uri); }),
    vscode.workspace.onDidSaveTextDocument(d => {
      if (d.languageId === 'fscript' && vscode.workspace.isTrusted && vscode.workspace.getConfiguration('fscript', d.uri).get('checkOnSave', false)) compile(d, false, true).catch(e => output.appendLine(e.message));
    })
  );
  for (const document of vscode.workspace.textDocuments) updateUiDiagnostics(document);
  for (const [name, build] of [['fscript.check', false], ['fscript.build', true]]) context.subscriptions.push(vscode.commands.registerCommand(name, async () => {
    try { await compile(await currentScript(), build); } catch (error) { vscode.window.showErrorMessage(error.message); }
  }));
  for (const [command, force] of [['fscript.prepareTools', false], ['fscript.rebuildTools', true]]) context.subscriptions.push(vscode.commands.registerCommand(command, async () => {
    try {
      if (!vscode.workspace.isTrusted) throw new Error('Preparing tools requires a trusted workspace.');
      const document = vscode.window.activeTextEditor?.document;
      const config = vscode.workspace.getConfiguration('fscript', document?.uri);
      const base = document ? vscode.workspace.getWorkspaceFolder(document.uri)?.uri.fsPath || path.dirname(document.uri.fsPath) : process.cwd();
      tools.cxx = config.get('cxxPath', '') ? resolveSetting(config.get('cxxPath'), base) : '';
      if (force) { runRequest++;runner.stop(); }
      const prepared = await tools.prepare(force);output.show(true);
      vscode.window.showInformationMessage('fosc and fosrun are ready.');
      return prepared;
    } catch (error) { output.appendLine(error.message);vscode.window.showErrorMessage(error.message); }
  }));
  context.subscriptions.push(vscode.commands.registerCommand('fscript.run', async () => {
    if (runPending) return;
    runPending = true;
    const requestId = ++runRequest;
    try {
      const document = await currentScript();
      const build = await compile(document, true);
      if (!build || requestId !== runRequest) return;
      if (!build.layout) throw new Error('fosrun requires an associated layout.ui file.');
      const config = vscode.workspace.getConfiguration('fscript', document.uri);
      const base = vscode.workspace.getWorkspaceFolder(document.uri)?.uri.fsPath || path.dirname(document.uri.fsPath);
      const executable = await tools.resolve('fosrun', config.get('runnerPath', ''), base);
      const sdSetting = config.get('sdRoot', '');
      const sdRoot = sdSetting ? path.resolve(base, sdSetting.startsWith('~/') ? path.join(os.homedir(), sdSetting.slice(2)) : sdSetting) : '';
      if (requestId !== runRequest) return;
      const html = await runner.start(executable, build.image, build.layout.uri.fsPath, { cwd: path.dirname(document.uri.fsPath), sdRoot, wifiOff: config.get('wifiOff', false) });
      const panel = vscode.window.createWebviewPanel('fscript.fosrun', `fosrun: ${build.manifest.name || path.basename(path.dirname(document.uri.fsPath))}`, vscode.ViewColumn.Beside, { enableScripts: true, retainContextWhenHidden: true, localResourceRoots: [] });
      runner.panel = panel;
      panel.webview.onDidReceiveMessage(async message => { const response = await runner.handleMessage(message);if (response && runner.panel === panel) await panel.webview.postMessage(response); });
      panel.onDidDispose(() => { if (runner.panel === panel) runner.stop(); });
      runner.onExit = () => { if (runner.panel === panel) panel.webview.html = '<html><body><p>fosrun has stopped. Use Run App to start the preview again.</p></body></html>'; };
      panel.webview.html = webviewHtml(html);
    } catch (error) { runner.stop();if (requestId !== runRequest) return;output.appendLine(error.message);vscode.window.showErrorMessage(error.message); }
    finally { runPending = false; }
  }));
  context.subscriptions.push(vscode.commands.registerCommand('fscript.stop', () => { runRequest++;runner.stop(); }));
  context.subscriptions.push(vscode.commands.registerCommand('fscript.showOutput', () => output.show(true)));
  context.subscriptions.push(vscode.commands.registerCommand('fscript.openLayout', async () => {
    try {
      const script = await currentScript(), { layout } = await appFiles(script);
      if (!layout) throw new Error('No associated UI found. Set layout in app.json or place layout.ui beside the .fscript file.');
      await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(layout.uri));
    } catch (error) { vscode.window.showErrorMessage(error.message); }
  }));
}
function deactivate() { runRequest++;checking.clear(); runner?.dispose();tools?.dispose(); }
module.exports = { activate, deactivate, _test: { appFiles, uiObjects, scriptCompletions, uiCompletions, scriptHover, uiHover, signatureHelp, definition, documentSymbols, colors, compile } };
