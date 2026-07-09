# Player Guide

## Overview

Structural Power injects a **hidden structural bus** into eligible buildables. Power travels through connected foundations, walls, ramps, stairs, walkways, beams, pillars, and bridge poles — without visible cables between those pieces.

You still use normal power poles and cables where the mod does not apply (machines, generators, legacy geometry).

**v2.2** adds opt-in **structural lighting** and an **I-key Id config panel**. **v2.1** adds **structural power-switch gating** and a **hoverpack structural tether** (see below).

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

## Structural lighting (v2.2)

**Off by default.** Enable in **Pause → Mods → Structural Power → Debug → Structural lighting**, or chat `!lighting` on the host.

When enabled:

- Lights on **powered** structure can draw from the hidden bus — no wire to every foundation.
- Use **lights control panels** and **named groups** for multiple zones on one build.
- Press **I** while looking at a light, switch, or panel to assign **Source** / **Control** ids.

See [v2.2.md](v2.2.md) for group and switch-subnet setup.

## Power switches (v2.1)

Power switches on structures can **gate** keyed subnets (Mode B, default). Use building tags and matching device Ids to isolate sections. Optional pole-like bridge on switches; assign ids via **I** or building tag.

Toggle gating and Mode A/B in **Pause → Mods → Structural Power → Debug**.

## Hoverpack structural tether (v2.1)

When enabled, the hoverpack can tether to **powered structure** geometry nearby — fly above, below, or beside your base with fewer poles.

Adjust reach on the server:

- **Pause → Mods → Structural Power** — horizontal/vertical multipliers (main panel)
- Chat: `!HoverH` / `!HoverV` — see [chat-commands.md](chat-commands.md)

Defaults: **1.5×** base radius per axis (clamp 1.0–10.0). Disable tether in the **Debug** section.

## Isolated structures

Structure with **no adjacency** to the tracked graph stays on its own island. A pole on a solitary foundation does not propagate to the main factory unless you build a continuous structural chain between them.

## Save / load

Nothing structural is written to the save. On load the mod reconstructs the connectivity graph from the live world and re-establishes the hidden bus, so saves can never carry stale or "wireless" links from earlier builds.

Device **Source/Control** overrides (lights, switches, panels) are stored in the world save (graph subsystem), not in `StructuralPower.cfg`. Mod runtime components (outlet bus, panel control bus) are stripped and rebuilt on load.

## Settings

| Where | What |
|-------|------|
| Pause → Mods → Structural Power | Hover multipliers; **Debug** (collapsed) for propagation, switches, **lighting**, hoverpack tether, trace |
| `Configs/StructuralPower.cfg` | Server-persisted mod settings |
| Chat `!` commands | `!lighting`, `!HoverH`, `!HoverV`, `!pwrhelp` |
| Console | `StructuralPower.Set` — [console-commands.md](console-commands.md) |
