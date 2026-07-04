# Changelog

## 2.0.0 — 2026-07-03
Retroactive support for existing structures and a rebuilt connectivity engine.
The structural graph is now derived from live world geometry on load instead of
being persisted, so bases built before installing (or before an earlier version)
are powered correctly without rebuilding. Structural pieces are decoupled from
the power circuit — only bridge poles join the circuit — which removes the
load-time stutter and the false "power everywhere" behaviour seen in earlier
builds.
- Retroactive: existing foundations, walls, ramps, and poles are wired on load — no rebuild required
- Pole-to-pole architecture: structural pieces form a data-only connectivity graph (spatial hash + union-find); poles carry power between components and are promoted to the game circuit only when a pole in the component is actually powered
- Rebuild-from-geometry on load: structural links are no longer saved, eliminating stale/"wireless" links carried over from contaminated saves
- Performance: incremental component merge/split on build and demolish; no O(N²) circuit rebuilds while loading
- Retains the persistent-power fix and broad mesh support from 1.1.0

## 1.1.0 — 2026-07-03
More structure support and a fix for persistent power: a structure
disconnected from the active grid no longer stays powered. Incompatible with
structures built by earlier versions — only new builds from this update onward
behave correctly.
- Broader structural mesh support (thin / edge / beam / ramp segments propagate reliably)
- Persistent power fixed: disconnecting a structure from the active grid now drops its power instead of leaving it lit
- No retroactive fix for existing modded structures; rebuild affected structures after updating

## 1.0.0 — initial release
Hidden power network through connected foundations, walls, ramps, and bridge poles. Wire one point to your grid; the structural mesh shares the circuit without cables on every segment.
- New placements only (no retroactive save scan)
- Bridge poles link visible outlets to the hidden bus
- Graph persists on save/load
- MP: required on clients; dedicated server targets included

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.
