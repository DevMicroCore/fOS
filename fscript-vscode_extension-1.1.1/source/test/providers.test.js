'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const posix = path.posix;
const documents = [], virtualFiles = new Map(), providers = [], commands = new Map(), messages = [];
let compilerPath = 'fosc';
class Position { constructor(line, character) { this.line = line; this.character = character; } translate(l = 0, c = 0) { return new Position(this.line + l, this.character + c); } }
class Range { constructor(a,b,c,d) { this.start = typeof a === 'number' ? new Position(a,b) : a; this.end = typeof a === 'number' ? new Position(c,d) : b; } }
class Uri {
  constructor(p, scheme='file') { this.path=p;this.fsPath=p;this.scheme=scheme; }
  toString() { return `${this.scheme}://${this.path}`; }
  static file(p) { return new Uri(p); }
  static joinPath(u,...parts) { return new Uri(posix.join(u.path,...parts),u.scheme); }
}
class Document {
  constructor(filename,text,languageId) { this.uri=Uri.file(filename);this.text=text;this.languageId=languageId;this.isDirty=false; }
  getText(r) { return r ? this.text.slice(this.offsetAt(r.start),this.offsetAt(r.end)) : this.text; }
  lineAt(n) { return { text:this.text.split('\n')[n] }; }
  positionAt(n) { const before=this.text.slice(0,n).split('\n');return new Position(before.length-1,before.at(-1).length); }
  offsetAt(p) { return this.text.split('\n').slice(0,p.line).reduce((sum,line)=>sum+line.length+1,0)+p.character; }
  getWordRangeAtPosition(p,regex=/\w+/) {
    const line=this.lineAt(p.line).text;
    for(const match of line.matchAll(new RegExp(regex.source,'g'))) if(match.index<=p.character&&match.index+match[0].length>=p.character) return new Range(p.line,match.index,p.line,match.index+match[0].length);
  }
  async save() { this.isDirty=false;return true; }
}
class MarkdownString { constructor(){this.value='';} appendText(s){this.value+=s;return this;} appendMarkdown(s){this.value+=s;return this;} appendCodeblock(s){this.value+=s;return this;} }
class Basic { constructor(...args){this.args=args;this.label=args[0];} }
class Collection { constructor(){this.data=new Map();} set(uri,values){this.data.set(uri.toString(),values);}delete(uri){this.data.delete(uri.toString());}dispose(){} }
const panels=[];
function createPanel(type,title,column,options){
 const panel={type,title,column,options,disposed:false,webview:{html:'',responses:[],onDidReceiveMessage(fn){this.handler=fn;return {dispose(){}};},async postMessage(message){this.responses.push(message);return true;}},onDidDispose(fn){this.disposeHandler=fn;return {dispose(){}};},dispose(){if(!this.disposed){this.disposed=true;this.disposeHandler?.();}}};
 panels.push(panel);return panel;
}
const disposable=()=>({dispose(){}});
const mock={
 Position,Range,Uri,MarkdownString,CompletionItem:Basic,SnippetString:class{constructor(v){this.value=v;}},Hover:Basic,Location:Basic,DocumentSymbol:Basic,FoldingRange:Basic,
 SignatureInformation:Basic,ParameterInformation:Basic,SignatureHelp:class{},Diagnostic:Basic,ColorInformation:Basic,Color:Basic,ColorPresentation:Basic,
 CompletionItemKind:{Method:1,Property:2,Variable:3,Function:4,Keyword:5,Value:6},SymbolKind:{Object:1,Function:2,Variable:3},DiagnosticSeverity:{Error:1,Warning:2},TextEdit:{replace:(range,text)=>({range,newText:text})},
 workspace:{isTrusted:true,textDocuments:documents,fs:{readFile:async uri=>{if(virtualFiles.has(uri.path))return Buffer.from(virtualFiles.get(uri.path));return fs.promises.readFile(uri.fsPath);}},
  getConfiguration:()=>({get:(key,def)=>key==='compilerPath'?compilerPath:key==='runnerPath'?(process.env.FOSRUN_PATH||def):def}),getWorkspaceFolder:()=>undefined,
  openTextDocument:async uri=>{const existing=documents.find(d=>d.uri.path===uri.path);if(existing)return existing;return new Document(uri.path,(await mock.workspace.fs.readFile(uri)).toString(),uri.path.endsWith('.ui')?'fscript-ui':'fscript');},
  onDidOpenTextDocument:disposable,onDidChangeTextDocument:disposable,onDidCloseTextDocument:disposable,onDidSaveTextDocument:disposable},
 ViewColumn:{Beside:2},ProgressLocation:{Notification:1},
 window:{createWebviewPanel:createPanel,withProgress:async(options,work)=>work({report(){}}),createOutputChannel:()=>({append(){},appendLine(){},show(){},dispose(){}}),showInformationMessage:(s)=>messages.push(s),showErrorMessage:(s)=>messages.push(s),showTextDocument:async d=>d},
 commands:{registerCommand:(name,fn)=>{commands.set(name,fn);return disposable();}},languages:{createDiagnosticCollection:()=>new Collection()}
};
for(const name of ['registerCompletionItemProvider','registerHoverProvider','registerSignatureHelpProvider','registerDefinitionProvider','registerDocumentSymbolProvider','registerFoldingRangeProvider','registerColorProvider']) mock.languages[name]=(...args)=>{providers.push([name,...args]);return disposable();};
const original=Module._load;
Module._load=function(name,parent,isMain){return name==='vscode'?mock:original.call(this,name,parent,isMain);};
const extension=require('../src/extension');Module._load=original;
extension.activate({subscriptions:[]});
function doc(text, language='fscript', filename='/test app/main.fscript'){const d=new Document(filename,text,language);documents.push(d);return d;}
const layout=doc('type=button;id=btn_play\ntype=slider;id=slider_volume;min=0;max=21\ntype=roller;id=roller_files\ntype=textarea;id=txt_editor','fscript-ui','/test app/screen.ui');
virtualFiles.set('/test app/app.json',JSON.stringify({layout:'screen.ui'}));

test('activation registers both languages, all editing providers and compile commands',()=>{
 assert.ok(providers.some(p=>p[0]==='registerCompletionItemProvider'&&p[1].language==='fscript-ui'));
 assert.ok(providers.some(p=>p[0]==='registerSignatureHelpProvider'));
 assert.ok(commands.has('fscript.build'));
});
test('layout IDs and widget-specific methods are completed from custom app.json layout',async()=>{
 const d=doc('roller_files.lo');const p=d.positionAt(d.text.length);
 const items=await extension._test.scriptCompletions(d,p);
 assert.ok(items.some(i=>i.label==='load_pairs'));
 assert.ok(!items.some(i=>i.label==='set_range'));
 const item=items.find(i=>i.label==='load_dir_filtered');assert.match(item.insertText.value,/extensions|\.mp3/);
 assert.equal(item.range.start.character,13);
});
test('event completions differ from property/method completions',async()=>{
 const d=doc('on slider_volume.re');const items=await extension._test.scriptCompletions(d,d.positionAt(d.text.length));
 assert.ok(items.some(i=>i.label==='released'));assert.ok(!items.some(i=>i.label==='set_range'));
 const app=doc('on app.');assert.deepEqual((await extension._test.scriptCompletions(app,app.positionAt(app.text.length))).map(i=>i.label),['start','close','theme_changed','timer']);
});
test('namespace completion inserts a member without duplicating namespace',async()=>{
 const d=doc('timer.st');const items=await extension._test.scriptCompletions(d,d.positionAt(d.text.length));
 assert.equal(items[0].label,'start');assert.match(items[0].insertText.value,/^start\(/);
});
test('global completion includes current user symbols and returns nothing inside strings',async()=>{
 const d=doc('function custom(x)\nend\nvar value = 1\naudio_');const items=await extension._test.scriptCompletions(d,d.positionAt(d.text.length));
 for(const name of ['audio_stream','btn_play','custom','value']) assert.ok(items.some(i=>i.label===name));
 const s=doc('file_read("audio_');assert.equal((await extension._test.scriptCompletions(s,s.positionAt(s.text.length))).length,0);
});
test('signature help counts UI method arguments inside a string',async()=>{
 const d=doc('roller_files.load_dir_filtered("/music/files", "/music/files", "');const result=await extension._test.signatureHelp(d,d.positionAt(d.text.length));
 assert.equal(result.activeParameter,2);assert.equal(result.signatures[0].parameters.length,3);
});
test('UI completion replaces only an existing key and preserves its value',async()=>{
 const d=doc('type=button;bg=theme','fscript-ui');const items=await extension._test.uiCompletions(d,new Position(0,2));
 const type=items.find(i=>i.label==='type');assert.ok(type);assert.equal(type.insertText,'type');assert.equal(type.range.end.character,4);
});
test('UI values include types, theme colors and textarea targets',async()=>{
 const d=doc('type=keyboard;target=\ntype=textarea;id=txt','fscript-ui');const items=await extension._test.uiCompletions(d,new Position(0,21));assert.deepEqual(items.map(i=>i.label),['txt']);
 const d2=doc('type=button;bg=','fscript-ui');assert.ok((await extension._test.uiCompletions(d2,d2.positionAt(d2.text.length))).some(i=>i.label==='theme'));
});
test('definition lookup and hover resolve an ID into its source layout',async()=>{
 const d=doc('roller_files.text');const result=await extension._test.definition(d,new Position(0,4));assert.equal(result.args[0].path,'/test app/screen.ui');
 const hover=await extension._test.scriptHover(d,new Position(0,14));assert.match(hover.args[0].value,/selected text/);
});
test('UI swatches are returned only for actual color fields, not text/comments',()=>{
 const d=doc('# bg=0xFF0000\ntype=label;id=lbl;text=0xFF0000;fg=0xFF0000','fscript-ui');const result=extension._test.colors(d);
 assert.equal(result.length,1);assert.deepEqual(result[0].args[1].args,[1,0,0,1]);
});
test('compiler commands use real fosc with spaces in paths; trusted workspace required',async t=>{
 if(!process.env.FOSC_PATH){t.skip('Set FOSC_PATH to run the real compiler integration.');return;}
 compilerPath=process.env.FOSC_PATH;
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'fscript app '));
 try{
  const source='on app.start\n  lbl.text = "Hello"\nend\n';
  fs.writeFileSync(path.join(root,'main.fscript'),source);fs.writeFileSync(path.join(root,'layout.ui'),'type=label;id=lbl;text=');
  const d=doc(source,'fscript',path.join(root,'main.fscript'));
  await extension._test.compile(d,false);await extension._test.compile(d,true);
  assert.ok(fs.existsSync(path.join(root,'main.fapp')));
  mock.workspace.isTrusted=false;await assert.rejects(()=>extension._test.compile(d,false),/trusted workspace/);mock.workspace.isTrusted=true;
 }finally{fs.rmSync(root,{recursive:true,force:true});}
});

test('VS Code Start/Stop commands build the app and open/close the runner webview',async t=>{
 if(!process.env.FOSC_PATH||!process.env.FOSRUN_PATH){t.skip('Set native tool paths.');return;}
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'vscode runner '));
 try {
  const source='var n = 0\non app.start\n lbl.text = text(n)\nend\non btn.click\n n = n + 1\n lbl.text = text(n)\nend\n';
  fs.writeFileSync(path.join(root,'main.fscript'),source);fs.writeFileSync(path.join(root,'layout.ui'),'type=label;id=lbl;text=0\ntype=button;id=btn;text=Add');
  const d=doc(source,'fscript',path.join(root,'main.fscript'));mock.window.activeTextEditor={document:d};
  await commands.get('fscript.run')();
  const panel=panels.at(-1);assert.ok(panel);assert.equal(panel.options.enableScripts,true);assert.match(panel.webview.html,/acquireVsCodeApi/);
  await panel.webview.handler({type:'fosrun-request',id:8,path:'/event?name=btn.click'});
  assert.equal(JSON.parse(panel.webview.responses.at(-1).text).state.objects[0].text,'1');
  commands.get('fscript.stop')();assert.ok(panel.disposed);
 }finally{commands.get('fscript.stop')();fs.rmSync(root,{recursive:true,force:true});}
});
