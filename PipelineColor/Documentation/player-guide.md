# Player Guide

## Overview

PipelineColor paints vanilla pipeline networks from the fluid currently in the pipe. Empty lines use Neutral; liquids and gases use built-in wiki colors. Gases get a metallic PaintFinish by default.

Matching **pipe supports** (floor, stackable, wall, wall-hole BP parents — including WallPipeSupports-style children) pick up the same look from a touching pipe.

## What gets colored

- Pipelines (Mk.1 / Mk.2)
- Pipeline junctions
- Inline pumps
- Pipe supports that inherit the fluid support parent classes above

Machines, tanks, and non-pipe buildables stay vanilla unless they are one of those supports.

## Typical workflow

1. Install the mod and load a save — existing networks scan and paint on load.
2. Fill or empty pipes; colors update as fluid descriptors change.
3. Open the **Customizer** → **PipelineColor** category to browse or edit PC swatches. Customizer edits persist in the **world save**.
4. Place or reposition supports against colored pipes — they inherit the pipe Spec after the pipe applies.

## Metallic finishes

At **apply time**, the mod chooses metallic vs default/matte finish from:

1. Per-fluid override (`MetallicOverrides` / `!Metallic` / `PipelineColor.Set Metallic.<Key>`)
2. Else defaults: gases metallic, liquids not (`DefaultGasMetallic` / `DefaultLiquidMetallic`)

Colored fluids keep their pigment when metallic. Near-neutral greys remap along a silver rail (bright like Alumina → chrome; darker greys → burnished). Empty **Neutral** uses Matte unless you turn Neutral metallic; Neutral metallic uses roughness `1.0`, Neutral matte/color uses roughness `4.0`.

Chat:

- `!Metallic <fluid>` — toggle that fluid
- `!Metallic all on` — stamp metallic **on** for every catalog fluid (overrides, not the gas/liquid default flags)
- `!Metallic all off` — stamp metallic **off** (color) for every catalog fluid the same way
- `!Metallic default` — clear those overrides, restore gas-on / liquid-off defaults, and reseed Customizer store colors from catalog

`all on` / `all off` leave `DefaultGasMetallic` / `DefaultLiquidMetallic` alone until `default`.

This is **not** the Customizer Secondary RGB slot. Changing Secondary in the Customizer stores color data in the SaveGame store; metallic sheen is finish + config.

## Empty / Neutral

When a network has no fluid, pipes receive the Neutral swatch (Matte by default). Budgeted empty scans catch transitions after drain.

## Config surfaces

| Surface | Use |
|---------|-----|
| `Configs/PipelineColor.cfg` | Host JSON — defaults + metallic overrides |
| Console `PipelineColor.Set` | Same keys on authority |
| Chat `!Metallic` / `!pchelp` | Toggle, `all on`, `all off`, `default`, help |

There is **no** SML Mods configuration menu.

## Related

- [Chat commands](chat-commands.md)
- [Console commands](console-commands.md)
- [Multiplayer](multiplayer.md)
- [Troubleshooting](troubleshooting.md)
