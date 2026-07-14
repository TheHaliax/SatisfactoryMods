# Player Guide

## Overview

Structural Power injects a **hidden structural bus** into eligible buildables. Power travels through connected foundations, walls, ramps, stairs, walkways, beams, pillars, and bridge poles — without visible cables between those pieces.

You still use normal power poles and cables where the mod does not apply (machines, generators, legacy geometry).

The mod rewrites attach and reconcile on vanilla circuit APIs — **stable retroactive load** after earlier save-load pain. Shipped today: opt-in **structural lighting**, **I-key Id config**, **power-switch gating**, and **hoverpack structural tether**.

> **Welcome back if an older version put you off:** see the [README](../README.md) v2.1 note. Short version: retroactive wiring is **kept**; v3.0 is worth retrying if you left on v2.1.

## What gets wired

**Structural bus members** (existing and newly placed):

- Foundations, ramps, stairs, walkways
- Walls and corner walls
- Beams and pillars
- Mod structural packs that inherit factory building without their own power component

**Bridge poles** (connect visible grid to the hidden bus):

- Ground power poles
- Wall outlets (single and double)
- Power towers

## Retroactive on load

The mod **rebuilds its connectivity from world geometry** every time the save loads, so existing foundations, walls, ramps, and poles are tracked automatically — no rebuild or re-placement required after installing or updating.

Pieces the mod does not recognise (machines, generators) keep working with normal Satisfactory wiring.

## Typical workflow

1. Install the mod and load your save — existing structure and poles are wired on load.
2. Connect one point on the bus to your generator grid with a **visible cable** — the rest of the connected mesh shares that circuit.
3. Extend with new foundations or walls; they link to adjacent tracked structure when placed.
4. Snap a bridge pole to powered structure; the pole's outlet bus merges with the structural mesh.

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
| Chat `!` commands | `!lighting`, `!HoverH`, `!HoverV`, `!pwrhelp` |
| Console | `StructuralPower.Set` — [console-commands.md](console-commands.md) |

Per-device ids (lights, switches, panels) are **not** in `.cfg` — use **I** key or building tag.

Release history: [CHANGELOG](../CHANGELOG.md).
