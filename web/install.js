// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 delirioux
// Xebec - FM-1 installer over Web MIDI. Protocol facts from M-VAVE's WebUpdate.js and Felucca's fm1ota.js.

export const PRODUCT = "FM-1_47932320";
export const STOCK_HEAD_SHA256 = "d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67";
export const LOADER_SHA256 = "ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541";
export const STOCK_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a";
const FLASH_LEN = 0x93000;
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const STAGED = 0xE0000000, WRITTEN = 0xF0000000;
const SUCCESS = [...new TextEncoder().encode("success"), 0];

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const le = (p, at, n) => { let v = 0; for (let i = 0; i < n; i++) v += p[at + i] * 2 ** (8 * i); return v; };
const ascii = (p) => String.fromCharCode(...p).replace(/\0.*$/s, "");
export class InstallError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}
const fail = (code, message) => { throw new InstallError(code, message); };

export function crc16(p) {
  let c = 0;
  for (const b of p) {
    c ^= b << 8;
    for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c;
}

function decipher(p, key = 0xFFFF) {
  const out = Uint8Array.from(p);
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
}

async function sha256(p) {
  const d = new Uint8Array(await crypto.subtle.digest("SHA-256", p));
  return Array.from(d, (b) => b.toString(16).padStart(2, "0")).join("");
}

export function pack7(bytes) {
  const out = []; let acc = 0, n = 0;
  for (const b of bytes) {
    acc |= b << n; n += 8;
    while (n >= 7) { out.push(acc & 0x7F); acc >>>= 7; n -= 7; }
  }
  if (n) out.push(acc & 0x7F);
  return out;
}

export function unpack7(bytes) {
  const out = []; let acc = 0, n = 0;
  for (const b of bytes) {
    acc |= (b & 0x7F) << n; n += 7;
    if (n >= 8) { out.push(acc & 0xFF); acc >>>= 8; n -= 8; }
  }
  return out;
}

export function frame(cmd, payload = []) {
  let sum = 0;
  for (const b of payload) sum = (sum + b) & 0xFF;
  const n = payload.length;
  return [0xF0, ...pack7([0x00, 0x59, cmd, n & 0xFF, (n >> 8) & 0xFF, 0, ...payload, ~sum & 0xFF]), 0xF7];
}

export function parse(msg) {
  if (!msg || msg.length < 10 || msg[0] !== 0xF0 || msg[msg.length - 1] !== 0xF7) return null;
  const raw = unpack7(msg.slice(1, -1));
  if (raw.length < 7 || raw[0] !== 0 || raw[1] !== 0x59 || le(raw, 3, 3) !== raw.length - 7) return null;
  let sum = 0;
  for (let i = 6; i < raw.length; i++) sum = (sum + raw[i]) & 0xFF;
  return sum === 0xFF ? { cmd: raw[2], data: Uint8Array.from(raw.slice(6, -1)) } : null;
}

// A .fwsc: the identity, the UFW image the device reads from, and its files.
export async function readPackage(bytes) {
  bytes = new Uint8Array(bytes);
  if (bytes.length < 1024 + 20) fail("package", "This is not an FM-1 package.");
  const image = new Uint8Array(bytes.length - 20);
  let id = "", ended = false;
  for (let i = 0; i < 20; i++) {
    image.set(bytes.subarray(i * 48, i * 48 + 47), i * 47);
    const m = bytes[i * 48 + 47];
    if (m === 125) ended = true;
    else if (!ended) id += String.fromCharCode((m - i - 1) & 0xFF);
  }
  image.set(bytes.subarray(960), 940);
  const head = decipher(image.subarray(0, 64));
  const count = le(head, 8, 2);
  if (le(head, 0, 2) !== crc16(head.subarray(2)) || ascii(head.subarray(16, 24)) !== "AC791N" ||
      count < 1 || count > 12 || crc16(image.subarray(64, 64 + 80 * count)) !== le(head, 2, 2))
    fail("package", "This is not an FM-1 package (its head does not check out).");
  const files = {};
  for (let i = 0; i < count; i++) {
    const e = decipher(image.subarray(64 + 80 * i, 64 + 80 * (i + 1)));
    const type = le(e, 0, 2), off = le(e, 8, 4), len = le(e, 12, 4);
    if (off + len > image.length) fail("package", "The package is cut short.");
    const data = image.subarray(off, off + len);
    if ((type === 0 || type === 100) && crc16(data) !== le(e, 4, 2)) fail("package", `The package's ${ascii(e.subarray(64))} is damaged.`);
    files[type] = data;
  }
  const flash = files[0], loader = files[100];
  if (!flash || !loader) fail("package", "The package has no flash image or no loader.");
  return {
    id, image, sha256: await sha256(bytes),
    flashLen: flash.length,
    loaderLen: loader.length,
    headSha256: await sha256(flash.subarray(0, 0x4000)),
    loaderSha256: await sha256(loader),
  };
}

// Only packages that keep M-VAVE's boot head and loader go to an FM-1 from this page.
export function checkPackage(pkg, expectSha256) {
  if (expectSha256 && pkg.sha256 !== expectSha256) fail("package", "The package does not match the one this page was made for.");
  if (pkg.flashLen !== FLASH_LEN) fail("package", "The package's flash image is not the FM-1's size.");
  if (pkg.headSha256 !== STOCK_HEAD_SHA256)
    fail("package", "The package would replace M-VAVE's boot loader on the FM-1. This page only installs packages that keep it.");
  if (pkg.loaderSha256 !== LOADER_SHA256) fail("package", "The package does not carry M-VAVE's update loader.");
}

// One MIDI in/out pair, with the device's SysEx messages queued.
export class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.queue = []; this.wake = null;
    this.listener = (ev) => {
      const p = parse(Array.from(ev.data));
      if (!p) return;
      if (this.queue.length < 64) this.queue.push(p);
      if (this.wake) this.wake();
    };
    input.addEventListener("midimessage", this.listener);
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  send(bytes) { if (this.lost) fail("lost", "The FM-1 was disconnected."); this.output.send(bytes); }
  async read(ms) {
    if (!this.queue.length && !this.lost) {
      let timer;
      await new Promise((r) => { this.wake = r; timer = setTimeout(r, ms); });
      clearTimeout(timer); this.wake = null;
    }
    return this.queue.shift() || null;
  }
  async identity(ms = 1000) {
    this.queue.length = 0;
    this.send(frame(0x11));
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const p = await this.read(end - Date.now());
      if (p && p.cmd === 0x11 && p.data.length === 27) return ascii(p.data);
      if (this.lost) return null;
    }
    return null;
  }
  close() { this.input.removeEventListener("midimessage", this.listener); }
}

// The device asks for parts of the package (cmd 0x30: flags, address, length) until it asks
// for `finish`; that one is answered "success". Returns the bytes served.
export async function serve(link, image, finish, idleMs, progress = () => {}) {
  let last = Date.now(), served = 0;
  for (;;) {
    const p = await link.read(250);
    if (!p) {
      if (link.lost) fail("lost", "The FM-1 was disconnected.");
      if (Date.now() - last > idleMs) fail("stopped", "The FM-1 stopped asking for the package.");
      continue;
    }
    if (p.cmd !== 0x30 || p.data.length !== 8) continue;
    last = Date.now();
    const addr = le(p.data, 1, 4), len = le(p.data, 5, 3), req = Array.from(p.data);
    if (addr === STAGED || addr === WRITTEN) {
      if (addr !== finish) fail("phase", "The FM-1 is in another step of the update than this page.");
      link.send(frame(0x30, [...req, ...SUCCESS]));
      const end = Date.now() + 600;        // repeated finish requests get the same answer
      while (Date.now() < end && !link.lost) {
        const again = await link.read(100);
        if (again && again.cmd === 0x30 && le(again.data, 1, 4) === finish) link.send(frame(0x30, [...Array.from(again.data), ...SUCCESS]));
      }
      return served;
    }
    if (len < 1 || len > 512 || addr + len > image.length) fail("request", "The FM-1 asked for a part outside the package.");
    link.send(frame(0x30, [...req, ...image.subarray(addr, addr + len)]));
    served += len;
    progress(served);
  }
}

const NAME = /fm.?1|ota|felucca|composite|usb.?midi/i;   // never probe other MIDI gear

// The FM-1's MIDI port pair answering with an identity that `want` accepts.
export async function findDevice(access, want) {
  const outputs = [...access.outputs.values()];
  for (const input of access.inputs.values()) {
    if (input.state === "disconnected" || !NAME.test(input.name || "")) continue;
    const output = outputs.find((o) => o.name === input.name && o.state !== "disconnected");
    if (!output) continue;
    try { await input.open(); await output.open(); } catch { continue; }
    const link = new Link(input, output);
    // Chrome on Linux can still be connecting the input when open() resolves, and the FM-1
    // answers within a few ms: give it a moment, and ask again if the first answer is missed.
    await sleep(200);
    let id = null;
    for (let i = 0; i < 3 && !id && !link.lost; i++) id = await link.identity(500);
    if (id && want(id)) return { link, id };
    link.close();
  }
  return null;
}

async function waitDevice(access, want, ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const d = await findDevice(access, want);
    if (d) return d;
    await sleep(1000);
  }
  return null;
}

const isLoader = (id) => /^ota-/i.test(id);

// The whole install. From a running firmware: it stages the loader (step 1), then the loader
// writes the package (step 2). From the loader (update mode, or an install cut off): step 2 only.
export async function install(access, pkg, step = () => {}) {
  const dev = await findDevice(access, (id) => /^FM-1_/.test(id) || isLoader(id));
  if (!dev) fail("notfound", "No FM-1 found. Check the USB cable, switch it on, and close other MIDI apps.");
  let loader = dev;
  if (dev.id === PRODUCT) {
    dev.link.close();
    fail("xebec", "Xebec is running. It cannot take an update over USB yet: switch the FM-1 off, hold OCT- and OCT+ while switching it on, then press Install again.");
  }
  if (!isLoader(dev.id)) {
    step("staging", dev.id);
    dev.link.send(UPGRADE);
    const stageBytes = 1024 + 0x4000 + pkg.loaderLen;      // UFW head, flash head, loader
    try { await serve(dev.link, pkg.image, STAGED, 10000, (n) => step("staging", dev.id, 0.1 * Math.min(n / stageBytes, 1))); }
    catch (e) {
      if (e.code === "stopped") fail("refused", `${dev.id} stopped before staging the loader: it refused the package, and nothing was written. Its screen shows why.`);
      throw e;
    } finally { dev.link.close(); }
    step("restart");
    await sleep(3000);
    loader = await waitDevice(access, isLoader, 40000);
    if (!loader) fail("noloader", "The update loader did not appear. Unplug and replug the USB cable, then press Install again: the FM-1 stays in update mode until it is done.");
  }
  step("writing", loader.id);
  loader.link.send(UPGRADE);
  const writeBytes = 1024 + pkg.flashLen;                    // UFW head, flash image
  try { await serve(loader.link, pkg.image, WRITTEN, 180000, (n) => step("writing", loader.id, 0.1 + 0.89 * Math.min(n / writeBytes, 1))); }
  finally { loader.link.close(); }
  if (pkg.id !== PRODUCT) { step("done"); return; }
  step("starting");
  await sleep(3000);
  const back = await waitDevice(access, (id) => id === PRODUCT, 40000);
  if (back) { back.link.close(); step("running", back.id); }
  else step("unconfirmed");
}
