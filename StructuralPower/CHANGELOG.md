# Changelog

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
