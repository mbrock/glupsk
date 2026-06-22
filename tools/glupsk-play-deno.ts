#!/usr/bin/env -S deno run --allow-read

const latin1Decoder = new TextDecoder("latin1")
const utf8Encoder = new TextEncoder()

const VM_OK = 0
const VM_BLOCKED = 1
const VM_HALTED = 2
const VM_ERROR = 3

type Exports = {
  memory: WebAssembly.Memory
  vm_alloc(size: number): number
  vm_free(ptr: number, size: number): void
  vm_create(): number
  vm_destroy(vm: number): void
  vm_load_story(vm: number, ptr: number, size: number): number
  vm_run_until_blocked(vm: number, maxSteps: number): number
  vm_resume(vm: number): number
  vm_last_error(vm: number): number
}

class WasiPreview1 {
  memory?: WebAssembly.Memory

  setMemory(memory: WebAssembly.Memory) {
    this.memory = memory
  }

  view() {
    if (!this.memory) throw new Error("WASI memory is not initialized")
    return new DataView(this.memory.buffer)
  }

  bytes(ptr: number, len: number) {
    if (!this.memory) throw new Error("WASI memory is not initialized")
    return new Uint8Array(this.memory.buffer, ptr, len)
  }

  exports() {
    return {
      environ_get: () => 0,
      environ_sizes_get: (_count: number, size: number) => {
        this.view().setUint32(size, 0, true)
        return 0
      },
      fd_close: () => 0,
      fd_seek: (_fd: number, _offset: bigint, _whence: number, newOffset: number) => {
        this.view().setBigUint64(newOffset, 0n, true)
        return 0
      },
      fd_write: (fd: number, iovs: number, iovsLen: number, nwritten: number) => {
        const view = this.view()
        let written = 0
        for (let index = 0; index < iovsLen; ++index) {
          const iov = iovs + index * 8
          const ptr = view.getUint32(iov, true)
          const len = view.getUint32(iov + 4, true)
          const data = this.bytes(ptr, len)
          if (fd === 2) Deno.stderr.writeSync(data)
          else Deno.stdout.writeSync(data)
          written += len
        }
        view.setUint32(nwritten, written, true)
        return 0
      },
      proc_exit: (code: number) => {
        throw new Error(`wasm proc_exit(${code})`)
      },
      random_get: (ptr: number, len: number) => {
        crypto.getRandomValues(this.bytes(ptr, len))
        return 0
      },
    }
  }
}

class Terminal {
  exports?: Exports

  constructor(private inputLines?: string[]) {}

  setExports(exports: Exports) {
    this.exports = exports
  }

  memory() {
    if (!this.exports) throw new Error("terminal memory is not initialized")
    return this.exports.memory
  }

  writeLatin1(ptr: number, length: number) {
    Deno.stdout.writeSync(latin1BytesToUtf8(this.bytes(ptr, length)))
  }

  writeUnicode(ptr: number, length: number) {
    const view = new DataView(this.memory().buffer)
    const codepoints = []
    for (let index = 0; index < length; ++index) {
      codepoints.push(view.getUint32(ptr + index * 4, true))
    }
    Deno.stdout.writeSync(utf8Encoder.encode(String.fromCodePoint(...codepoints)))
  }

  readLineLatin1(_window: number, ptr: number, maxLength: number) {
    const line = this.readLine()
    const encoded = latin1Encode(line)
    const count = Math.min(maxLength, encoded.length)
    this.bytes(ptr, count).set(encoded.subarray(0, count))
    return count
  }

  readLineUnicode(_window: number, ptr: number, maxLength: number) {
    const line = this.readLine()
    const view = new DataView(this.memory().buffer)
    const chars = Array.from(line)
    const count = Math.min(maxLength, chars.length)
    for (let index = 0; index < count; ++index) {
      view.setUint32(ptr + index * 4, chars[index].codePointAt(0) ?? 0, true)
    }
    return count
  }

  bytes(ptr: number, len: number) {
    return new Uint8Array(this.memory().buffer, ptr, len)
  }

  private readLine() {
    if (this.inputLines) {
      const line = this.inputLines.shift()
      if (line === undefined) throw new Error("scripted input reached EOF")
      return line
    }

    const line = prompt("\n> ")
    if (line === null) throw new Error("terminal input cancelled")
    return line
  }
}

function latin1BytesToUtf8(bytes: Uint8Array) {
  return utf8Encoder.encode(latin1Decoder.decode(bytes))
}

function latin1Encode(text: string) {
  const bytes = new Uint8Array(text.length)
  for (let index = 0; index < text.length; ++index) {
    bytes[index] = text.charCodeAt(index) & 0xff
  }
  return bytes
}

function inputLines(text: string) {
  const lines = text.replaceAll("\r\n", "\n").replaceAll("\r", "\n").split("\n")
  if (lines.at(-1) === "") lines.pop()
  return lines
}

function cString(exports: Exports, ptr: number) {
  const bytes = []
  const view = new DataView(exports.memory.buffer)
  for (let cursor = ptr; cursor !== 0; ++cursor) {
    const ch = view.getUint8(cursor)
    if (ch === 0) break
    bytes.push(ch)
  }
  return latin1Decoder.decode(new Uint8Array(bytes))
}

if (Deno.args.length < 1) {
  console.error("usage: deno run --allow-read tools/glupsk-play-deno.ts STORY.ulx")
  Deno.exit(2)
}

const storyPath = Deno.args[0]
const wasmPath = new URL("../build/wasm/glupsk.wasm", import.meta.url)

const wasi = new WasiPreview1()
const terminal = new Terminal(
  Deno.stdin.isTerminal()
    ? undefined
    : inputLines(await new Response(Deno.stdin.readable).text()),
)

const wasmBytes = await Deno.readFile(wasmPath)
const instance = await WebAssembly.instantiate(await WebAssembly.compile(wasmBytes), {
  env: {
    glupsk_host_write_latin1: (ptr: number, length: number) =>
      terminal.writeLatin1(ptr, length),
    glupsk_host_write_unicode: (ptr: number, length: number) =>
      terminal.writeUnicode(ptr, length),
    glupsk_host_read_line_latin1: (window: number, ptr: number, maxLength: number) =>
      terminal.readLineLatin1(window, ptr, maxLength),
    glupsk_host_read_line_unicode: (window: number, ptr: number, maxLength: number) =>
      terminal.readLineUnicode(window, ptr, maxLength),
  },
  wasi_snapshot_preview1: wasi.exports(),
})

const exports = instance.exports as Exports
wasi.setMemory(exports.memory)
terminal.setExports(exports)

const story = await Deno.readFile(storyPath)
const storyPtr = exports.vm_alloc(story.length)
if (storyPtr === 0) throw new Error("vm_alloc failed")
new Uint8Array(exports.memory.buffer, storyPtr, story.length).set(story)

const vm = exports.vm_create()
if (vm === 0) throw new Error(cString(exports, exports.vm_last_error(0)) || "vm_create failed")

try {
  const loaded = exports.vm_load_story(vm, storyPtr, story.length)
  if (loaded !== VM_OK) {
    throw new Error(cString(exports, exports.vm_last_error(vm)) || `vm_load_story failed: ${loaded}`)
  }

  while (true) {
    const status = exports.vm_run_until_blocked(vm, 1_000_000)
    if (status === VM_HALTED) break
    if (status === VM_BLOCKED) {
      exports.vm_resume(vm)
      continue
    }
    if (status === VM_OK) continue
    if (status === VM_ERROR) {
      throw new Error(cString(exports, exports.vm_last_error(vm)) || "VM error")
    }
    throw new Error(`unexpected VM status ${status}`)
  }
} finally {
  exports.vm_destroy(vm)
  exports.vm_free(storyPtr, story.length)
}
