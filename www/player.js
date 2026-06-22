const GLK_WINDOW_TEXT_BUFFER = 3;
const GLK_WINDOW_TEXT_GRID = 4;

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
    this.status.value = `Waiting for input in window ${window}`;
  }

  setStatus(text) {
    this.flush();
    this.status.value = text;
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
    element.className = `window ${operation.split === 0 ? "root" : "split"} ${
      operation.glkType === GLK_WINDOW_TEXT_GRID ? "grid" : "buffer"
    }`;
    element.dataset.window = String(operation.window);
    element.dataset.stream = String(operation.stream);
    element.innerHTML = `
      <div class="window-header"></div>
      <div class="window-content"></div>
    `;
    element.querySelector(".window-header").textContent =
      `window ${operation.window} ${windowTypeName(operation.glkType)}`;
    this.root.append(element);
    this.windows.set(operation.window, {
      ...operation,
      element,
      content: element.querySelector(".window-content"),
    });
  }

  setCursor(operation) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    record.element.dataset.cursorX = String(operation.x);
    record.element.dataset.cursorY = String(operation.y);
  }

  appendText(operation) {
    const record = this.windows.get(operation.window);
    if (!record || operation.text.length === 0) return;
    record.content.append(document.createTextNode(operation.text));
    this.scrollTarget = record.element;
  }

  setText(operation) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    record.content.textContent = operation.text;
  }
}

function windowTypeName(type) {
  if (type === GLK_WINDOW_TEXT_BUFFER) return "text buffer";
  if (type === GLK_WINDOW_TEXT_GRID) return "text grid";
  return `type ${type}`;
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
