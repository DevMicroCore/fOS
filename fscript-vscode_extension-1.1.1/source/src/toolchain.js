'use strict';
const fs = require('node:fs/promises');
const fsSync = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const { execFile } = require('node:child_process');

function execute(executable, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = execFile(executable, args, { timeout: 180000, maxBuffer: 4 * 1024 * 1024, windowsHide: true, ...options }, (error, stdout, stderr) => {
      if (error) { error.output = stdout + stderr; reject(error); }
      else resolve({ stdout, stderr });
    });
    if (options.onChild) options.onChild(child);
  });
}
function resolveSetting(value, base) {
  if (value.startsWith('~/')) value = path.join(os.homedir(), value.slice(2));
  if (!path.isAbsolute(value) && /[\\/]/.test(value)) return path.resolve(base, value);
  return value;
}
async function sourceHash(root) {
  const hash = crypto.createHash('sha256');
  async function visit(directory) {
    for (const name of (await fs.readdir(directory)).sort()) {
      const file = path.join(directory, name);
      if ((await fs.stat(file)).isDirectory()) await visit(file);
      else { hash.update(path.relative(root, file)); hash.update(await fs.readFile(file)); }
    }
  }
  await visit(root);return hash.digest('hex').slice(0, 16);
}
class Toolchain {
  constructor(options) {
    this.extensionPath = options.extensionPath;
    this.storagePath = options.storagePath;
    this.platform = options.platform || process.platform;
    this.arch = options.arch || process.arch;
    this.log = options.log || (() => {});
    this.progress = options.progress || (async work => work(() => {}));
    this.cxx = options.cxx || '';
    this.pending = null;
    this.location = null;
    this.child = null;
  }
  async usable(executable) {
    try { await execute(executable, ['--help'], { timeout: 10000 }); return true; } catch { return false; }
  }
  async resolve(name, setting = '', base = process.cwd(), automatic = false) {
    if (!['fosc', 'fosrun'].includes(name)) throw new Error('Unknown tool.');
    if (setting && setting !== name) {
      const executable = resolveSetting(setting, base);
      if (!await this.usable(executable)) throw new Error(`${name} cannot be executed at ${executable}.`);
      return executable;
    }
    // "fosc" was the 1.0 default; retain existing PATH setups during upgrade.
    if (setting === name && await this.usable(name)) return name;
    const suffix = this.platform === 'win32' ? '.exe' : '';
    const cached = await this.cachePath();
    if (await this.usable(path.join(cached, name + suffix))) return path.join(cached, name + suffix);
    if (automatic) throw new Error('Set up the toolchain once with "fScript: Prepare Tools (fosc + fosrun)".');
    const tools = await this.prepare();return tools[name];
  }
  async cachePath() {
    if (!this.location) {
      const version = await sourceHash(path.join(this.extensionPath, 'toolchain'));
      this.fingerprint = version;
      this.location = path.join(this.storagePath, 'tools', `${this.platform}-${this.arch}-${version}`);
    }
    return this.location;
  }
  async prepare(force = false) {
    if (this.pending) return this.pending;
    this.pending = this.progress(report => this.build(force, report)).finally(() => { this.pending = null; this.child = null; });
    return this.pending;
  }
  async build(force, report) {
    const cache = await this.cachePath(), suffix = this.platform === 'win32' ? '.exe' : '';
    const tools = { fosc: path.join(cache, 'fosc' + suffix), fosrun: path.join(cache, 'fosrun' + suffix) };
    if (!force && await this.usable(tools.fosc) && await this.usable(tools.fosrun)) return tools;
    await fs.mkdir(path.dirname(cache), { recursive: true });
    const stage = await fs.mkdtemp(path.join(path.dirname(cache), '.build-'));
    try {
      const prebuilt = path.join(this.extensionPath, 'bin', `${this.platform}-${this.arch}`);
      let metadata;
      try { metadata = JSON.parse(await fs.readFile(path.join(prebuilt, 'source.json'), 'utf8')); } catch { /* no matching prebuilt */ }
      if (!force && metadata?.sourceHash === this.fingerprint && fsSync.existsSync(path.join(prebuilt, 'fosc' + suffix)) && fsSync.existsSync(path.join(prebuilt, 'fosrun' + suffix))) {
        report('Preparing bundled tools');
        for (const name of ['fosc', 'fosrun']) { await fs.copyFile(path.join(prebuilt, name + suffix), path.join(stage, name + suffix)); if (this.platform !== 'win32') await fs.chmod(path.join(stage, name + suffix), 0o700); }
      }
      if (force || !await this.usable(path.join(stage, 'fosc' + suffix)) || !await this.usable(path.join(stage, 'fosrun' + suffix))) {
        const candidates = this.cxx ? [this.cxx] : ['c++', 'g++', 'clang++'];
        let cxx;
        for (const candidate of candidates) try { await execute(candidate, ['--version'], { timeout: 10000 }); cxx = candidate; break; } catch { /* try next */ }
        if (!cxx) throw new Error(this.platform === 'darwin' ? 'No C++ compiler found. On macOS, install Xcode Command Line Tools: xcode-select --install' : 'No C++17 compiler found. Install C++ tools or set fscript.cxxPath, fscript.compilerPath, and fscript.runnerPath as needed. Windows requires MinGW-w64.');
        const source = path.join(this.extensionPath, 'toolchain'), recipe = JSON.parse(await fs.readFile(path.join(source, 'build.json'), 'utf8'));
        const cwd = path.join(source, recipe.root);
        const common = recipe.common.map(file => path.join(cwd, file));
        const flags = ['-std=c++17', '-O2', '-I' + path.join(cwd, 'tests/arduino_stubs'), '-I' + cwd, '-I' + path.resolve(cwd, '../../fOS4.0')];
        for (const name of ['fosc', 'fosrun']) {
          report(`Compiling ${name} for ${this.platform}/${this.arch}`);
          this.log(`Compiler: ${cxx}; tool: ${name}`);
          const sources = name === 'fosc' ? [path.join(cwd, recipe.compiler), ...common] : [path.join(cwd, recipe.runner), ...common, ...recipe.runtime.map(file => path.resolve(cwd, file))];
          const args = [...flags, ...sources, '-o', path.join(stage, name + suffix)];
          if (name === 'fosrun' && this.platform === 'win32') args.push('-lws2_32');
          try { const result = await execute(cxx, args, { cwd, onChild: child => { this.child = child; } }); if (result.stderr) this.log(result.stderr); }
          catch (error) { this.log(error.output || error.message); throw new Error(`Could not build ${name}. See the "fScript / fosc" output channel.${this.platform === 'darwin' ? ' Xcode Command Line Tools: xcode-select --install.' : ''}`); }
        }
      }
      if (!await this.usable(path.join(stage, 'fosc' + suffix)) || !await this.usable(path.join(stage, 'fosrun' + suffix))) throw new Error('The built tools cannot be executed.');
      await fs.rm(cache, { recursive: true, force: true });
      await fs.rename(stage, cache);
      this.log(`Tools ready: ${cache}`);
      return tools;
    } finally { await fs.rm(stage, { recursive: true, force: true }); }
  }
  dispose() { this.child?.kill(); }
}
module.exports = { Toolchain, execute, sourceHash, resolveSetting };
