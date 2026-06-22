const GLK_WINDOW_TEXT_BUFFER = 3;
const GLK_WINDOW_TEXT_GRID = 4;
const WINMETHOD_LEFT = 0x00;
const WINMETHOD_RIGHT = 0x01;
const WINMETHOD_ABOVE = 0x02;
const WINMETHOD_BELOW = 0x03;
const WINMETHOD_DIRECTION_MASK = 0x0f;
const WINMETHOD_FIXED = 0x10;
const WINMETHOD_DIVISION_MASK = 0xf0;

class DomRenderer {
  windows = new Map();
  pendingLineWindow;
  scrollTarget;
  pendingOperations = [];
  flushScheduled = false;

  constructor(root, commandInput, status, worker) {
    this.root = root;
    this.commandInput = commandInput;
    this.status = status;
    this.worker = worker;
  }

  enqueue(operations) {
    this.pendingOperations.push(...operations);
    if (this.flushScheduled) return;
    this.flushScheduled = true;
    requestAnimationFrame(() => this.flush());
  }

  flush() {
    this.flushScheduled = false;
    if (this.pendingOperations.length === 0) return;
    const operations = this.pendingOperations;
    this.pendingOperations = [];
    this.applyWithTransition(operations);
  }

  requestInput(window) {
    this.flush();
    this.pendingLineWindow = window;
    this.commandInput.disabled = false;
    this.commandInput.focus();
    this.status.value = "Waiting for input";
  }

  setStatus(text) {
    this.flush();
    this.status.value = text === "Running" ? "" : text;
  }

  applyWithTransition(operations) {
    if (!document.startViewTransition) {
      this.apply(operations);
      return;
    }
    document.startViewTransition(() => this.apply(operations));
  }

  apply(operations) {
    for (const operation of operations) {
      this.applyOne(operation);
    }
    if (this.scrollTarget) {
      this.scrollTarget.scrollIntoView({ block: "end" });
      this.scrollTarget = undefined;
    }
  }

  submitLine() {
    if (this.pendingLineWindow === undefined) return;
    const text = this.commandInput.value;
    this.commandInput.value = "";
    this.commandInput.disabled = true;
    this.worker.postMessage({
      type: "line",
      window: this.pendingLineWindow,
      text,
    });
    this.pendingLineWindow = undefined;
  }

  applyOne(operation) {
    switch (operation.type) {
      case "openWindow":
        this.openWindow(operation);
        return;
      case "clear":
        this.windows.get(operation.window)?.content.replaceChildren();
        return;
      case "cursor":
        this.setCursor(operation);
        return;
      case "append":
        this.appendText(operation);
        return;
      case "setText":
        this.setText(operation);
        return;
    }
  }

  openWindow(operation) {
    const element = document.createElement("section");
    const direction = splitDirection(operation.method);
    element.className = [
      "window",
      operation.split === 0 ? "root" : "split",
      operation.glkType === GLK_WINDOW_TEXT_GRID ? "grid" : "buffer",
      direction,
    ].join(" ");
    element.dataset.window = String(operation.window);
    element.dataset.stream = String(operation.stream);
    element.dataset.split = String(operation.split);
    element.dataset.method = String(operation.method);
    element.style.setProperty("--glk-size", String(operation.size));
    if (isFixedSplit(operation.method)) {
      element.classList.add("fixed");
    }
    element.innerHTML = `
      <div class="window-content"></div>
    `;
    this.insertWindow(element, operation);
    this.windows.set(operation.window, {
      ...operation,
      element,
      content: element.querySelector(".window-content"),
    });
  }

  insertWindow(element, operation) {
    const split = this.windows.get(operation.split);
    if (!split) {
      this.root.append(element);
      return;
    }

    const direction = operation.method & WINMETHOD_DIRECTION_MASK;
    if (direction === WINMETHOD_ABOVE || direction === WINMETHOD_LEFT) {
      split.element.before(element);
      return;
    }
    split.element.after(element);
  }

  setCursor(operation) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    record.element.dataset.cursorX = String(operation.x);
    record.element.dataset.cursorY = String(operation.y);
  }

  appendText(operation) {
    const record = this.windows.get(operation.window);
    const runs = operation.runs ?? [{ text: operation.text ?? "", style: 0 }];
    if (!record || runs.length === 0) return;
    const fragment = document.createDocumentFragment();
    for (const run of runs) {
      fragment.append(renderTextRun(run));
    }
    record.content.append(fragment);
    this.scrollTarget = record.element;
  }

  setText(operation) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    if (record.glkType === GLK_WINDOW_TEXT_GRID && renderStatusLine(record, operation.text)) {
      return;
    }
    record.element.classList.remove("status-grid");
    record.content.textContent = operation.text;
  }
}

function renderStatusLine(record, text) {
  const lines = text.replace(/\s+$/u, "").split("\n").filter((line) =>
    line.trim().length > 0
  );
  if (lines.length !== 1) return false;
  const match = lines[0].match(/^\s*(.*?)\s{2,}(\S.*?)\s*$/u);
  if (!match) return false;

  const left = document.createElement("span");
  left.textContent = match[1];
  const right = document.createElement("span");
  right.textContent = match[2];
  record.element.classList.add("status-grid");
  record.content.replaceChildren(left, right);
  return true;
}

function renderTextRun(run) {
  if (run.style === 0) {
    return document.createTextNode(run.text);
  }
  const element = document.createElement("span");
  element.className = `glk-style-${run.style}`;
  element.textContent = run.text;
  return element;
}

function splitDirection(method) {
  switch (method & WINMETHOD_DIRECTION_MASK) {
    case WINMETHOD_LEFT:
      return "left";
    case WINMETHOD_RIGHT:
      return "right";
    case WINMETHOD_ABOVE:
      return "above";
    case WINMETHOD_BELOW:
      return "below";
    default:
      return "unplaced";
  }
}

function isFixedSplit(method) {
  return (method & WINMETHOD_DIVISION_MASK) === WINMETHOD_FIXED;
}

const worker = new Worker("./player-worker.js", { type: "module" });
const renderer = new DomRenderer(
  document.querySelector("#windows"),
  document.querySelector("#command-input"),
  document.querySelector("#status"),
  worker,
);

worker.addEventListener("message", (event) => {
  const message = event.data;
  switch (message.type) {
    case "flush":
      renderer.enqueue(message.operations);
      return;
    case "input":
      renderer.requestInput(message.window);
      return;
    case "status":
      renderer.setStatus(message.text);
      return;
    case "error":
      renderer.setStatus(message.message);
      console.error(message.message);
      return;
  }
});

document.querySelector("#command-form").addEventListener("submit", (event) => {
  event.preventDefault();
  renderer.submitLine();
});

worker.postMessage({ type: "start" });
