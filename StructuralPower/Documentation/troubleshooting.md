# Troubleshooting

## Legacy poles show NO_PARENT / LEGACY_UNTRACKED

Expected for poles placed **before** the mod was installed. They use normal cable power only. Place new poles on tracked structure to use the hidden bus.

## NO_COMPONENT on a bridge pole

Means: parent structure was resolved, but the mod outlet bus or parent hidden connector is missing.

Common case: **isolated test foundation** — pole on structure not connected to the tracked graph. Not a bug if you intentionally kept the island separate.

## PostLoadGame: "0 links newly added, N already present"

Normal on reload. Hidden connections persist in save data; rebuild confirms they are still linked. **Already present** is success, not failure.

## No LogStructuralPower lines during gameplay

Trace is **off by default**. Enable `StructuralPower.Trace 1` before testing placements.

## Menu map shows nothing in log

Automatic diagnostics skip menu worlds (`Map_Menu_*`). Load a save or run `StructuralPower.Diag` manually.

## Power not propagating

1. Confirm structure was placed **after** mod install.
2. Verify continuous adjacency — no gaps in foundation/wall chain.
3. Connect **one** visible cable from generator grid to the structural bus (pole or wired machine on tracked piece).
4. Run `StructuralPower.Diag` — tracked poles should appear under **OK**.
5. Enable trace, place one new foundation, grep log for `[PWR]`.

## Reporting issues

Include:

- StructuralPower version (1.0.0)
- SML version
- Single-player or dedicated server
- Relevant `LogStructuralPower` lines (with `StructuralPower.Trace 1` if possible)
- Whether structures were pre-mod or post-mod
