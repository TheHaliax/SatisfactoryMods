# Player Guide

## Overview

Structural Power injects a **hidden structural bus** into eligible buildables. Power travels through connected foundations, walls, ramps, stairs, walkways, beams, pillars, and bridge poles — without visible cables between those pieces.

You still use normal power poles and cables where the mod does not apply (machines, generators, legacy geometry).

## What gets wired

**Structural bus members** (new placements after mod install):

- Foundations, ramps, stairs, walkways
- Walls and corner walls
- Beams and pillars
- Mod structural packs that inherit factory building without their own power component

**Bridge poles** (connect visible grid to the hidden bus):

- Ground power poles
- Wall outlets (single and double)
- Power towers

## Placement-only rule

The mod **does not scan** your existing save on install. Only structures and poles placed **after** StructuralPower is active are tracked.

Pre-mod foundations and poles keep working with normal Satisfactory wiring but do not join the hidden bus unless you rebuild or extend from new tracked pieces.

## Typical workflow

1. Install the mod and load your save.
2. Extend from existing builds using **new** foundations or walls — they link to adjacent tracked structure when placed.
3. Snap a bridge pole to powered structure; the pole's outlet bus merges with the structural mesh.
4. Connect one point on the bus to your generator grid with a **visible cable** — the rest of the connected mesh shares that circuit.

## Isolated structures

Structure with **no adjacency** to the tracked graph stays on its own island. A pole on a solitary foundation does not propagate to the main factory unless you build a continuous structural chain between them.

## Save / load

Graph nodes and lightweight connector keys are stored in the mod subsystem. On load, hidden links are restored from save data; the log reports how many links were already present vs newly added.
