# Troubleshooting

## A bridge pole shows NO_BUS

`StructuralPower.Diag` counts a pole as `NO_BUS` when it has no outlet bus yet. On load, poles are processed over several frames — re-run the diag after a moment. If it persists, the pole is not a recognised bridge pole; only ground poles, wall outlets (single/double), and power towers qualify.

## A pole has a bus (WITH_BUS) but never IN_CIRCUIT

The outlet bus exists but the pole's structural component was never powered. A component only joins the game circuit once a pole in it is actually powered — wire one visible cable from your generator grid to the structure (or to any pole on it).

Common case: **isolated structure** — a foundation with no adjacency to the rest of your base stays on its own island. Not a bug if you kept it separate.

## Power not propagating

1. Verify continuous adjacency — no gaps in the foundation / wall / ramp chain between source and target.
2. Connect **one** visible cable from your generator grid to the structural bus (a pole or wired machine on a tracked piece).
3. Run `StructuralPower.Diag` — poles on powered structure should count under `IN_CIRCUIT`.
4. Enable trace (`StructuralPower.Trace 1`), place or rewire one pole, and grep the log for `[PWR]`.

## No LogStructuralPower lines during gameplay

Trace is **off by default**. Enable `StructuralPower.Trace 1` before testing.

## Menu map shows nothing in the log

Automatic diagnostics skip menu worlds (`Map_Menu_*`). Load a save or run `StructuralPower.Diag` manually.

## Reporting issues

Include:

- StructuralPower version (2.0.0)
- SML version
- Single-player or dedicated server
- Relevant `LogStructuralPower` / `[PWR]` lines (with `StructuralPower.Trace 1` if possible)
