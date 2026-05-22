# P1: DFOS Relay as Logos Module

**Phase:** P1 (first of four)
**Status:** In progress
**Branch:** `phase/P1-module` on `feat/dfos-logos-prototype`

---

## Goal

Build a DFOS relay that runs as a `logos_host` subprocess, managed by `liblogos_core`. Prove the module lifecycle works.

---

## What We Build

A DFOS relay module that:
1. Runs as a separate `logos_host` subprocess (not in-process)
2. Is managed by `liblogos_core` (same mechanism as pkg_mgr, capability, waku, storage)
3. Exposes a C API for Basecamp to interact with it (load, unload, query, send operations)
4. Uses the same operation model as the current HTTP relay (but will be extended for gossipsub in P2)

---

## Key Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Process model | Separate process (logos_host) | Isolation, Keycard integration easier |
| Module management | Same logos_host mechanism | Reuse existing infrastructure |
| API | C API (logos_core_*) | Basecamp already links liblogos_core |
| Operation model | Same as HTTP relay | P2 will swap transport, not operations |

---

## Files to Create/Modify

### New files
- `modules/dfos_relay/` — DFOS relay module directory
  - `dfos_relay.c` — Main relay implementation (C API)
  - `dfos_relay.h` — Header with C API
  - `CMakeLists.txt` — Build config
  - `README.md` — Module documentation

### Modified files
- `modules/CMakeLists.txt` — Add dfos_relay to module list
- `app/main.cpp` — Add `--module dfos_relay` CLI arg
- `src/MainUIBackend.cpp` — Add module loading for dfos_relay

---

## Acceptance Criteria

- [ ] DFOS relay compiles as a Logos module
- [ ] Module loads via `logos_core_load_module("dfos_relay")`
- [ ] Module starts, runs, stops, unloads cleanly
- [ ] Module stats (CPU, memory) exposed via `logos_core_get_module_stats()`
- [ ] Module crash doesn't crash Basecamp (process isolation verified)
- [ ] `--module dfos_relay` CLI arg works
- [ ] Smoke test passes

---

## What We DON'T Do (Yet)

- Not replacing HTTP gossip (that's P2)
- Not integrating with Keycard (that's P4)
- Not integrating with Logos Storage (that's P3)
- Not adding Basecamp UI apps (that's P4)

---

## Dependencies

- None (standalone phase)

---

## Build

```bash
nix build '.#dfos-relay-module'
nix build '.#app'
./result/bin/LogosBasecamp --module dfos_relay
```

---

## Related Docs

| Doc | Location |
|-----|----------|
| Prototype plan | `research/dfos-logos-integration.md` |
| Basecamp architecture | `research/logos-basecamp-architecture.md` |
| DFOS protocol overview | [protocol.dfos.com](https://protocol.dfos.com/) |
