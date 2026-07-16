# Player Guide

## Overview

Structural Power injects a **hidden structural bus** into eligible buildables. Power travels through connected foundations, walls, ramps, stairs, walkways, beams, pillars, and bridge poles — without visible cables between those pieces.

**v3.1** adds opt-in **machine groups** (generators, storage, resources, production; transport/pipes stubs). You still use normal power poles and cables where a group is off or the mod does not apply.

The mod rewrites attach and reconcile on vanilla circuit APIs — **stable retroactive load** after earlier save-load pain. Also shipped: opt-in **structural lighting**, **I-key Id config**, **power-switch gating**, and **hoverpack structural tether**.

> **Welcome back if an older version put you off:** see the [README](../README.md) v2.1 note. Short version: retroactive wiring is **kept**; v3.0+ is worth retrying if you left on v2.1.

## What gets wired

**Structural bus members** (existing and newly placed):

- Foundations, ramps, stairs, walkways
- Walls and corner walls
- Beams and pillars
- Mod structural packs that inherit factory building without their own power component

**Bridge hosts** (connect visible grid / machines to the hidden bus):

- Ground power poles, wall outlets, power towers
- Power storage (when **Generation** is on)
- Generators (when **Generation** is on) — host a site bus on the foundation they sit on

## Retroactive on load

The mod **rebuilds its connectivity from world geometry** every time the save loads, so existing foundations, walls, ramps, and poles are tracked automatically — no rebuild or re-placement required after installing or updating.

## Typical workflow

1. Install the mod and load your save — existing structure and poles are wired on load.
2. Connect one point on the bus to your generator grid with a **visible cable** — the rest of the connected mesh shares that circuit.
3. Extend with new foundations or walls; they link to adjacent tracked structure when placed.
4. Snap a bridge pole (or place a generator with **Generation** on) to powered structure; the outlet bus merges with the structural mesh.

## Machines (opt-in)

**All machine groups default off.** Enable on the host with chat (`!Generation`, `!Resources`, …), console `StructuralPower.Set Group* 1`, or cfg. See [chat-commands.md](chat-commands.md).

| Group | Chat | What attaches |
|-------|------|----------------|
| **Generation** | `!Generation` | Generators + power storage as site bus hosts on structure |
| **Resources** | `!Resources` | Miners, water/oil extractors, fracking, geysers (consumers) |
| **Production** | `!Production` | Manufacturers, radar tower, AWESOME Sink (consumers) |
| **Transport** | `!Transport` | Wired stations / elevators / etc. — **stub** (no track topology) |
| **Pipes** | `!Pipes` | Pipeline pumps — **stub** (no pipe-run topology) |
| **Belts** | `!Belts` | Toggle only — **no attach** |

Consumers need a **bus host** on the same structure island (pole, storage, or generator). An isolated pad with only a miner and no gen/pole stays unattached until a host appears.

Vanilla wires still win: if a machine has a normal cable, structural attach yields to vanilla.

### Generator daisy chains (legacy saves)

When converting a save that wired generators in a chain (generator → generator → … → pole), removing wires while Generation is on can trip the fuse if several gens drop onto the bus in one jolt.

**Workaround:** start at the **far end** of the daisy chain (the last generator, farthest from the pole / grid) and unwire **one gen at a time**, working back toward the feed point. That lets each machine join the structural bus before the next release. HUB integrated biomass gens often share a short wire between siblings — same tip.

## Structural lighting

**Off by default.** Enable with chat `!lighting` on the host, console `StructuralPower.Set GroupLighting 1`, or `"GroupLighting": true` in `Configs/StructuralPower.cfg`.

When enabled:

- Lights on **powered** structure can draw from the hidden bus — no wire to every foundation.
- Use **lights control panels** and **named groups** for multiple zones on one build (assign group ids with the **I** panel below).

Without Group Lighting enabled, lights behave vanilla (wire required).

## Id config panel (I key)

**Global** feature on eligible / enabled components — not limited to structural lighting. Look at a supported buildable and press **I** to set **Source** / **Control** ids (light groups, switch subnets, and other keyed consumers as they ship).

| Target | Source | Control |
|--------|--------|---------|
| **Light** | Where power comes from (structural default, switch id, or light group) | — |
| **Power switch** | Structural feed | Switch gate id (building tag or override) |
| **Lights control panel** | Structural feed or switch id | Light group name (e.g. `Light1`) |

Ids are scoped to the **structure component** (connected island on the bus). Dropdowns list ids already in use on that island plus the component default.

Apply sends an RCO to the server; overrides persist in the **world save** (not in `.cfg`).

## Named light groups

1. Enable Group Lighting.
2. Place lights control panel on powered structure.
3. Press **I** on the panel — set **Control** to a group name (e.g. `Light1`).
4. Press **I** on each light — set **Source** to the same name.
5. Vanilla **E** on the panel still adjusts only lights in that keyed group — not every light on the structure.

## Switch subnets

When **Mode B** (`PowerSwitchManualGroups: 1`, default) is on:

- Assign a switch **Control** id (e.g. `Switch1`) via **I** or building tag.
- Set a panel or light **Source** to `Switch1`.
- Keyed consumers power only when that switch is **ON**.

Mode A (whole-component gate): set `PowerSwitchManualGroups` to `0` in `StructuralPower.cfg`.

## Hoverpack structural tether

The hoverpack can tether to **powered structure** geometry nearby — fly above, below, or beside your base with fewer poles.

Adjust reach on the server:

- Chat: `!HoverH` / `!HoverV` — see [chat-commands.md](chat-commands.md)
- Console: `StructuralPower.Set HoverpackStructuralHorizontalMultiplier` / `HoverpackStructuralVerticalMultiplier`
- cfg file on server/host

Defaults: **1.2×** base radius per axis (clamp 1.0–10.0).

## Isolated structures

Structure with **no adjacency** to the tracked graph stays on its own island. A pole on a solitary foundation does not propagate to the main factory unless you build a continuous structural chain between them.

## Save / load

Nothing structural is written to the save. On load the mod reconstructs the connectivity graph from the live world and re-establishes the hidden bus, so saves can never carry stale or "wireless" links from earlier builds.

Device **Source/Control** overrides (lights, switches, panels) are stored in the world save (graph subsystem), not in `StructuralPower.cfg`. Mod runtime components (outlet bus, panel control bus) are stripped and rebuilt on load.

## Settings

| Where | What |
|-------|------|
| `Configs/StructuralPower.cfg` | Server-persisted mod settings (JSON) |
| Chat `!` commands | Groups, hover, `!pwrhelp` — [chat-commands.md](chat-commands.md) |
| Console | `StructuralPower.Set` — [console-commands.md](console-commands.md) |

Per-device ids (lights, switches, panels) are **not** in `.cfg` — use **I** key or building tag.

Release history: [CHANGELOG](../CHANGELOG.md).
