# M5 MIDI Chords — Cleanup + UI Upgrade Drop-In Pack

This package is meant to be copied into the root of the current repository.

## What this pack contains

- `docs/REPO_AUDIT_AND_CLEANUP_PLAN.md`
- `docs/UI_REFACTOR_MASTER_PLAN.md`
- `docs/UI_COMPONENT_SPEC.md`
- `docs/UI_IMPLEMENTATION_MAP.md`
- `docs/CURSOR_TASK_BREAKDOWN.md`
- `CURSOR_RULES.md`
- `CURSOR_CONTEXT.md`

## Intended use

1. Copy these files into the repo.
2. Treat `docs/REPO_AUDIT_AND_CLEANUP_PLAN.md` as the current cleanup brief.
3. Treat `docs/UI_REFACTOR_MASTER_PLAN.md` as the UI refactor source of truth.
4. Use `CURSOR_CONTEXT.md` + `CURSOR_RULES.md` to guide the agent during implementation.
5. Work in small slices:
   - doc cleanup
   - UI foundation extraction
   - X-Y drawer
   - Play drawer
   - Sequencer drawer
   - old UI cleanup

## Primary goals

- reduce documentation drift
- reduce `main.cpp` responsibility
- establish a reusable glossy component system
- move secondary controls into top drawers
- preserve all existing MIDI / transport / persistence behavior

## Recommended first commit

Add these docs first without changing code. Then begin the UI extraction work in a separate commit series.
