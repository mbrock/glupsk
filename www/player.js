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

class DomRenderer {
  windows = new Map();
  pendingLineWindow;
  scrollTarget;
  pendingOperations = [];
  flushScheduled = false;
  outputHasRendered = false;
  followNextOutput = false;
  inputHasBeenRequested = false;

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
    const followInput = this.followNextOutput ||
      (this.inputHasBeenRequested && this.isScrolledNearBottom());
    this.followNextOutput = false;
    this.pendingLineWindow = window;
    this.placeCommandForm(window, followInput);
    this.commandInput.disabled = false;
    this.commandInput.focus({ preventScroll: !followInput });
    this.status.value = "Waiting for input";
    this.inputHasBeenRequested = true;
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
    const followOutput = this.followNextOutput ||
      (this.outputHasRendered && this.isScrolledNearBottom());
    this.followNextOutput = false;
    for (const operation of operations) {
      this.applyOne(operation, followOutput);
    }
    this.scheduleResizeReport();
    if (this.scrollTarget) {
      this.root.scrollTo({ top: this.root.scrollHeight });
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
    this.followNextOutput = true;
    this.pendingLineWindow = undefined;
  }

  placeCommandForm(window, followInput = true) {
    const record = this.windows.get(window);
    if (!record) return;
    this.commandForm.hidden = false;
    this.updateCommandWidth();
    if (record.glkType === GLK_WINDOW_TEXT_BUFFER) {
      let paragraph = record.currentParagraph ?? record.lastParagraph ??
        this.createBufferParagraph(record);
      paragraph = this.syncParagraphSemantics(record, paragraph);
      record.currentParagraph = paragraph;
      paragraph.append(this.commandForm);
    } else {
      record.content.append(this.commandForm);
    }
    if (followInput) {
      this.scrollTarget = record.element;
    }
  }

  updateCommandWidth() {
    const ch = Math.max(8, Math.min(32, this.commandInput.value.length + 2));
    this.commandForm.style.setProperty("--command-ch", String(ch));
  }

  applyOne(operation, followOutput) {
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
        this.appendText(operation, followOutput);
        return;
      case "setText":
        this.setText(operation, followOutput);
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
    element.dataset.glkWindowType = windowTypeName(operation.glkType);
    element.dataset.glkWindowTypeId = String(operation.glkType);
    annotateStyleHints(element, operation.styleHints ?? []);
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

  appendText(operation, followOutput) {
    const record = this.windows.get(operation.window);
    const runs = operation.runs ?? [{ text: operation.text ?? "", style: 0 }];
    console.log("appendText", {
      window: operation.window,
      runs: runs.map((run) => ({ style: run.style, text: run.text })),
    });
    if (!record || runs.length === 0) return;
    if (record.glkType === GLK_WINDOW_TEXT_BUFFER) {
      this.appendBufferText(record, runs);
      this.noteOutput(record, followOutput);
      return;
    }
    const fragment = document.createDocumentFragment();
    for (const run of runs) {
      fragment.append(renderTextRun(run));
    }
    record.content.append(fragment);
    this.noteOutput(record, followOutput);
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
        record.currentParagraph = this.syncParagraphSemantics(
          record,
          record.currentParagraph,
        );
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

  syncParagraphSemantics(record, paragraph) {
    const summary = summarizeParagraphStyles(paragraph);
    const styleName = summary.style === undefined ? undefined
      : glkStyleName(summary.style);
    const tag = semanticTagForParagraphStyle(styleName);
    const next = paragraph.tagName.toLowerCase() === tag ? paragraph
      : replaceElementTag(paragraph, tag);
    next.dataset.glkParagraphStyle = styleName ?? "mixed";
    next.dataset.glkParagraphStyleId = summary.style === undefined ? "mixed"
      : String(summary.style);
    next.dataset.glkStyles = summary.styles.map(glkStyleName).join(" ");
    next.dataset.glkStyleIds = summary.styles.join(" ");
    next.classList.toggle("glk-paragraph-style-mixed", styleName === undefined);
    for (const name of GLK_STYLE_NAMES) {
      next.classList.toggle(
        `glk-paragraph-style-${name}`,
        styleName === name,
      );
    }
    if (record.currentParagraph === paragraph) record.currentParagraph = next;
    if (record.lastParagraph === paragraph) record.lastParagraph = next;
    return next;
  }

  setText(operation, followOutput) {
    const record = this.windows.get(operation.window);
    if (!record) return;
    record.content.textContent = operation.text;
    record.currentParagraph = undefined;
    record.lastParagraph = undefined;
    this.noteOutput(record, followOutput);
  }

  noteOutput(record, followOutput) {
    this.outputHasRendered = true;
    if (followOutput) {
      this.scrollTarget = record.element;
    }
  }

  isScrolledNearBottom() {
    const remaining = this.root.scrollHeight -
      this.root.scrollTop -
      this.root.clientHeight;
    return remaining <= 48;
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
  const element = document.createElement("span");
  const styleName = glkStyleName(run.style);
  element.className = [
    "glk-run",
    `glk-style-${run.style}`,
    `glk-style-${styleName}`,
  ].join(" ");
  element.dataset.glkStyle = styleName;
  element.dataset.glkStyleId = String(run.style);
  element.textContent = run.text;
  return element;
}

function summarizeParagraphStyles(paragraph) {
  let style;
  let hasText = false;
  let mixed = false;
  const styles = new Set();
  const walker = document.createTreeWalker(paragraph, NodeFilter.SHOW_TEXT, {
    acceptNode(node) {
      if (!node.nodeValue || node.nodeValue.trim() === "") {
        return NodeFilter.FILTER_REJECT;
      }
      if (node.parentElement?.closest(".command")) {
        return NodeFilter.FILTER_REJECT;
      }
      return NodeFilter.FILTER_ACCEPT;
    },
  });
  let node;
  while ((node = walker.nextNode())) {
    const run = node.parentElement?.closest("[data-glk-style-id]");
    const nodeStyle = run ? Number(run.dataset.glkStyleId) : 0;
    styles.add(nodeStyle);
    if (!hasText) {
      style = nodeStyle;
      hasText = true;
    } else if (style !== nodeStyle) {
      mixed = true;
      break;
    }
  }
  return {
    style: hasText && !mixed ? style : undefined,
    styles: [...styles].sort((left, right) => left - right),
  };
}

function semanticTagForParagraphStyle(styleName) {
  switch (styleName) {
    case "header":
      return "h1";
    case "subheader":
      return "h2";
    case "block-quote":
      return "blockquote";
    default:
      return "p";
  }
}

function replaceElementTag(element, tag) {
  const replacement = document.createElement(tag);
  replacement.className = element.className;
  for (const { name, value } of element.attributes) {
    if (name !== "class") replacement.setAttribute(name, value);
  }
  replacement.replaceChildren(...element.childNodes);
  element.replaceWith(replacement);
  return replacement;
}

function glkStyleName(style) {
  return GLK_STYLE_NAMES[style] ?? `unknown-${style}`;
}

function annotateStyleHints(element, styleHints) {
  if (styleHints.length === 0) return;
  element.dataset.glkStyleHints = JSON.stringify(styleHints);
  for (const hint of styleHints) {
    const styleName = hint.styleName ?? glkStyleName(hint.style);
    const hintName = hint.hintName ?? glkStyleHintName(hint.hint);
    element.dataset[
      dataKey(`glk-stylehint-${styleName}-${hintName}`)
    ] = String(hint.signedValue ?? hint.value);
  }
}

function glkStyleHintName(hint) {
  return GLK_STYLEHINT_NAMES[hint] ?? `unknown-${hint}`;
}

function windowTypeName(type) {
  switch (type) {
    case GLK_WINDOW_TEXT_BUFFER:
      return "text-buffer";
    case GLK_WINDOW_TEXT_GRID:
      return "text-grid";
    default:
      return `unknown-${type}`;
  }
}

function dataKey(name) {
  return name.replace(/-([a-z0-9])/g, (_, ch) => ch.toUpperCase());
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

const GAMES = {
  aa: {
    id: "aa",
    title: "Bertil's Bash Blues",
    author: "Daniel Brockman",
    storyUrl: "./aa-14.ulx",
  },
  "shutdown-garden": {
    id: "shutdown-garden",
    title: "The Shutdown Garden",
    author: "Codex",
    storyUrl: "./shutdown-garden-20.ulx",
  },
};

function selectedGame() {
  const gameId = new URLSearchParams(window.location.search).get("game") ??
    "aa";
  return GAMES[gameId] ?? GAMES.aa;
}

const game = selectedGame();
document.title = `${game.title} - Glupsk`;
document.querySelector(".player")?.setAttribute(
  "aria-label",
  `${game.title} in the Glupsk interactive fiction player`,
);
document.querySelector("#status").value = `Loading ${game.title}...`;
for (const link of document.querySelectorAll("[data-game]")) {
  const selected = link.dataset.game === game.id;
  link.classList.toggle("selected", selected);
  link.toggleAttribute("aria-current", selected);
}

const worker = new Worker("./player-worker.js?v=28", { type: "module" });
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

worker.postMessage({
  type: "start",
  storyUrl: game.storyUrl,
  title: game.title,
});
