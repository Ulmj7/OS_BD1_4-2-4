"use strict";
const $ = (id) => document.getElementById(id);
const NS = "http://www.w3.org/2000/svg";
const names = {color: "Өнгөний жагсаалт", collatz: "Collatz дараалал", stack: "Стек", queue: "Дараалал"};
const definitions = {
  color: [["add_tail", "Төгсгөлд нэмэх"], ["add_head", "Эхэнд нэмэх"], ["insert", "Индексийн дараа оруулах"], ["delete_index", "Индексээр устгах"], ["delete", "Өнгөөр устгах"], ["search", "Өнгөөр хайх"], ["traverse", "Жагсаалтыг гүйлгэх"], ["clear", "Жагсаалтыг хоослох"]],
  collatz: [["run", "Дараалал үүсгэх"], ["search", "Утгаар хайх"], ["delete", "Утгаар устгах"], ["traverse", "Жагсаалтыг гүйлгэх"], ["clear", "Жагсаалтыг хоослох"]],
  stack: [["push", "Эхэнд оруулах · push"], ["pop", "Оройгоос авах · pop"], ["peek", "Оройг харах · peek"]],
  queue: [["enqueue", "Төгсгөлд оруулах · enqueue"], ["dequeue", "Эхнээс авах · dequeue"], ["peek", "Эхний утгыг харах · peek"]]
};
const errors = {
  E_SYNTAX: "Командын бичлэг буруу байна. key=value хэлбэрийг шалгана уу.",
  E_UNKNOWN_CMD: "Үл мэдэгдэх команд. Үйлдлийн жагсаалтаас сонгоно уу.",
  E_UNKNOWN_ARG: "Үл мэдэгдэх аргумент байна. Аргументын нэрийг шалгана уу.",
  E_MISSING_ARG: "Шаардлагатай аргумент дутуу байна.",
  E_BAD_VALUE: "Бүхэл аравтын тоо оруулна уу.",
  E_RANGE: "Утга зөвшөөрөгдсөн хязгаараас гарсан. Утга болон индексийг шалгана уу.",
  E_NOT_FOUND: "Тохирох зангилаа олдсонгүй.",
  E_EMPTY: "Бүтэц хоосон байна. Эхлээд зангилаа нэмнэ үү.",
  E_FULL: "Зангилааны дээд хязгаарт хүрлээ. Бүтцийг хоослох эсвэл бага утга сонгоно уу.",
  E_NOMEM: "kmalloc санах ой олгосонгүй. Өмнөх төлөв хадгалагдсан. inject_fail off командыг ашиглаж болно.",
  E_ENGINE: "Хөдөлгүүрийн холболт тасарсан. Дахин холбогдоод командыг давтана уу."
};
let active = "color", history = [], cursor = -1, busy = false;
let scene = {width: 1100, height: 320}, camera = {x: 0, y: 0, scale: 1}, animation = 0;
let drag = null;
const graph = $("graph"), frame = $("graph-frame");
const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
const systemDark = window.matchMedia("(prefers-color-scheme: dark)");
let theme;
try { theme = localStorage.getItem("kds-theme"); } catch { /* Private browsing may disable storage. */ }
if (theme === "dark" || theme === "light") document.documentElement.dataset.theme = theme;
function updateThemeLabel() {
  const dark = (document.documentElement.dataset.theme || (systemDark.matches ? "dark" : "light")) === "dark";
  $("theme").textContent = dark ? "Цайвар горим" : "Бараан горим";
  $("theme").setAttribute("aria-pressed", String(dark));
}
$("theme").addEventListener("click", () => {
  const dark = $("theme").getAttribute("aria-pressed") === "true";
  document.documentElement.dataset.theme = dark ? "light" : "dark";
  try { localStorage.setItem("kds-theme", document.documentElement.dataset.theme); } catch { /* Optional preference. */ }
  updateThemeLabel();
});
systemDark.addEventListener("change", updateThemeLabel);
updateThemeLabel();
function element(tag, attributes = {}, text) {
  const node = document.createElementNS(NS, tag);
  Object.entries(attributes).forEach(([key, value]) => node.setAttribute(key, value));
  if (text !== undefined) node.textContent = String(text);
  return node;
}
function field(name, label, value, options = {}) {
  const box = document.createElement("div"); box.className = "field";
  const lab = document.createElement("label"); lab.htmlFor = `field-${name}`; lab.textContent = label;
  const input = document.createElement("input"); input.id = `field-${name}`; input.name = name; input.value = value;
  input.type = options.type || "number"; input.required = !options.optional; input.autocomplete = "off";
  if (input.type === "number") { input.step = "1"; if (options.min !== undefined) input.min = options.min; if (options.max !== undefined) input.max = options.max; }
  else { input.inputMode = "numeric"; input.pattern = "[+-]?[0-9]+"; }
  if (options.optional) input.placeholder = "Дурын";
  box.append(lab, input); return box;
}
function updateFields() {
  const op = $("operation").value, fields = $("fields"); fields.replaceChildren();
  let hint = "Үйлдэл нь C хөдөлгүүрийн бодит жагсаалтад хийгдэнэ.";
  if (active === "color") {
    if (op === "insert" || op === "delete_index") fields.append(field("index", "Индекс · 0-оос эхэлнэ", "0", {min: 0}));
    if (["add_head", "add_tail", "insert", "delete", "search"].includes(op)) {
      const optional = op === "delete" || op === "search", rgb = document.createElement("div"); rgb.className = "rgb-fields";
      [["red", "red", 138], ["green", "green", 43], ["blue", "blue", 226]].forEach(([key, label, value]) => rgb.append(field(key, label, optional ? "" : value, {min: 0, max: 255, optional})));
      fields.append(rgb);
      hint = optional ? "RGB: 0–255. Дор хаяж нэг өнгө өгнө. Устгах үйлдэл бүх тохирох зангилааг чөлөөлнө." : "RGB суваг бүр 0–255. insert нь өгсөн индексийн дараа нэмнэ.";
    }
  } else if (active === "collatz") {
    if (op === "run") { fields.append(field("start", "Эхлэх утга · start", "25", {min: 1, max: 2147483647})); hint = "1–2147483647. Өмнөх дарааллыг солино. Утгуудыг 64 битээр хадгална."; }
    else if (op === "search" || op === "delete") { fields.append(field("value", "Хайх утга · value", "88", {type: "text"})); hint = "Бүхэл тоо оруулна. Хайлт зангилаа бүрээр дарааллаар явна."; }
  } else if (op === "push" || op === "enqueue") { fields.append(field("value", "Зангилааны утга · value", "42", {type: "text"})); hint = "Тэмдэгтэй 64 битийн бүхэл тоо. Стек: LIFO · дараалал: FIFO."; }
  $("form-hint").textContent = hint;
}
function selectTab(name) {
  active = name;
  document.querySelectorAll("[data-tab]").forEach((tab) => {
    const selected = tab.dataset.tab === name; tab.setAttribute("aria-selected", selected); tab.tabIndex = selected ? 0 : -1;
  });
  $("workspace").setAttribute("aria-labelledby", `tab-${name}`);
  $("operation").replaceChildren(...definitions[name].map(([value, label]) => new Option(label, value)));
  updateFields(); render(true);
}
document.querySelectorAll("[data-tab]").forEach((tab, i, tabs) => {
  tab.addEventListener("click", () => selectTab(tab.dataset.tab));
  tab.addEventListener("keydown", (event) => {
    let next;
    if (event.key === "ArrowRight") next = (i + 1) % tabs.length;
    if (event.key === "ArrowLeft") next = (i - 1 + tabs.length) % tabs.length;
    if (event.key === "Home") next = 0;
    if (event.key === "End") next = tabs.length - 1;
    if (next !== undefined) { event.preventDefault(); selectTab(tabs[next].dataset.tab); tabs[next].focus(); }
  });
});
$("operation").addEventListener("change", updateFields);
$("operation-form").addEventListener("submit", (event) => {
  event.preventDefault(); const args = [];
  for (const input of $("fields").querySelectorAll("input")) if (input.value.trim()) args.push(`${input.name}=${input.value.trim()}`);
  const op = $("operation").value;
  if (active === "color" && ["search", "delete"].includes(op) && !args.length) { showMessage("Дор хаяж нэг өнгөний утга оруулна уу.", true); return; }
  send(`${active} ${op === "delete_index" ? "delete" : op}${args.length ? ` ${args.join(" ")}` : ""}`);
});
$("command-form").addEventListener("submit", (event) => { event.preventDefault(); send($("command").value); });
document.querySelectorAll("[data-command]").forEach((button) => button.addEventListener("click", () => send(button.dataset.command)));
$("reset").addEventListener("click", () => send("reset"));
function showMessage(text, error = false) { $("message").textContent = text; $("message").classList.toggle("error", error); }
function lockControls() {
  const past = cursor >= 0 && cursor < history.length - 1, disabled = busy || past;
  $("operation-fields").disabled = disabled;
  document.querySelectorAll(".mutating").forEach((button) => { button.disabled = disabled; });
  $("command").disabled = disabled;
  $("history-notice").hidden = !past;
  $("back").disabled = busy || cursor <= 0;
  $("forward").disabled = busy || cursor >= history.length - 1;
  $("latest").disabled = busy || cursor >= history.length - 1;
  $("history").disabled = busy || history.length < 2;
}
function parseReply(text) {
  // Preserve exact int64 display values beyond JavaScript's integer precision.
  return JSON.parse(text.replace(/("value"\s*:\s*)(-?\d+)/g, (whole, prefix, digits) => {
    const number = BigInt(digits);
    return number > BigInt(Number.MAX_SAFE_INTEGER) || number < BigInt(Number.MIN_SAFE_INTEGER) ? prefix + JSON.stringify(digits) : whole;
  }));
}
async function send(command, initial = false) {
  if (busy || (!initial && cursor < history.length - 1)) return;
  busy = true; lockControls(); $("connection").textContent = "Гүйцэтгэж байна…";
  try {
    const response = await fetch(initial ? "/api/state" : "/api/cmd", initial ? {cache: "no-store"} : {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({cmd: command})});
    const reply = parseReply(await response.text());
    if (!response.ok || !reply.state) throw new Error(reply.error?.code || "E_ENGINE");
    const prefix = command.trim().split(/\s+/)[0];
    if (names[prefix] && prefix !== active) selectTab(prefix);
    const previousCount = history[cursor]?.state.structures.find((s) => s.name === active)?.count;
    history.push({...reply, command, timestamp: new Date().toLocaleTimeString("mn-MN", {hour12: false})}); cursor = history.length - 1;
    render(previousCount !== reply.state.structures.find((s) => s.name === active).count);
    $("connection").textContent = "Хөдөлгүүр холбогдсон";
  } catch (error) {
    $("connection").textContent = "Холболтын алдаа";
    showMessage(errors[error.message] || errors.E_ENGINE, true);
  } finally { busy = false; lockControls(); }
}
function render(fit = false) {
  if (cursor < 0) return;
  const reply = history[cursor], state = reply.state, list = state.structures.find((s) => s.name === active);
  state.structures.forEach((s) => { $(`count-${s.name}`).textContent = s.count; });
  $("graph-title").textContent = names[active];
  let summary = `${list.count} зангилаа · тойрог, хоёр холбоост жагсаалт`;
  if (active === "collatz" && list.count) {
    const peak = list.nodes.reduce((max, n) => BigInt(n.fields.value) > max ? BigInt(n.fields.value) : max, 0n);
    const complete = BigInt(list.nodes.at(-1).fields.value) === 1n && list.nodes.every((node, index) => {
      if (index === list.nodes.length - 1) return true;
      const value = BigInt(node.fields.value), next = BigInt(list.nodes[index + 1].fields.value);
      return next === (value % 2n === 0n ? value / 2n : 3n * value + 1n);
    });
    summary = `${list.count} гишүүн${complete ? ` · ${list.count - 1} алхам` : ""} · оргил ${peak}`;
  }
  if (active === "stack") summary = `${list.count} зангилаа · LIFO · TOP = эхний зангилаа`;
  if (active === "queue") summary = `${list.count} зангилаа · FIFO · FRONT → REAR`;
  $("graph-summary").textContent = summary;
  $("invariant").textContent = list.invariant.ok ? "Холбоос бүрэн" : "Холбоос алдаатай";
  $("invariant").classList.toggle("bad", !list.invariant.ok);
  $("invariant").title = list.invariant.ok ? "next→prev, prev→next болон хоёр чиглэлийн тоо тохирч байна." : "Тойрог эсвэл next/prev холбоос зөрчигдсөн.";
  for (const [field, id] of [["allocs", "allocs"], ["frees", "frees"], ["live", "live"], ["live_bytes", "bytes"]]) $(id).textContent = state.kmalloc[field].toLocaleString("mn-MN");
  $("limit").textContent = `Бүтэц бүрийн дээд хязгаар: ${state.max_nodes} зангилаа.`;
  $("ops").textContent = reply.ops?.join("\n") || "Энэ алхамд Kernel API дуудлага хийгдээгүй.";
  $("ops-count").textContent = reply.ops?.length || 0;
  $("logs").textContent = reply.log?.join("\n") || "Энэ алхамд printk бүртгэл үүсээгүй.";
  $("freed").hidden = !reply.effect?.removed?.length;
  $("freed").textContent = `kfree-ээр чөлөөлсөн дугаарууд: ${(reply.effect?.removed || []).map((id) => `#${id}`).join(", ")}`;
  $("history").max = history.length - 1; $("history").value = cursor;
  $("history-label").textContent = `${cursor + 1} / ${history.length}`;
  $("history-command").textContent = `${reply.timestamp} · ${reply.command}`;
  if (reply.ok) showMessage(cursor < history.length - 1 ? "Түүхэнд хадгалсан төлөвийг үзэж байна." : `Үйлдэл амжилттай · ${reply.command}`);
  else showMessage(`${reply.error.code} · ${errors[reply.error.code] || "Үйлдэл амжилтгүй. Командаа шалгаад дахин оролдоно уу."}`, true);
  renderGraph(list, reply, fit); renderTable(list); lockControls();
}
function renderTable(list) {
  $("table-caption").textContent = `${names[active]} · ${list.count} зангилаа`;
  $("node-table").replaceChildren(...[{index: "—", id: "head", fields: {}, addr: list.head.addr, next: list.head.next, prev: list.head.prev}, ...list.nodes].map((node) => {
    const tr = document.createElement("tr");
    [node.index, node.id, Object.entries(node.fields).map(([key, value]) => `${key}=${value}`).join(", ") || "list_head", node.addr, node.next, node.prev].forEach((text) => { const td = document.createElement("td"); td.textContent = text; tr.append(td); });
    return tr;
  }));
}
function renderGraph(list, reply, fit) {
  stopAnimation();
  graph.querySelectorAll("defs, g").forEach((node) => node.remove());
  const defs = element("defs");
  for (const [id, color] of [["next", "var(--next)"], ["prev", "var(--prev)"], ["unknown", "var(--danger)"]]) {
    const marker = element("marker", {id: `arrow-${id}`, markerWidth: 7, markerHeight: 7, refX: 6, refY: 3.5, orient: "auto", markerUnits: "strokeWidth"});
    marker.append(element("path", {d: "M0 0 L7 3.5 L0 7 Z", fill: color})); defs.append(marker);
  }
  graph.append(defs);
  const nodes = [{id: "head", fields: {}, ...list.head}, ...list.nodes], cols = Math.min(5, nodes.length), w = 172, h = 126, pitch = 210, rowPitch = 208;
  const locations = new Map(nodes.map((node, i) => [node.id, {x: 52 + (i % cols) * pitch, y: 72 + Math.floor(i / cols) * rowPitch}]));
  scene = {width: 104 + cols * pitch - (pitch - w), height: 72 + Math.ceil(nodes.length / cols) * rowPitch + 14};
  const edges = element("g", {"aria-hidden": "true"}), bodies = element("g"); graph.append(edges, bodies);
  nodes.forEach((source) => {
    const a = locations.get(source.id);
    for (const kind of ["next", "prev"]) {
      const target = source[kind], b = locations.get(target), offset = kind === "next" ? 46 : 83;
      let d;
      if (!b) {
        d = `M${a.x + w} ${a.y + offset} l25 0`;
        edges.append(element("text", {x: a.x + w + 29, y: a.y + offset + 4, class: "unknown-label"}, "?"));
      } else if (source.id === target) {
        d = kind === "next" ? `M${a.x + w} ${a.y + 46} C${a.x + w + 65} ${a.y - 28},${a.x + 30} ${a.y - 50},${a.x + 50} ${a.y}` : `M${a.x} ${a.y + 83} C${a.x - 65} ${a.y + h + 55},${a.x + 110} ${a.y + h + 70},${a.x + 120} ${a.y + h}`;
      } else if (source.id === nodes.at(-1).id && target === "head") {
        d = `M${a.x + w} ${a.y + 46} C${scene.width - 12} ${a.y + 46},${scene.width - 16} ${a.y + 26},${scene.width - 16} ${a.y} L${scene.width - 16} 24 Q${scene.width - 16} 14,${scene.width - 32} 14 H${b.x + 80} Q${b.x + 66} 14,${b.x + 66} ${b.y}`;
      } else if (source.id === "head" && target === nodes.at(-1).id) {
        const bottom = b.y + h + 40;
        d = `M${a.x} ${a.y + 83} Q16 ${a.y + 83},16 ${a.y + 110} V${bottom - 12} Q16 ${bottom},32 ${bottom} H${b.x + 110} Q${b.x + 126} ${bottom},${b.x + 126} ${b.y + h}`;
      } else if (a.y === b.y) {
        const east = b.x > a.x;
        d = `M${a.x + (east ? w : 0)} ${a.y + offset} L${b.x + (east ? 0 : w)} ${b.y + offset}`;
      } else {
        const next = kind === "next", startX = a.x + (next ? w : 0), endX = b.x + (next ? 0 : w), outerA = startX + (next ? 15 : -15), outerB = endX + (next ? -15 : 15), gap = next ? a.y + h + 24 : b.y + h + 45;
        d = `M${startX} ${a.y + offset} H${outerA} V${gap} H${outerB} V${b.y + offset} H${endX}`;
      }
      edges.append(element("path", {d, class: `edge ${b ? kind : "unknown"}`, "marker-end": `url(#arrow-${b ? kind : "unknown"})`}));
    }
  });
  nodes.forEach((node, i) => {
    const a = locations.get(node.id), isHead = node.id === "head";
    const group = element("g", {transform: `translate(${a.x} ${a.y})`, class: `${isHead ? "head" : "node"}${reply.effect?.added?.includes(node.id) ? " new-node" : ""}${reply.effect?.matched?.includes(node.id) ? " matched" : ""}`, "data-id": node.id});
    group.append(element("title", {}, isHead ? `&${active}_list · ${node.addr}` : `kmalloc #${node.id} · ${node.addr} · next=${node.next} · prev=${node.prev}`));
    group.append(element("rect", {width: w, height: h, rx: 7, class: "node-body"}));
    group.append(element("text", {x: 14, y: 24, class: "node-name"}, isHead ? "list_head" : `#${node.id}`));
    group.append(element("text", {x: w - 14, y: 24, "text-anchor": "end", class: "node-meta"}, isHead ? "Толгой" : `[${node.index}]`));
    group.append(element("line", {x1: 0, x2: w, y1: 36, y2: 36, stroke: "var(--line)"}));
    if (isHead) {
      group.append(element("text", {x: 14, y: 62}, `&${active}_list`));
      group.append(element("text", {x: 14, y: 84, class: "node-meta"}, "next ↔ prev"));
      if (!list.count) group.append(element("text", {x: 14, y: 107, class: "node-meta"}, "Хоосон жагсаалт"));
    } else if (active === "color") {
      const c = node.fields;
      group.append(element("rect", {x: 128, y: 47, width: 29, height: 56, rx: 4, fill: `rgb(${c.red},${c.green},${c.blue})`, stroke: "var(--muted)", "stroke-width": .6}));
      ["red", "green", "blue"].forEach((key, n) => group.append(element("text", {x: 14, y: 59 + n * 20}, `${key.padEnd(5)} ${c[key]}`)));
    } else {
      const value = String(node.fields.value);
      group.append(element("text", {x: 14, y: 63, class: "node-meta"}, "value"));
      group.append(element("text", {x: 14, y: 88, class: "value", "font-size": value.length > 13 ? 11 : value.length > 9 ? 14 : 18, style: `font-size:${value.length > 13 ? 11 : value.length > 9 ? 14 : 18}px`}, value));
    }
    group.append(element("text", {x: 14, y: 117, class: "pointer-label"}, isHead ? "next / prev → холбоос" : node.addr));
    if (active === "stack" && i === 1) group.append(element("text", {x: 0, y: -12, class: "position-label"}, "TOP · орой"));
    if (active === "queue" && i > 0 && (i === 1 || i === nodes.length - 1)) group.append(element("text", {x: 0, y: -12, class: "position-label"}, `${i === 1 ? "FRONT · эхлэл" : ""}${i === 1 && i === nodes.length - 1 ? " / " : ""}${i === nodes.length - 1 ? "REAR · төгсгөл" : ""}`));
    bodies.append(group);
  });
  if (fit) { fitWidth(); if (frame.clientWidth < 650 && camera.scale < .85) { camera.scale = .85; updateCamera(); } } else updateCamera();
  if (cursor === history.length - 1 && reply.ok && /\b(search|traverse)\b/.test(reply.command)) animateVisited(reply.effect?.visited || []);
}
function updateCamera() {
  const width = frame.clientWidth || 600, height = frame.clientHeight || 390;
  graph.setAttribute("viewBox", `${camera.x} ${camera.y} ${width / camera.scale} ${height / camera.scale}`);
  $("zoom-level").textContent = `${Math.round(camera.scale * 100)}%`;
}
function fitWidth() { camera = {x: 0, y: 0, scale: Math.min(1.35, (frame.clientWidth || 600) / scene.width)}; updateCamera(); }
function zoom(factor, centerX = frame.clientWidth / 2, centerY = frame.clientHeight / 2) {
  const old = camera.scale; camera.scale = Math.max(.12, Math.min(3, old * factor));
  camera.x += centerX / old - centerX / camera.scale; camera.y += centerY / old - centerY / camera.scale; updateCamera();
}
$("fit").addEventListener("click", fitWidth);
$("zoom-in").addEventListener("click", () => zoom(1.25));
$("zoom-out").addEventListener("click", () => zoom(.8));
frame.addEventListener("wheel", (event) => { if (!event.ctrlKey && !event.metaKey) return; event.preventDefault(); const rect = frame.getBoundingClientRect(); zoom(event.deltaY < 0 ? 1.1 : 1 / 1.1, event.clientX - rect.left, event.clientY - rect.top); }, {passive: false});
frame.addEventListener("pointerdown", (event) => { if (event.button !== 0) return; drag = {px: event.clientX, py: event.clientY, x: camera.x, y: camera.y}; frame.setPointerCapture(event.pointerId); frame.classList.add("dragging"); });
frame.addEventListener("pointermove", (event) => { if (!drag) return; camera.x = drag.x - (event.clientX - drag.px) / camera.scale; camera.y = drag.y - (event.clientY - drag.py) / camera.scale; updateCamera(); });
function endDrag() { drag = null; frame.classList.remove("dragging"); }
frame.addEventListener("pointerup", endDrag); frame.addEventListener("pointercancel", endDrag);
frame.addEventListener("keydown", (event) => {
  const delta = 80 / camera.scale;
  if (event.key === "ArrowLeft") camera.x -= delta;
  else if (event.key === "ArrowRight") camera.x += delta;
  else if (event.key === "ArrowUp") camera.y -= delta;
  else if (event.key === "ArrowDown") camera.y += delta;
  else if (event.key === "+" || event.key === "=") zoom(1.25);
  else if (event.key === "-") zoom(.8);
  else if (event.key === "Home") fitWidth();
  else return;
  event.preventDefault(); updateCamera();
});
function stopAnimation() { window.clearTimeout(animation); graph.querySelectorAll(".visiting").forEach((node) => node.classList.remove("visiting")); }
function animateVisited(ids) {
  if (reducedMotion.matches || document.hidden || !ids.length) return;
  let index = 0;
  function step() {
    graph.querySelectorAll(".visiting").forEach((node) => node.classList.remove("visiting"));
    const node = graph.querySelector(`[data-id="${ids[index]}"]`);
    if (node) node.classList.add("visiting", "visited");
    ++index;
    if (index < ids.length) animation = window.setTimeout(step, Math.max(24, Math.min(180, 2600 / ids.length)));
    else animation = window.setTimeout(stopAnimation, 200);
  }
  step();
}
document.addEventListener("visibilitychange", () => { if (document.hidden) stopAnimation(); });
reducedMotion.addEventListener("change", () => { if (reducedMotion.matches) stopAnimation(); });
function stepHistory(index) { cursor = Math.max(0, Math.min(history.length - 1, index)); render(true); }
$("back").addEventListener("click", () => stepHistory(cursor - 1));
$("forward").addEventListener("click", () => stepHistory(cursor + 1));
$("latest").addEventListener("click", () => stepHistory(history.length - 1));
$("history").addEventListener("input", (event) => stepHistory(Number(event.target.value)));
new ResizeObserver(updateCamera).observe(frame);
async function start() {
  selectTab("color"); lockControls(); await send("state", true);
  const demo = new URLSearchParams(location.search).get("run");
  if (demo && history.length) {
    for (const command of demo.split(";").filter((s) => s.trim())) await send(command.trim());
  }
  const tab = new URLSearchParams(location.search).get("tab");
  if (names[tab]) selectTab(tab);
  if (new URLSearchParams(location.search).get("theme") === "dark") { document.documentElement.dataset.theme = "dark"; updateThemeLabel(); }
}
start();
