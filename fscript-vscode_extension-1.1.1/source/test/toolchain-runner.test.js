'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const { Toolchain, execute, resolveSetting } = require('../src/toolchain');
const { Runner, allowedRequest, webviewHtml } = require('../src/runner');

test('tool paths preserve spaces and explicit overrides; legacy PATH remains optional', () => {
 assert.equal(resolveSetting('./my tools/fosc','/workspace'),path.resolve('/workspace','./my tools/fosc'));
 assert.equal(resolveSetting('fosc','/workspace'),'fosc');
 assert.equal(resolveSetting('~/fosc','/workspace'),path.join(os.homedir(),'fosc'));
});
test('runner webview bridges allowed local endpoints and blocks arbitrary requests', () => {
 for(const p of ['/state','/event?name=btn.click','/set?value=50','/keyboard?key=q']) assert.ok(allowedRequest(p));
 for(const p of ['http://example.com','/other','//example.com/state','/state/../file','/state?bad=\n']) assert.ok(!allowedRequest(p));
 const html=webviewHtml('<html><head></head><body><script>fetch("/state")</script></body></html>');
 assert.match(html,/Content-Security-Policy/);assert.match(html,/acquireVsCodeApi/);
 assert.ok(!/<script>/.test(html));assert.equal((html.match(/<script nonce=/g)||[]).length,2);
});
test('webview fetch bridge pairs out-of-order replies and keeps its VS Code API private', async () => {
 const html=webviewHtml('<html><head></head><body></body></html>');const bridge=html.match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
 const sent=[],listeners=[];
 const window={addEventListener:(name,fn)=>listeners.push(fn)};
 const context=vm.createContext({window,acquireVsCodeApi:()=>({postMessage:message=>sent.push(message)}),setTimeout,clearTimeout,Map,Promise,Error,JSON});
 vm.runInContext(bridge,context);
 const a=window.fetch('/state'),b=window.fetch('/event?name=btn.click');
 assert.equal(sent.length,2);assert.ok(!window.api);
 listeners[0]({data:{type:'fosrun-response',id:sent[1].id,status:200,text:'{"b":2}'}});
 listeners[0]({data:{type:'fosrun-response',id:sent[0].id,status:200,text:'{"a":1}'}});
 assert.equal((await (await a).json()).a,1);assert.equal((await (await b).json()).b,2);
 await assert.rejects(()=>window.fetch('https://example.com'),/Invalid runner request/);
});
test('embedded toolchain cache resolves both tools without rebuilding', async t => {
 if(!process.env.TOOLCHAIN_CACHE){t.skip('Set TOOLCHAIN_CACHE for the cached bundled toolchain check.');return;}
 const manager=new Toolchain({extensionPath:path.resolve(__dirname,'..'),storagePath:process.env.TOOLCHAIN_CACHE});
 assert.ok((await manager.resolve('fosc','',process.cwd(),true)).endsWith('fosc'));
 assert.ok((await manager.resolve('fosrun','',process.cwd(),true)).endsWith('fosrun'));
});
test('fosrun executes newly compiled events through the same webview message interface', async t => {
 if(!process.env.FOSC_PATH||!process.env.FOSRUN_PATH){t.skip('Set FOSC_PATH and FOSRUN_PATH for the native integration test.');return;}
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'fosrun app ')),logs=[];const runner=new Runner(message=>logs.push(message));
 try{
  const source='var count = 0\non app.start\n lbl.text = text(count)\nend\non btn.click\n count = count + 1\n lbl.text = text(count)\nend\non app.timer\n count = count + 10\n lbl.text = text(count)\nend\n';
  const script=path.join(root,'main.fscript'),layout=path.join(root,'layout.ui'),image=path.join(root,'main.fapp');
  await fs.writeFile(script,source);await fs.writeFile(layout,'type=label;id=lbl;text=0\ntype=button;id=btn;text=Add');
  await execute(process.env.FOSC_PATH,['build',script,'--ui',layout,'-o',image]);
  const html=await runner.start(process.env.FOSRUN_PATH,image,layout,{cwd:root});
  assert.match(html,/<title>fosrun UI<\/title>/);
  let result=await runner.handleMessage({type:'fosrun-request',id:1,path:'/event?name=btn.click'});
  assert.equal(result.status,200);let state=JSON.parse(result.text);assert.equal(state.state.objects[0].text,'1');
  result=await runner.handleMessage({type:'fosrun-request',id:2,path:'/event?name=app.timer'});
  assert.equal(JSON.parse(result.text).state.objects[0].text,'11');
  result=await runner.handleMessage({type:'fosrun-request',id:3,path:'/state'});
  assert.equal(JSON.parse(result.text).objects[0].text,'11');
  assert.ok((await runner.handleMessage({type:'fosrun-request',id:4,path:'/other'})).error);
  const child=runner.child;runner.stop();assert.equal(runner.child,null);
  await new Promise(resolve=>child.exitCode!==null||child.signalCode!==null?resolve():child.once('exit',resolve));
  assert.ok((await runner.handleMessage({type:'fosrun-request',id:5,path:'/state'})).error);
 }finally{runner.stop();await fs.rm(root,{recursive:true,force:true});}
});

test('runner app.timer repeats automatically at the fScript-selected interval', async t => {
 if(!process.env.FOSC_PATH||!process.env.FOSRUN_PATH){t.skip('Set native tool paths.');return;}
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'fosrun timer ')),runner=new Runner();
 try {
  const script=path.join(root,'main.fscript'),layout=path.join(root,'layout.ui'),image=path.join(root,'main.fapp');
  await fs.writeFile(script,'var count = 0\non app.start\n timer.start(100)\nend\non app.timer\n count = count + 1\n lbl.text = text(count)\nend\n');
  await fs.writeFile(layout,'type=label;id=lbl;text=0');
  await execute(process.env.FOSC_PATH,['build',script,'--ui',layout,'-o',image]);
  await runner.start(process.env.FOSRUN_PATH,image,layout,{cwd:root});
  await new Promise(resolve=>setTimeout(resolve,350));
  const result=await runner.handleMessage({type:'fosrun-request',id:10,path:'/state'});
  assert.ok(Number(JSON.parse(result.text).objects[0].text)>=2);
 } finally { runner.stop();await fs.rm(root,{recursive:true,force:true}); }
});

test('first setup uses matching bundled binaries even without a C++ compiler', async t => {
 const extensionPath=path.resolve(__dirname,'..'),platform=process.platform+'-'+process.arch;
 try { await fs.access(path.join(extensionPath,'bin',platform,'source.json')); }catch{t.skip('No binary for this host; local source build is used.');return;}
 const root=await fs.mkdtemp(path.join(os.tmpdir(),'fscript install '));
 try {
  const manager=new Toolchain({extensionPath,storagePath:root,cxx:'/missing/compiler'});
  const tools=await manager.prepare();assert.ok(await manager.usable(tools.fosc));assert.ok(await manager.usable(tools.fosrun));
 } finally { await fs.rm(root,{recursive:true,force:true}); }
});
