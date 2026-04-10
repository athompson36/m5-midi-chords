# Current roadmap and actionable todos

**Last synced:** 2026-04-09 (after pull to `b4ede12`, tag `v0.1.0` on GitHub)

**Authoritative backlog (implementation detail):** `docs/TODO_IMPLEMENTATION.md` — it is **ahead** of some checkboxes in `docs/DEV_ROADMAP.md` (Milestone 4 items are largely done in code; update `DEV_ROADMAP.md` when you next edit docs).

**Release:** `docs/RELEASE_NOTES_v0.1.0.md` — v0.1.0 shipped; hardware E2E sign-off still open.

---

## 1. Where the project stands

| Area | Status (high level) |
|------|---------------------|
| **Build / CI** | `pio run -e m5stack-cores3` + native tests in CI (`requirements.txt`, `.github/workflows/ci.yml`) |
| **Persistence** | NVS schema + migration (`prefsMigrateOnBoot`), table in `docs/PERSISTENCE_KEYS.md` |
| **MIDI stack** | USB (TinyUSB class device path), BLE, DIN UART; ingress + tagged dispatch; `docs/MIDI_STACK.md` |
| **Play** | Chords + MIDI out, transpose, voicing, categories, suggestions (ranked), IN monitor, corner BPM/flash |
| **Transport** | BPM/tap tempo, swing/humanize, clock follow, realtime send, dropdowns |
| **Sequencer** | Lanes, step picker (chord/tie/rest/surprise), MIDI playback, SeqExtras, Shift step params, `seqEx` v2 |
| **X–Y** | CC/channel/curve, 14-bit pairs, throttle, release policy |
| **Shift** | Hold-to-Shift, sequencer param nudge, Play CC/PC map (defaults; **user remap storage** still pending per TODO) |
| **Hardware proof** | **`docs/HARDWARE_E2E_CHECKLIST.md` not fully executed** — `docs/E2E_STATUS.md` records host builds + pending manual sign-off |

---

## 2. Roadmap by phase (execution order)

Use this as the **path to “done”** for the next stretch. Phases map to `TODO_IMPLEMENTATION.md`.

```
Done in repo for v0.1.0 scope: Phases 0–1, most of 2–4, 5–9, most of 10 (tag + notes).
Still open: hardware validation, doc sync, optional MIDI/UX polish, backlog items below.
```

### Phase A — Close the release loop (highest priority)

**Goal:** Evidence-backed v0.1.x (or v0.2.0) with signed-off hardware checklist.

| Step | Task | Owner / artifact |
|------|------|-------------------|
| A.1 | Run **`docs/HARDWARE_E2E_CHECKLIST.md`** on a physical CoreS3 | Tick boxes, fill git SHA + date |
| A.2 | Log results in **`docs/E2E_STATUS.md`** or checklist appendix | Dated block + PASS/FAIL |
| A.3 | **30‑minute soak** (no crash / obvious heap issues) | Same log |
| A.4 | **Two-controller MIDI** sanity (USB/BLE/DIN as available) | Note host + device models |
| A.5 | **Persistence reboot test** (BPM, key, seq step, settings) | Confirm NVS |
| A.6 | Optional: SD backup/restore round-trip | If SD present |

### Phase B — MIDI stack hardening (from `MIDI_STACK.md` + open `[~]` items)

**Goal:** Confidence across real hosts (phones, DAWs, DIN wiring).

| Step | Task | Notes |
|------|------|--------|
| B.1 | USB MIDI **enumeration matrix** (multiple hosts) | Track failures in `E2E_STATUS` or issues |
| B.2 | BLE interoperability (burst + realtime) | Already improved in recent commits; validate apps |
| B.3 | DIN: confirm **`docs/DIN_MIDI_CHIP_SETUP.md`** pinout on hardware | |
| B.4 | **Chord detection pipeline** — richer than triad/suggest v1 | `TODO_IMPLEMENTATION` Phase 2 |
| B.5 | **Suggestion ranking** tuning / weights | Optional; `MIDI Debug` supports A/B |
| B.6 | **Panic / all-notes-off** policy sweep | Review optional triggers as UX evolves (`MIDI_INPUT_SPEC.md`) |
| B.7 | Per-transport **OUT** diagnostics split | Marked optional in TODO |

### Phase C — Product backlog (features not fully closed)

| Step | Task | Source |
|------|------|--------|
| C.1 | **Shift + Play**: user-stored **CC/PC remap** (defaults exist) | Phase 7 in `TODO_IMPLEMENTATION` |
| C.2 | **Shift + Sequencer**: full persistence/engine for div/arp matrix | Partially `[~]` in TODO Phase 7–8 |
| C.3 | **DEV_ROADMAP Milestone 6** remaining: Shift UX polish, schema docs for any new fields | `DEV_ROADMAP.md` |
| C.4 | **README.md** sync: sequencer no longer “cycle only”; point at step picker + MIDI | Stale bullets ~L33–36 |

### Phase D — Documentation & repo hygiene

| Step | Task |
|------|------|
| D.1 | Align **`docs/DEV_ROADMAP.md`** Milestone 4 checkboxes with shipped firmware (or add “superseded by TODO_IMPLEMENTATION”) |
| D.2 | Keep **`docs/TODO_IMPLEMENTATION.md`** footer “Last updated” when stack changes |
| D.3 | Regenerate **`docs/GITHUB_ISSUES.md`** via `scripts/gen_github_issues.py` when backlog changes |
| D.4 | Refresh **`BUILD_INFO.md`** / committed **`firmware.bin`** when cutting releases (known issue in release notes) |

### Phase E — UI/UX polish (scroll, redraws, clock clarity, settings, seq tools)

**Primary doc:** **`docs/UI_UX_POLISH_BACKLOG.md`** (mirrored as **Phase 11** in `TODO_IMPLEMENTATION.md`).

| Theme | Intent |
|-------|--------|
| Scroll vs tap | Stop first-touch selecting a row while scrolling lists/dropdowns |
| Redraws | End full-screen flash on every touch; use partial invalidation where possible |
| Transport clock | Clarify single-route clock OUT vs multi-transport note broadcast; debug clkFollow / echo suppression |
| Settings + admin | Surface missing persisted options; optional advanced/admin unlock |
| Sequencer / arp | SELECT-driven tools; dropdowns + sliders; full SeqExtras coverage, finger-friendly |

---

## 3. Master todo lists (copy into issues if useful)

### Every step — hardware E2E (blocking “fully released”)

- [ ] Environment block: firmware SHA, date, SD yes/no (`HARDWARE_E2E_CHECKLIST.md`)
- [ ] Boot & navigation (ring, settings hold, save/exit persist)
- [ ] Play surface (key, pads, heart, padding/debounce)
- [ ] Sequencer (grid, lanes, tools, persistence) — update checklist text if “cycle” is obsolete
- [ ] Transport (play/stop/rec/metro/count-in)
- [ ] X–Y pad
- [ ] Persistence reboot
- [ ] Optional SD backup/restore
- [ ] MIDI: OUT + clock/thru as designed
- [ ] 30‑min soak
- [ ] Bright/dim readability sweep
- [ ] **Sign-off** line filled

### Every step — software / MIDI validation (can parallel A on bench)

- [ ] Re-run `pio test -e native` on each dev machine (ensure `gcc` in PATH on Windows per `E2E_STATUS`)
- [ ] `pio run -e m5stack-cores3` clean build
- [ ] Spot-check `MidiIngress` / provider tests after transport changes
- [ ] USB MIDI device on ≥2 hosts
- [ ] BLE to ≥1 synth app
- [ ] DIN loopback or keyboard if wired

### Every step — documentation

- [ ] `README.md` sequencer + “planned” section vs `TODO_IMPLEMENTATION`
- [ ] `DEV_ROADMAP.md` vs shipped (M4/M6)
- [ ] `E2E_STATUS.md` after each hardware session
- [ ] Release packaging: `firmware.bin` at repo root vs CI artifact path

### Every step — backlog / optional (nice-to-have)

- [ ] Shift pad **MIDI map** NVS + settings UI
- [ ] Sequencer Shift **per-step** arp/div **persistence** verification end-to-end
- [ ] X–Y **per-axis** release policy (global only today)
- [ ] `python3 scripts/create_github_issues.py` (requires `gh` auth) to mirror checkboxes

---

## 4. Suggested next actions (this week)

1. Execute **Phase A** on one CoreS3 and write results to **`E2E_STATUS.md`**.
2. Fix **README** sequencer paragraph to match firmware (step picker + MIDI playback).
3. Sweep **`DEV_ROADMAP.md`** Milestone 4–6 checkboxes against **`TODO_IMPLEMENTATION.md`** to avoid double sources of truth.
4. If anything fails hardware validation, file a **GitHub issue** per cluster (or regenerate `GITHUB_ISSUES.md`).

---

## 5. Quick reference — doc map

| File | Use |
|------|-----|
| `docs/TODO_IMPLEMENTATION.md` | Detailed phase checklist + dependency sketch (includes Phase 11 UI polish) |
| `docs/UI_UX_POLISH_BACKLOG.md` | Scroll/redraw/clock/settings/sequencer UX backlog |
| `docs/DEV_ROADMAP.md` | Milestones 1–6 (high level; may lag) |
| `docs/MIDI_INPUT_SPEC.md` | Transport, clock, panic policy |
| `docs/MIDI_STACK.md` | Current I/O implementation |
| `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md` | Seq / Shift / XY product spec |
| `docs/PERSISTENCE_KEYS.md` | NVS keys |
| `docs/HARDWARE_E2E_CHECKLIST.md` | Manual test procedure |
| `docs/E2E_STATUS.md` | Build + manual status log |
| `docs/GITHUB_ISSUES.md` | Generated issue stubs |
| `docs/DIN_MIDI_CHIP_SETUP.md` | DIN hardware |

---

*Generated to consolidate post–v0.1.0 state; edit this file as checkpoints complete.*
