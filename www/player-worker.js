const latin1Decoder = new TextDecoder("latin1");
const utf8Decoder = new TextDecoder();

const VM_OK = 0;
const VM_BLOCKED = 1;
const VM_HALTED = 2;
const VM_ERROR = 3;
const DEFAULT_STORY_URL = "./aa-14.ulx";
const HOST_INPUT_BLOCKED = 0xffffffff;
const GLK_WINDOW_TEXT_BUFFER = 3;
const GLK_WINDOW_TEXT_GRID = 4;
const GLK_STYLE_INPUT = 8;
const GLK_STYLE_NAMES = [
  "normal",
  "emphasized",
  "preformatted",
  "header",
  "subheader",
  "alert",
  "note",
  "block-quote",
  "input",
  "user1",
  "user2",
];
const GLK_STYLEHINT_NAMES = [
  "indentation",
  "para-indentation",
  "justification",
  "size",
  "weight",
  "oblique",
  "proportional",
  "text-color",
  "back-color",
  "reverse-color",
];

class WasiPreview1 {
  memory;

  setMemory(memory) {
    this.memory = memory;
  }

  view() {
    if (!this.memory) throw new Error("WASI memory is not initialized");
    return new DataView(this.memory.buffer);
  }

  bytes(ptr, len) {
    if (!this.memory) throw new Error("WASI memory is not initialized");
    return new Uint8Array(this.memory.buffer, ptr, len);
  }

  exports() {
    return {
      environ_get: () => 0,
      environ_sizes_get: (_count, size) => {
        this.view().setUint32(size, 0, true);
        return 0;
      },
      fd_close: () => 0,
      fd_seek: (_fd, _offset, _whence, newOffset) => {
        this.view().setBigUint64(newOffset, 0n, true);
        return 0;
      },
      fd_write: (fd, iovs, iovsLen, nwritten) => {
        const view = this.view();
        let written = 0;
        let text = "";
        for (let index = 0; index < iovsLen; ++index) {
          const iov = iovs + index * 8;
          const ptr = view.getUint32(iov, true);
          const len = view.getUint32(iov + 4, true);
          text += utf8Decoder.decode(this.bytes(ptr, len));
          written += len;
        }
        const tag = fd === 2 ? "wasm stderr" : `wasm fd${fd}`;
        console.log(`[${tag}] ${text.replace(/\n$/, "")}`);
        view.setUint32(nwritten, written, true);
        return 0;
      },
      proc_exit: (code) => {
        throw new Error(`wasm proc_exit(${code})`);
      },
      random_get: (ptr, len) => {
        crypto.getRandomValues(this.bytes(ptr, len));
        return 0;
      },
    };
  }
}

class WorkerGlkHost {
  exports;
  windows = new Map();
  windowSizes = new Map();
  queuedLines = new Map();
  styleHints = new Map();
  pendingLineWindow;
  arrangePending = false;
  operations = [];

  setExports(exports) {
    this.exports = exports;
  }

  memory() {
    if (!this.exports) throw new Error("host memory is not initialized");
    return this.exports.memory;
  }

  bytes(ptr, len) {
    return new Uint8Array(this.memory().buffer, ptr, len);
  }

  openWindow(window, stream, split, method, size, type, rock) {
    this.windows.set(window, {
      stream,
      split,
      method,
      size,
      glkType: type,
      rock,
      cursor: 0,
      text: "",
      pendingRuns: [],
      dirty: true,
      clearPending: false,
      cursorX: 0,
      cursorY: 0,
      cursorDirty: false,
    });
    const styleHints = this.styleHintsForWindowType(type);
    this.operations.push({
      type: "openWindow",
      window,
      stream,
      split,
      method,
      size,
      glkType: type,
      rock,
      styleHints,
    });
  }

  stylehintSet(windowType, style, hint, value) {
    const windowTypeHints = this.styleHints.get(windowType) ?? new Map();
    const styleHints = windowTypeHints.get(style) ?? new Map();
    styleHints.set(hint, value);
    windowTypeHints.set(style, styleHints);
    this.styleHints.set(windowType, windowTypeHints);
  }

  styleHintsForWindowType(windowType) {
    const windowTypeHints = this.styleHints.get(windowType);
    if (!windowTypeHints) return [];
    const hints = [];
    for (const [style, styleHints] of windowTypeHints) {
      for (const [hint, value] of styleHints) {
        hints.push({
          style,
          styleName: glkStyleName(style),
          hint,
          hintName: glkStyleHintName(hint),
          value,
          signedValue: value | 0,
          hexValue: `0x${value.toString(16).padStart(8, "0")}`,
        });
      }
    }
    return hints;
  }

  windowWidth(window) {
    return this.windowSizes.get(window)?.width ?? 80;
  }

  windowHeight(window) {
    return this.windowSizes.get(window)?.height ?? 24;
  }

  pollArrange() {
    if (!this.arrangePending) return 0;
    this.arrangePending = false;
    return 1;
  }

  updateWindowSizes(sizes) {
    let changed = false;
    for (const size of sizes) {
      const previous = this.windowSizes.get(size.window);
      if (
        !previous ||
        previous.width !== size.width ||
        previous.height !== size.height
      ) {
        this.windowSizes.set(size.window, {
          width: size.width,
          height: size.height,
        });
        changed = true;
      }
    }
    if (changed) this.arrangePending = true;
  }

  clearWindow(window) {
    const record = this.windows.get(window);
    if (!record) return;
    record.text = "";
    record.cursor = 0;
    record.pendingRuns = [];
    record.clearPending = true;
    record.dirty = true;
  }

  moveCursor(window, x, y) {
    const record = this.windows.get(window);
    if (!record) return;
    record.cursor = y * this.windowWidth(window) + x;
    record.cursorX = x;
    record.cursorY = y;
    record.cursorDirty = true;
  }

  writeLatin1(window, _stream, style, ptr, length) {
    const text = latin1Decoder.decode(this.bytes(ptr, length));
    console.log("host.writeLatin1", { window, style, length, text });
    this.writeWindowText(window, text, style);
  }

  writeUnicode(window, _stream, style, ptr, length) {
    const view = new DataView(this.memory().buffer);
    const codepoints = [];
    for (let index = 0; index < length; ++index) {
      codepoints.push(view.getUint32(ptr + index * 4, true));
    }
    const text = String.fromCodePoint(...codepoints);
    console.log("host.writeUnicode", { window, style, length, text });
    this.writeWindowText(window, text, style);
  }

  readLineLatin1(window, ptr, maxLength) {
    const line = this.shiftLine(window);
    if (line === undefined) return HOST_INPUT_BLOCKED;
    const encoded = latin1Encode(line);
    const count = Math.min(maxLength, encoded.length);
    this.bytes(ptr, count).set(encoded.subarray(0, count));
    return count;
  }

  readLineUnicode(window, ptr, maxLength) {
    const line = this.shiftLine(window);
    if (line === undefined) return HOST_INPUT_BLOCKED;
    const view = new DataView(this.memory().buffer);
    const chars = Array.from(line);
    const count = Math.min(maxLength, chars.length);
    for (let index = 0; index < count; ++index) {
      view.setUint32(ptr + index * 4, chars[index].codePointAt(0) ?? 0, true);
    }
    return count;
  }

  submitLine(window, text) {
    const lines = this.queuedLines.get(window) ?? [];
    lines.push(text);
    this.queuedLines.set(window, lines);
    this.writeWindowText(window, `${text}\n`, GLK_STYLE_INPUT);
    this.pendingLineWindow = undefined;
  }

  writeWindowText(window, text, style = 0) {
    const record = this.windows.get(window);
    if (!record) return;
    if (record.glkType === GLK_WINDOW_TEXT_GRID) {
      this.writeGridText(record, text);
      return;
    }
    this.appendRun(record, text, style);
    record.dirty = true;
  }

  appendRun(record, text, style) {
    if (text.length === 0) return;
    const previous = record.pendingRuns.at(-1);
    if (previous && previous.style === style) {
      previous.text += text;
      return;
    }
    record.pendingRuns.push({ text, style });
  }

  writeGridText(record, text) {
    const padded = record.text.padEnd(record.cursor, " ");
    record.text = padded.slice(0, record.cursor) +
      text +
      padded.slice(record.cursor + text.length);
    record.cursor += text.length;
    record.dirty = true;
  }

  flush() {
    for (const [window, record] of this.windows) {
      if (record.clearPending) {
        this.operations.push({ type: "clear", window });
        record.clearPending = false;
      }
      if (record.cursorDirty) {
        this.operations.push({
          type: "cursor",
          window,
          x: record.cursorX,
          y: record.cursorY,
        });
        record.cursorDirty = false;
      }
      if (!record.dirty) continue;
      if (record.glkType === GLK_WINDOW_TEXT_GRID) {
        this.operations.push({ type: "setText", window, text: record.text });
      } else if (record.pendingRuns.length > 0) {
        this.operations.push({
          type: "append",
          window,
          runs: record.pendingRuns,
        });
        record.pendingRuns = [];
      }
      record.dirty = false;
    }
    if (this.operations.length === 0) return;
    console.log("host.flush", this.operations);
    postMessage({ type: "flush", operations: this.operations });
    this.operations = [];
  }

  shiftLine(window) {
    const lines = this.queuedLines.get(window);
    if (lines && lines.length > 0) return lines.shift();
    this.pendingLineWindow = window;
    return undefined;
  }
}

function latin1Encode(text) {
  const bytes = new Uint8Array(text.length);
  for (let index = 0; index < text.length; ++index) {
    bytes[index] = text.charCodeAt(index) & 0xff;
  }
  return bytes;
}

function cString(exports, ptr) {
  const bytes = [];
  const view = new DataView(exports.memory.buffer);
  for (let cursor = ptr; cursor !== 0; ++cursor) {
    const ch = view.getUint8(cursor);
    if (ch === 0) break;
    bytes.push(ch);
  }
  return latin1Decoder.decode(new Uint8Array(bytes));
}

class PlayerVm {
  constructor(host, exports, vm) {
    this.host = host;
    this.exports = exports;
    this.vm = vm;
  }

  run() {
    while (true) {
      const status = this.exports.vm_run_until_blocked(this.vm, 10_000_000);
      this.host.flush();
      if (status === VM_HALTED) {
        postMessage({ type: "status", text: "Halted" });
        return;
      }
      if (status === VM_BLOCKED) {
        postMessage({ type: "input", window: this.host.pendingLineWindow });
        return;
      }
      if (status === VM_OK) continue;
      if (status === VM_ERROR) {
        throw new Error(
          cString(this.exports, this.exports.vm_last_error(this.vm)) ||
            "VM error",
        );
      }
      throw new Error(`unexpected VM status ${status}`);
    }
  }

  submitLine(window, text) {
    this.host.submitLine(window, text);
    this.exports.vm_resume(this.vm);
    this.run();
  }

  resizeWindows(sizes) {
    this.host.updateWindowSizes(sizes);
    this.exports.vm_resume(this.vm);
    this.run();
  }
}

let player;

self.addEventListener("message", async (event) => {
  try {
    const message = event.data;
    if (message.type === "start") {
      player = await startPlayer(message.storyUrl ?? DEFAULT_STORY_URL);
      player.run();
      return;
    }
    if (message.type === "line") {
      player?.submitLine(message.window, message.text);
      return;
    }
    if (message.type === "resize") {
      player?.resizeWindows(message.sizes);
    }
  } catch (error) {
    postMessage({ type: "error", message: String(error?.stack ?? error) });
  }
});

async function startPlayer(storyUrl) {
  const wasi = new WasiPreview1();
  const host = new WorkerGlkHost();

  const imports = {
    env: {
      glupsk_host_window_open: (
        window,
        stream,
        split,
        method,
        size,
        type,
        rock,
      ) => host.openWindow(window, stream, split, method, size, type, rock),
      glupsk_host_window_width: (window) => host.windowWidth(window),
      glupsk_host_window_height: (window) => host.windowHeight(window),
      glupsk_host_window_clear: (window) => host.clearWindow(window),
      glupsk_host_window_move_cursor: (window, x, y) =>
        host.moveCursor(window, x, y),
      glupsk_host_stylehint_set: (windowType, style, hint, value) =>
        host.stylehintSet(windowType, style, hint, value),
      glupsk_host_poll_arrange: () => host.pollArrange(),
      glupsk_host_write_latin1: (window, stream, style, ptr, length) =>
        host.writeLatin1(window, stream, style, ptr, length),
      glupsk_host_write_unicode: (window, stream, style, ptr, length) =>
        host.writeUnicode(window, stream, style, ptr, length),
      glupsk_host_read_line_latin1: (window, ptr, maxLength) =>
        host.readLineLatin1(window, ptr, maxLength),
      glupsk_host_read_line_unicode: (window, ptr, maxLength) =>
        host.readLineUnicode(window, ptr, maxLength),
    },
    wasi_snapshot_preview1: wasi.exports(),
  };

  const [wasmResult, story] = await Promise.all([
    instantiateWasmStreaming(fetch("./glupsk.wasm"), imports),
    fetch(storyUrl)
      .then(requireOk)
      .then((response) => response.arrayBuffer()),
  ]);

  const exports = wasmResult.instance.exports;
  wasi.setMemory(exports.memory);
  host.setExports(exports);

  const storyBytes = new Uint8Array(story);
  const storyPtr = exports.vm_alloc(storyBytes.length);
  if (storyPtr === 0) throw new Error("vm_alloc failed");
  new Uint8Array(exports.memory.buffer, storyPtr, storyBytes.length).set(
    storyBytes,
  );

  const vm = exports.vm_create();
  if (vm === 0) {
    throw new Error(
      cString(exports, exports.vm_last_error(0)) || "vm_create failed",
    );
  }
  const loaded = exports.vm_load_story(vm, storyPtr, storyBytes.length);
  if (loaded !== VM_OK) {
    throw new Error(
      cString(exports, exports.vm_last_error(vm)) ||
        `vm_load_story failed: ${loaded}`,
    );
  }

  postMessage({ type: "status", text: "Running" });
  return new PlayerVm(host, exports, vm);
}

function glkStyleName(style) {
  return GLK_STYLE_NAMES[style] ?? `unknown-${style}`;
}

function glkStyleHintName(hint) {
  return GLK_STYLEHINT_NAMES[hint] ?? `unknown-${hint}`;
}

async function instantiateWasmStreaming(responsePromise, imports) {
  const response = await responsePromise.then(requireOk);
  try {
    return await WebAssembly.instantiateStreaming(
      Promise.resolve(response.clone()),
      imports,
    );
  } catch {
    return WebAssembly.instantiate(await response.arrayBuffer(), imports);
  }
}

function requireOk(response) {
  if (!response.ok) {
    throw new Error(`fetch failed: ${response.status} ${response.url}`);
  }
  return response;
}
