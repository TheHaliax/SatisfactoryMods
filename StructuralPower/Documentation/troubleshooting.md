# Troubleshooting

## A bridge pole shows NO_BUS

`StructuralPower.Diag` counts a pole as `NO_BUS` when it has no outlet bus yet. On load, poles are processed over several frames — re-run the diag after a moment. If it persists, the pole is not a recognised bridge pole; only ground poles, wall outlets (single/double), and power towers qualify.

## A pole has a bus (WITH_BUS) but never IN_CIRCUIT

The outlet bus exists but the pole's structural component was never powered. A component only joins the game circuit once a pole in it is actually powered — wire one visible cable from your generator grid to the structure (or to any pole on it).

**Fresh pole on powered structure, empty circuit graph UI:** update to latest dev build — runtime place should log `[HALSP] pole BeginPlay` then `site integrate` before any wire. If only `pole wire delta` appears, attach pipeline did not run on place.

Common case: **isolated structure** — a foundation with no adjacency to the rest of your base stays on its own island. Not a bug if you kept it separate.

## Power not propagating

1. Verify continuous adjacency — no gaps in the foundation / wall / ramp chain between source and target.
2. Connect **one** visible cable from your generator grid to the structural bus (a pole or wired machine on a tracked piece).
3. Run `StructuralPower.Diag` — poles on powered structure should count under `IN_CIRCUIT`.
4. Enable trace in console (`StructuralPower.Trace 1`), place or rewire one pole, and grep the log for `[HALSP]`.

For switch/panel toggle debugging with trace on: grep `switch restitch_off_settled` — `passPanel=0` and `poweredDirect=0` mean OFF succeeded even if `armedPanel>0`. See [development.md](development.md#halsp-trace-developers).

## Structural lighting

| Symptom | Check |
|---------|--------|
| Lights need wires | **Structural lighting** off — enable with `!lighting` or `GroupLighting` in cfg |
| Light dark on powered base | Light `Source` id must match feed (structural default, switch id, or group); press **I** on light |
| E panel affects all lights | Panel needs keyed **Control** id; lights need matching **Source** — see [player-guide.md](player-guide.md#named-light-groups) |
| Switch subnet wrong | Mode B on (`PowerSwitchManualGroups: 1`); panel/light `Source` = switch Control id; switch ON |
| Dropdown missing switch id | Assign switch Control first; ids pool is structure-island scoped |

## Switch gating unexpected

1. Mode B (default): keyed subnets need matching building tag + device Id — **I** key or building tag.
2. Mode A: set `PowerSwitchManualGroups` to `0` in config for whole-component gate.

## Hoverpack tether

| Symptom | Check |
|---------|--------|
| No tether | Stand near **powered** structure within reach |
| Short reach | Raise `!HoverH` / `!HoverV` (max 10× per axis) or edit cfg multipliers |

## Save / reload issues

v3.0.0 strips persisted mod bus components before circuit bridge BeginPlay and rebuilds topology from geometry. If an older v2.1/v2.2 build failed on load, update to **3.0.0** and retry. Report with `FactoryGame.log` from load if it persists.

## No LogStructuralPower lines during gameplay

Trace is **off by default**. Enable `StructuralPower.Trace 1` in the console before testing.

## Menu map shows nothing in the log

Automatic diagnostics skip menu worlds (`Map_Menu_*`). Load a save or run `StructuralPower.Diag` manually.

## Reporting issues

Include:

- StructuralPower version (3.0.0)
- SML version
- Single-player, listen host, or dedicated server
- Group Lighting on/off
- Relevant `LogStructuralPower` / `[HALSP]` lines (with trace enabled if possible)
- Whether switches / lighting / hoverpack involved
