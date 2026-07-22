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

For switch/panel toggle debugging with trace on: grep `switch restitch_off_settled` — `passPanel=0` and `poweredDirect=0` mean OFF succeeded even if `armedPanel>0`. See [development.md](development.md#trace-developers).

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

## Machines not attaching

| Symptom | Check |
|---------|--------|
| Gen / miner stays wired-only | Group toggle off — enable `!Generation` / `!Resources` / `!Production` |
| Miner / mfg on foundation, no power | Need a **bus host** on same structure island (pole, storage, or generator with Generation on) |
| Log `machine_no_bridge_bus` | Same — no OutletBus peer on that root yet |
| Log `gen_attach_failed` / `gen_bus_create_failed` | Update to **3.1.0+** (gens create their own bus); confirm Generation on and foundation parent resolves |
| Vanilla cable present | Structural attach yields to vanilla wires |

## Save / reload issues

v3.0+ strips persisted mod bus components before circuit bridge BeginPlay and rebuilds topology from geometry. If an older v2.1 build failed on load, update to **3.1.0** and retry. Report with `FactoryGame.log` from load if it persists.

## satisfactory-calculator.com (SCIM) rejects the save

Older builds stored Id-panel data in a format the SCIM save parser cannot read
("Something went wrong... Source: readStructProperty"). Update StructuralPower, load the
save once, save again — the data migrates to a SCIM-safe format. Removing the mod does
**not** fix an already-affected save: the game preserves mod data blobs across resaves, so
the old-format blob stays until a resave happens with the updated mod installed.

## No LogStructuralPower lines during gameplay

Trace is **off by default**. Enable `StructuralPower.Trace 1` in the console before testing.

## Menu map shows nothing in the log

Automatic diagnostics skip menu worlds (`Map_Menu_*`). Load a save or run `StructuralPower.Diag` manually.

## Reporting issues

Include:

- StructuralPower version (**3.1.1**)
- SML version
- Single-player, listen host, or dedicated server
- Which group toggles on (`!Generation`, `!Resources`, `!lighting`, …)
- Relevant `LogStructuralPower` / `[HALSP]` lines (with trace enabled if possible)
- Whether switches / lighting / hoverpack / machines involved
