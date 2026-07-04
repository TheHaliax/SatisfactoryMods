# Player Guide

## Overview

Structural Power injects a **hidden structural bus** into eligible buildables. Power travels through connected foundations, walls, ramps, stairs, walkways, beams, pillars, and bridge poles — without visible cables between those pieces.

You still use normal power poles and cables where the mod does not apply (machines, generators, legacy geometry).

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

## Isolated structures

Structure with **no adjacency** to the tracked graph stays on its own island. A pole on a solitary foundation does not propagate to the main factory unless you build a continuous structural chain between them.

## Save / load

Nothing structural is written to the save. On load the mod reconstructs the connectivity graph from the live world and re-establishes the hidden bus, so saves can never carry stale or "wireless" links from earlier builds.
