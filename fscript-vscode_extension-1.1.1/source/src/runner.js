'use strict';
const http = require('node:http');
const net = require('node:net');
const crypto = require('node:crypto');
const { spawn } = require('node:child_process');

function request(port, pathname = '/', timeout = 2000) {
  return new Promise((resolve, reject) => {
    const req = http.get({ hostname: '127.0.0.1', port, path: pathname, timeout }, response => {
      let data = '', bytes = 0;
      response.setEncoding('utf8');
      response.on('data', chunk => { bytes += Buffer.byteLength(chunk); if (bytes > 2 * 1024 * 1024) { req.destroy(new Error('Runner response is too large.')); return; } data += chunk; });
      response.on('end', () => resolve({ status: response.statusCode, text: data }));
      response.on('error', reject);
    });
    req.on('timeout', () => req.destroy(new Error('Runner is not responding.')));
    req.on('error', reject);
  });
}
function freePort() {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.on('error', reject);
    server.listen(0, '127.0.0.1', () => { const port = server.address().port; server.close(error => error ? reject(error) : resolve(port)); });
  });
}
function allowedRequest(pathname) {
  return typeof pathname === 'string' && pathname.length <= 8190 && /^\/(?:state|event|set|keyboard)(?:\?|$)/.test(pathname) && !/[\r\n]/.test(pathname);
}
function webviewHtml(html) {
  const nonce = crypto.randomBytes(18).toString('base64');
  const bridge = `
(() => {
  const api = acquireVsCodeApi();
  const pending = new Map(); let sequence = 0;
  window.addEventListener('message', event => {
    const m = event.data;
    if (!m || m.type !== 'fosrun-response' || !pending.has(m.id)) return;
    const p = pending.get(m.id); pending.delete(m.id); clearTimeout(p.timer);
    if (m.error) p.reject(new Error(m.error));
    else p.resolve({ ok: m.status >= 200 && m.status < 300, status: m.status, json: async () => JSON.parse(m.text), text: async () => m.text });
  });
  window.fetch = url => new Promise((resolve, reject) => {
    if (typeof url !== 'string' || !/^\\/(?:state|event|set|keyboard)(?:\\?|$)/.test(url)) { reject(new Error('Invalid runner request')); return; }
    const id = ++sequence;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error('Runner is not responding')); }, 10000);
    pending.set(id, { resolve, reject, timer });
    api.postMessage({ type: 'fosrun-request', id, path: url });
  });
})();`;
  const policy = `<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'nonce-${nonce}'; img-src data:;">`;
  return html.replace('<head>', '<head>' + policy).replace(/<script>/g, `<script nonce="${nonce}">`).replace('<body>', `<body><script nonce="${nonce}">${bridge}</script>`);
}
class Runner {
  constructor(log = () => {}) { this.log = log; this.child = null; this.port = null; this.generation = 0; this.panel = null; }
  async start(executable, image, layout, options = {}) {
    this.stop(); const generation = this.generation;
    const port = await freePort();
    if (generation !== this.generation) throw new Error('Runner startup cancelled.');
    const args = [image, '--ui', layout, '--serve', '--port', String(port), '--trace-native'];
    if (options.sdRoot) args.push('--sd-root', options.sdRoot);
    if (options.wifiOff) args.push('--wifi-off');
    const child = spawn(executable, args, { cwd: options.cwd, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
    this.child = child;this.port = port;
    let error = null;
    child.stdout.on('data', data => this.log(data.toString()));
    child.stderr.on('data', data => this.log(data.toString()));
    child.on('error', e => { error = e; });
    child.on('exit', (code, signal) => {
      if (this.child === child) { this.child = null;this.log(`fosrun exited (${signal || code}).`);this.onExit?.(code, signal); }
    });
    for (let i = 0; i < 100; i++) {
      if (generation !== this.generation) throw new Error('Runner startup cancelled.');
      if (error) throw error;
      if (child.exitCode !== null || child.signalCode !== null) throw new Error(`fosrun exited during startup (${child.exitCode ?? child.signalCode}). See Output.`);
      try { const response = await request(port, '/', 250); if (response.status === 200) return response.text; } catch { /* wait for bind */ }
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    this.stop();throw new Error('Could not start the fosrun preview.');
  }
  async handleMessage(message) {
    if (!message || message.type !== 'fosrun-request' || !Number.isSafeInteger(message.id)) return null;
    if (!allowedRequest(message.path)) return { type: 'fosrun-response', id: message.id, error: 'Invalid runner request.' };
    try {
      if (!this.child || this.port === null) throw new Error('Runner has stopped.');
      const response = await request(this.port, message.path);
      return { type: 'fosrun-response', id: message.id, status: response.status, text: response.text };
    } catch (error) { return { type: 'fosrun-response', id: message.id, error: error.message }; }
  }
  stop() {
    this.generation++;
    const child = this.child;this.child = null;this.port = null;
    const panel = this.panel;this.panel = null;
    panel?.dispose();
    if (child && child.exitCode === null && child.signalCode === null) {
      child.kill();
      const timer = setTimeout(() => { if (child.exitCode === null && child.signalCode === null) child.kill('SIGKILL'); }, 1000);
      timer.unref();
    }
  }
  dispose() { this.stop(); }
}
module.exports = { Runner, request, freePort, webviewHtml, allowedRequest };
