import test from "node:test";
import assert from "node:assert/strict";
import {parseMacro, serializeMacro, normalizeMacro, duration, transitionsAt} from "../macro.js";

test("round trips presses and releases", () => {
 const original = "SWIFT1 240\n0 1 0 1\n12 1 0 0\n12 1 1 1\n";
 assert.equal(serializeMacro(parseMacro(original)), original);
});
test("preserves same-tick event order", () => {
 const m=normalizeMacro({tps:240,events:[
  {tick:5,button:1,player2:0,down:0},
  {tick:1,button:1,player2:0,down:1},
  {tick:5,button:1,player2:0,down:1}
 ]});
 assert.deepEqual(m.events.map(e=>e.down),[1,0,1]);
});
test("rejects malformed unsafe and out-of-order input", () => {
 for(const bad of ["SWIFT1 240\n9 1 0 1\n4 1 0 0", "SWIFT1 240\n0 9 0 1",
  "SWIFT1 240\n0 1 0 3","SWIFT1 240\n9007199254740993 1 0 0","SWIFT1 0\n"]) {
  assert.throws(()=>parseMacro(bad), {name:"Error"});
 }
});
test("duration and held button states", () => {
 const m=parseMacro("SWIFT1 240\n0 1 0 1\n120 1 0 0\n240 1 1 1\n");
 assert.equal(duration(m),1);
 assert.equal(transitionsAt(m,60).get("0:1"),true);
 assert.equal(transitionsAt(m,200).get("0:1"),false);
 assert.equal(transitionsAt(m,240).get("1:1"),true);
});
