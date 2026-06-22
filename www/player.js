import { refresh as refreshJustification } from "./kp.js";

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

  constructor(root, commandForm, commandInput, status, worker) {
    this.root = root;
    this.commandForm = commandForm;
    this.commandInput = commandInput;
    this.status = status;
    this.worker = worker;
    this.commandInput.addEventListener("input", () => this.updateCommandWidth());
    new ResizeObserver(() => this.scheduleResizeReport()).observe(this.root);
    this.updateCommandWidth();
  }

  enqueue(operations) {
    this.pendingOperations.push(...operations);
    if (this.flushScheduled) return;
    this.flushScheduled = true;
    requestAnimationFrame(() => this.flush());
  }

  flush(useTransition = true) {
    this.flushScheduled = false;
    if (this.pendingOperations.length === 0) return;
    const operations = this.pendingOperations;
    this.pendingOperations = [];
    if (!useTransition) {
      this.apply(operations);
      return;
    }
    this.applyWithTransition(operations);
  }

  requestInput(window) {
    this.flush(false);
    this.pendingLineWindow = window;
    this.placeCommandForm(window);
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
    this.scheduleResizeReport();
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
    this.commandForm.hidden = true;
    this.commandForm.remove();
    this.worker.postMessage({
      type: "line",
      window: this.pendingLineWindow,
      text,
    });
    this.pendingLineWindow = undefined;
  }

  placeCommandForm(window) {
    const record = this.windows.get(window);
    if (!record) return;
    this.commandForm.hidden = false;
    this.updateCommandWidth();
    if (record.glkType === GLK_WINDOW_TEXT_BUFFER) {
      const paragraph = record.currentParagraph ?? record.lastParagraph ??
        this.createBufferParagraph(record);
      record.currentParagraph = paragraph;
      paragraph.append(this.commandForm);
    } else {
      record.content.append(this.commandForm);
    }
    this.scrollTarget = record.element;
  }

  updateCommandWidth() {
    const ch = Math.max(8, Math.min(32, this.commandInput.value.length + 2));
    this.commandForm.style.setProperty("--command-ch", String(ch));
  }

  applyOne(operation) {
    switch (operation.type) {
      case "openWindow":
        this.openWindow(operation);
        return;
      case "clear":
        this.clearWindow(operation.window);
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
      currentParagraph: undefined,
      lastParagraph: undefined,
    });
  }

  clearWindow(window) {
    const record = this.windows.get(window);
    if (!record) return;
    record.content.replaceChildren();
    record.currentParagraph = undefined;
    record.lastParagraph = undefined;
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
    if (record.glkType === GLK_WINDOW_TEXT_BUFFER) {
      this.appendBufferText(record, runs);
      this.scrollTarget = record.element;
      return;
    }
    const fragment = document.createDocumentFragment();
    for (const run of runs) {
      fragment.append(renderTextRun(run));
    }
    record.content.append(fragment);
    this.scrollTarget = record.element;
  }

  appendBufferText(record, runs) {
    const changedParagraphs = new Set();
    for (const run of runs) {
      const parts = run.text.split(/(\n+)/);
      for (const part of parts) {
        if (part === "") continue;
        if (part[0] === "\n") {
          record.currentParagraph = undefined;
          continue;
        }
        if (!record.currentParagraph) {
          if (part.trim() === "") continue;
          record.currentParagraph = this.createBufferParagraph(record);
        }
        record.currentParagraph.append(renderTextRun({ ...run, text: part }));
        changedParagraphs.add(record.currentParagraph);
      }
    }
    for (const paragraph of changedParagraphs) {
      refreshJustification(paragraph);
    }
  }

  createBufferParagraph(record) {
    const paragraph = document.createElement("p");
    paragraph.className = "buffer-paragraph";
    record.content.append(paragraph);
    record.lastParagraph = paragraph;
    return paragraph;
  }

  setText(operation) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    record.content.textContent = operation.text;
    record.currentParagraph = undefined;
    record.lastParagraph = undefined;
  }

  scheduleResizeReport() {
    if (this.resizeScheduled) return;
    this.resizeScheduled = true;
    requestAnimationFrame(() => this.reportWindowSizes());
  }

  reportWindowSizes() {
    this.resizeScheduled = false;
    const sizes = [];
    const rootRect = this.root.getBoundingClientRect();
    for (const [window, record] of this.windows) {
      sizes.push({ window, ...measureWindow(record, rootRect) });
    }
    if (sizes.length === 0) return;
    this.worker.postMessage({ type: "resize", sizes });
  }
}

function measureWindow(record, rootRect) {
  const metrics = textMetrics(record.content);
  const style = getComputedStyle(record.content);
  const horizontalPadding = parseFloat(style.paddingLeft) +
    parseFloat(style.paddingRight);
  const verticalPadding = parseFloat(style.paddingTop) +
    parseFloat(style.paddingBottom);
  const rect = record.element.getBoundingClientRect();
  const visibleWidth = Math.min(rect.right, rootRect.right) -
    Math.max(rect.left, rootRect.left);
  const visibleHeight = Math.min(rect.bottom, rootRect.bottom) -
    Math.max(rect.top, rootRect.top);
  const width = Math.max(
    1,
    Math.floor((visibleWidth - horizontalPadding) / metrics.charWidth),
  );
  const height = Math.max(
    1,
    Math.floor((visibleHeight - verticalPadding) / metrics.lineHeight),
  );
  return { width, height };
}

function textMetrics(element) {
  const probe = document.createElement("span");
  probe.textContent = "0000000000";
  probe.style.position = "absolute";
  probe.style.visibility = "hidden";
  probe.style.whiteSpace = "pre";
  element.append(probe);
  const rect = probe.getBoundingClientRect();
  const style = getComputedStyle(element);
  const lineHeight = Number.parseFloat(style.lineHeight) ||
    rect.height ||
    Number.parseFloat(style.fontSize) * 1.2;
  probe.remove();
  return {
    charWidth: Math.max(1, rect.width / 10),
    lineHeight: Math.max(1, lineHeight),
  };
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
  document.querySelector("#command-form"),
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
