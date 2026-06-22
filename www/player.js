const latin1Decoder = new TextDecoder("latin1");
const utf8Encoder = new TextEncoder();
const utf8Decoder = new TextDecoder();

const VM_OK = 0;
const VM_BLOCKED = 1;
const VM_HALTED = 2;
const VM_ERROR = 3;
const HOST_INPUT_BLOCKED = 0xffffffff;
const GLK_WINDOW_TEXT_BUFFER = 3;
const GLK_WINDOW_TEXT_GRID = 4;

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
      fd_write: (_fd, iovs, iovsLen, nwritten) => {
        const view = this.view();
        let written = 0;
        for (let index = 0; index < iovsLen; ++index) {
          const iov = iovs + index * 8;
          const ptr = view.getUint32(iov, true);
          const len = view.getUint32(iov + 4, true);
          console.debug(utf8Decoder.decode(this.bytes(ptr, len)));
          written += len;
        }
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

class DomGlkHost {
  exports;
  windows = new Map();
  queuedLines = new Map();
  pendingLineWindow;
  scrollTarget;

  constructor(root, commandInput, status) {
    this.root = root;
    this.commandInput = commandInput;
    this.status = status;
  }

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
    const element = document.createElement("section");
    element.className = `window ${split === 0 ? "root" : "split"} ${
      type === GLK_WINDOW_TEXT_GRID ? "grid" : "buffer"
    }`;
    element.dataset.window = String(window);
    element.dataset.stream = String(stream);
    element.innerHTML = `
      <div class="window-header"></div>
      <div class="window-content"></div>
    `;
    element.querySelector(".window-header").textContent = `window ${window} ${
      windowTypeName(type)
    }`;
    this.root.append(element);
    this.windows.set(window, {
      stream,
      split,
      method,
      size,
      type,
      rock,
      element,
      content: element.querySelector(".window-content"),
      cursor: 0,
      text: "",
      pendingText: "",
      dirty: true,
      clearPending: false,
      cursorX: 0,
      cursorY: 0,
      cursorDirty: false,
    });
  }

  windowWidth(_window) {
    return 80;
  }

  windowHeight(_window) {
    return 24;
  }

  clearWindow(window) {
    const record = this.windows.get(window);
    if (!record) return;
    record.text = "";
    record.cursor = 0;
    record.pendingText = "";
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

  writeLatin1(window, _stream, ptr, length) {
    this.writeWindowText(window, latin1Decoder.decode(this.bytes(ptr, length)));
  }

  writeUnicode(window, _stream, ptr, length) {
    const view = new DataView(this.memory().buffer);
    const codepoints = [];
    for (let index = 0; index < length; ++index) {
      codepoints.push(view.getUint32(ptr + index * 4, true));
    }
    this.writeWindowText(window, String.fromCodePoint(...codepoints));
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

  submitLine(text) {
    if (this.pendingLineWindow === undefined) return false;
    const window = this.pendingLineWindow;
    const lines = this.queuedLines.get(window) ?? [];
    lines.push(text);
    this.queuedLines.set(window, lines);
    this.writeWindowText(window, `${text}\n`);
    this.pendingLineWindow = undefined;
    this.commandInput.disabled = true;
    return true;
  }

  writeWindowText(window, text) {
    const record = this.windows.get(window);
    if (!record) return;
    if (record.type === GLK_WINDOW_TEXT_GRID) {
      this.writeGridText(record, text);
      return;
    }
    record.pendingText += text;
    record.dirty = true;
    this.scrollTarget = record.element;
  }

  writeGridText(record, text) {
    const padded = record.text.padEnd(record.cursor, " ");
    record.text = padded.slice(0, record.cursor) + text +
      padded.slice(record.cursor + text.length);
    record.cursor += text.length;
    record.dirty = true;
  }

  flush() {
    for (const record of this.windows.values()) {
      if (record.clearPending) {
        record.content.textContent = "";
        record.clearPending = false;
      }
      if (record.cursorDirty) {
        record.element.dataset.cursorX = String(record.cursorX);
        record.element.dataset.cursorY = String(record.cursorY);
        record.cursorDirty = false;
      }
      if (!record.dirty) continue;
      if (record.type === GLK_WINDOW_TEXT_GRID) {
        record.content.textContent = record.text;
      } else if (record.pendingText.length > 0) {
        record.content.append(document.createTextNode(record.pendingText));
        record.pendingText = "";
      }
      record.dirty = false;
    }
    if (this.scrollTarget) {
      this.scrollTarget.scrollIntoView({ block: "end" });
      this.scrollTarget = undefined;
    }
  }

  shiftLine(window) {
    const lines = this.queuedLines.get(window);
    if (lines && lines.length > 0) return lines.shift();
    this.pendingLineWindow = window;
    this.commandInput.disabled = false;
    this.commandInput.focus();
    this.status.value = `Waiting for input in window ${window}`;
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

function windowTypeName(type) {
  if (type === GLK_WINDOW_TEXT_BUFFER) return "text buffer";
  if (type === GLK_WINDOW_TEXT_GRID) return "text grid";
  return `type ${type}`;
}

async function instantiate() {
  const wasi = new WasiPreview1();
  const host = new DomGlkHost(
    document.querySelector("#windows"),
    document.querySelector("#command-input"),
    document.querySelector("#status"),
  );

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
      glupsk_host_write_latin1: (window, stream, ptr, length) =>
        host.writeLatin1(window, stream, ptr, length),
      glupsk_host_write_unicode: (window, stream, ptr, length) =>
        host.writeUnicode(window, stream, ptr, length),
      glupsk_host_read_line_latin1: (window, ptr, maxLength) =>
        host.readLineLatin1(window, ptr, maxLength),
      glupsk_host_read_line_unicode: (window, ptr, maxLength) =>
        host.readLineUnicode(window, ptr, maxLength),
    },
    wasi_snapshot_preview1: wasi.exports(),
  };

  const [wasmResult, story] = await Promise.all([
    instantiateWasmStreaming(fetch("./glupsk.wasm"), imports),
    fetch("./aa.ulx").then(requireOk).then((response) =>
      response.arrayBuffer()
    ),
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

  const run = async () => {
    while (true) {
      const status = exports.vm_run_until_blocked(vm, 100_000);
      host.flush();
      if (status === VM_HALTED) {
        host.commandInput.disabled = true;
        host.status.value = "Halted";
        return;
      }
      if (status === VM_BLOCKED) return;
      if (status === VM_OK) {
        await nextFrame();
        continue;
      }
      if (status === VM_ERROR) {
        throw new Error(
          cString(exports, exports.vm_last_error(vm)) || "VM error",
        );
      }
      throw new Error(`unexpected VM status ${status}`);
    }
  };

  document.querySelector("#command-form").addEventListener(
    "submit",
    async (event) => {
      event.preventDefault();
      const text = host.commandInput.value;
      host.commandInput.value = "";
      if (!host.submitLine(text)) return;
      exports.vm_resume(vm);
      await run();
    },
  );

  await run();
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

function nextFrame() {
  return new Promise((resolve) => requestAnimationFrame(resolve));
}

instantiate().catch((error) => {
  document.querySelector("#status").value = String(error?.stack ?? error);
});
