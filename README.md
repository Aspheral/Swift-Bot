# Swift Bot

**A Geometry Dash showcase and replay workstation for impossible levels.** Built by Aspheral.

> **Early prototype, not a finished autoplay bot.** The current native Geode source records/replays whole physics-step inputs and experiments with continuous gameplay time scaling. CBF sub-step capture, verified TPS conversion, full state snapshots and accurate trajectory prediction are *not* implemented yet.

## What is in this repository?

- **Geode prototype** (Windows / GD 2.2081 / Geode v5.10.1): in-game REC, STOP and PLAY buttons, with live P1/P2 event counters; whole-physics-tick recording and playback on separate P1 and P2 channels; writes `latest.swift` in the mod save directory; adjustable experimental continuous simulation slow-motion.
- **Web macro lab** (`index.html`): load, inspect, edit, validate and export `.swift` files, plus a playback timeline inspector. This website **cannot control the installed game**.
- **Pure JavaScript macro format library** and automated tests.

### Getting the Geode mod running

1. Install Geometry Dash 2.2081, [Geode](https://geode-sdk.org/) and the [Geode CLI](https://github.com/geode-sdk/cli).
2. Set up the Geode SDK as shown in the [official docs](https://docs.geode-sdk.org/getting-started/).
3. Run `geode build` from this repository's root directory.
4. Install the generated `.geode` package using Geode and launch a level.
5. Use **REC** to restart and record; **STOP** to save; **PLAY** to restart and play the last recording.
6. Open the mod's save folder and import `latest.swift` into the web editor.

**Warning:** This source has not yet been validated by compiling against a local Windows Geode SDK or tested in GD. Treat the native portion as a source prototype, not a working binary. Speed scaling through Cocos scheduling is experimental; GD's internal minimum physics step behavior and audio sync must be measured before claiming extreme-slow-motion support. Avoid simultaneous timewarp/TPS/CBF mods for initial testing.

### Replay format, v1

UTF-8 text, exactly:

```text
SWIFT1 240
0 1 0 1
12 1 0 0
```

Header: `SWIFT1 <recorded_tps>`. Then one event per line: `tick button player2 down`.
All values are nonnegative integers. `button` is 1, 2 or 3; `player2` and `down` are 0 or 1. Events sharing a tick preserve their file order. Timing is **whole-tick**, not CBF. Replay is only intended at the **recorded TPS**, no silent conversion.

### Current limitations

- No verified determinism, level fingerprint, position anchors, RNG locking, start-position restoration or desync recovery yet.
- No integrated CBF timing or TPS bypass. The TPS field stores metadata, and the native recorder targets GD's default 240 TPS.
- No automatic route solving; future trajectory tools must use authoritative native simulation, not a browser parabola.
- Slow motion defaults to **1× (off)** and scales the Cocos scheduler delta when configured; it is not guaranteed to reduce GD's physics ticks when GD forces a minimum update count.
- The website is a **macro editor and timeline**, not a game renderer or an actual trajectory predictor.
- Recording/playback should only be used for showcases, with completions clearly marked as bot-assisted.

## Roadmap

1. Native hook compile check + deterministic 240 TPS same-session replay
2. Correct simulation clock independent of rendering (including a test of 0.01× / 0.001×)
3. CBF input phase support with matching injection semantics
4. State snapshots, desync diagnostics, level fingerprinting
5. Sandboxed physics trajectory branching and collisions
6. Full macro editor integration, checkpoints, video showcase pipeline

## Web development

`npm test` runs the macro format unit tests. The web app uses no build step or runtime dependencies and is suitable for static hosting on Vercel.

## Two-player recording and verification (v0.2)

Each input stores `player2=0` for **P1** or `player2=1` for **P2**. The pressed-state tracker is independent for each player and each of Jump, Left, and Right, including when both press the same button on the same tick. The in-game HUD displays live P1/P2 event totals. The website provides separate press/release statistics, player filtering and individual P1/P2 input visualization.

Both the **standalone C++ replay core** and **JavaScript macro tests** cover simultaneous two-player input, independent release timing, preservation of same-tick ordering, and replay output. These tests do NOT establish successful integration with Geometry Dash's live 2-player input routing.

To verify in GD:
1. Use a 2-player level with separately mapped controls.
2. Start REC, tap P1 alone, tap P2 alone, then hold both, releasing P1 before P2.
3. Confirm the HUD increments P1 and P2 separately.
4. STOP, import `latest.swift` into the Macro Lab, inspect both channels.
5. PLAY in GD and compare the recorded behavior.
6. Repeat after dying/resetting; verify that only the most recent attempt remains.

The mod captures **the player2 flag passed by GD's input handler**. It cannot distinguish two hardware keys if GD itself maps both to the same player. CBF sub-tick events and determinism/desync verification are future work.

