#!/usr/bin/env node
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const os = require('node:os');
const { Toolchain, sourceHash } = require('../src/toolchain');
(async () => {
 const root = path.resolve(__dirname, '..'), cache = await fs.mkdtemp(path.join(os.tmpdir(), 'fscript-tool-build-'));
 try {
  const manager = new Toolchain({ extensionPath: root, storagePath: cache, cxx: process.argv[2] || '', log: console.log, progress: async work => work(console.log) });
  const tools = await manager.prepare(true), dir = path.join(root, 'bin', process.platform + '-' + process.arch);
  await fs.mkdir(dir, { recursive: true });
  for (const [name, file] of Object.entries(tools)) { const output = path.join(dir, path.basename(file));await fs.copyFile(file, output);if (process.platform !== 'win32') await fs.chmod(output, 0o755);console.log(name, output); }
  await fs.writeFile(path.join(dir, 'source.json'), JSON.stringify({ sourceHash: await sourceHash(path.join(root, 'toolchain')), platform: process.platform, arch: process.arch }, null, 2) + '\n');
 } finally { await fs.rm(cache, { recursive: true, force: true }); }
})().catch(error => { console.error(error.output || error.message);process.exitCode = 1; });
