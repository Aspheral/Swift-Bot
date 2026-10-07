// Swift Macro v1: deterministic *whole-tick* event schema.
// This module intentionally makes no claim of click-between-frames precision.
export const MAX_EVENTS = 200000;
const integer = (value, label, min, max) => {
  if (!Number.isSafeInteger(value) || value < min || value > max) {
    throw new Error(`Invalid ${label}: ${value}`);
  }
  return value;
};

export function normalizeMacro(macro) {
  if (!macro || !Array.isArray(macro.events)) throw new Error("Invalid macro");
  const tps = integer(Number(macro.tps), "TPS", 1, 100000);
  if (macro.events.length > MAX_EVENTS) throw new Error("Macro exceeds event limit");
  const events = macro.events.map((e, i) => ({
    tick: integer(Number(e.tick), `tick at row ${i + 1}`, 0, Number.MAX_SAFE_INTEGER),
    button: integer(Number(e.button), "button", 1, 3),
    player2: integer(Number(e.player2), "player2", 0, 1),
    down: integer(Number(e.down), "down", 0, 1),
  }));
  events.sort((a, b) => a.tick - b.tick); // stable: preserves simultaneous events
  return { tps, events };
}

export function parseMacro(text) {
  if (typeof text !== "string" || text.length > 8000000) throw new Error("File too large");
  const lines = text.replace(/\r/g, "").split("\n").map(l => l.trim()).filter(Boolean);
  const match = /^SWIFT1\s+(\d+)$/.exec(lines.shift() || "");
  if (!match) throw new Error("Expected SWIFT1 header");
  const events = [];
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].startsWith("#")) continue;
    const parts = lines[i].split(/\s+/);
    if (parts.length !== 4 || parts.some(p => !/^\d+$/.test(p)))
      throw new Error(`Malformed event at line ${i + 2}`);
    events.push({tick:Number(parts[0]),button:Number(parts[1]),player2:Number(parts[2]),down:Number(parts[3])});
  }
  for (let i = 1; i < events.length; i++) {
    if (events[i].tick < events[i-1].tick) throw new Error("Events must be time ordered");
  }
  return normalizeMacro({tps:Number(match[1]),events});
}

export function serializeMacro(macro) {
  const clean = normalizeMacro(macro);
  return [`SWIFT1 ${clean.tps}`, ...clean.events.map(e =>
    `${e.tick} ${e.button} ${e.player2} ${e.down}`)].join("\n") + "\n";
}

export function duration(macro) {
  return macro.events.length ? macro.events[macro.events.length - 1].tick / macro.tps : 0;
}

export function transitionsAt(macro, tick) {
  const held = new Map();
  for (const e of macro.events) {
    if (e.tick > tick) break;
    held.set(`${e.player2}:${e.button}`, !!e.down);
  }
  return held;
}
