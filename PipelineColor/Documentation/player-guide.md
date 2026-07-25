# Player Guide

## Overview

PipelineColor paints vanilla pipeline networks from the fluid currently in the pipe. Empty lines use **PC Empty Pipe**; unknown fluids use **PC ERR0R** (magenta). Liquids and gases take RGB from the fluid descriptor (`mFluidColor` / `mGasColor`, selectable per fluid). Gases get a metallic PaintFinish by default. Auto fluid color always reapplies — Customizer / paint-gun colors on pipe targets do not stick against fluid updates.

Matching **pipe supports** (floor, stackable, wall, wall-hole BP parents — including WallPipeSupports-style children) pick up the same look from a touching pipe.

Any installed mod that ships `RF_LIQUID` / `RF_GAS` item descriptors (including [Satisfactory Plus](https://ficsit.app/mod/SatisfactoryPlus) and [Refined Power](https://ficsit.app/mod/RefinedPower)) gets ClassGen swatches under a Customizer subcategory named after that mod’s FriendlyName. Without those mods, their sections do not appear.

## What gets colored

- Pipelines (Mk.1 / Mk.2)
- Pipeline junctions
- Inline pumps
- Pipe supports that inherit the fluid support parent classes above

Machines, tanks, and non-pipe buildables stay vanilla unless they are one of those supports.

## Typical workflow

1. Install the mod and load a save — existing networks scan and paint on load.
2. Fill or empty pipes; colors update as fluid descriptors change.
3. Open the **Customizer** → **PipelineColor** category to browse or edit PC swatches. **Only edits you make** are written into the world save; everything else uses live catalog colors.
4. Place or reposition supports against colored pipes — they inherit the pipe Spec after the pipe applies.

**Upgrade note (1.3.0):** older SaveGame swatch stores (schema &lt; 4) are cleared once. Older `PipelineColor.cfg` (schema &lt; 2) is fully reseeded (prior metallic / color-source overrides discarded).

## Color source (RGB only)

Per fluid, paint RGB comes from either `mFluidColor` or `mGasColor`:

1. Per-fluid override (`ColorSourceOverrides` / `!pc <fluid> liquid|gas` / `PipelineColor.Set ColorSource.<Key>`)
2. Else defaults: **liquid** for every catalog key except **Nitrogen Gas** (`FactoryGame_NitrogenGas`) → **gas**

This does **not** change metallic. Metallic still follows `RF_GAS` / `!Metallic` / metallic cfg.

Catalog keys look like `FactoryGame_Water`. Chat also accepts display names (`Water`, `Nitrogen Gas`).

## Metallic finishes

At **apply time**, the mod chooses metallic vs default/matte finish from:

1. Per-fluid override (`MetallicOverrides` / `!Metallic` / `PipelineColor.Set Metallic.<Key>`)
2. Else defaults: gases metallic, liquids not (`DefaultGasMetallic` / `DefaultLiquidMetallic`)

Colored fluids keep their pigment when metallic. Near-neutral greys remap along a silver rail (bright like Alumina → chrome; darker greys → burnished). Empty **PC Empty Pipe** uses Matte unless you turn Neutral metallic; Neutral metallic uses roughness `1.0`, Neutral matte/color uses roughness `4.0`.

Chat:

- `!Metallic <fluid>` — toggle that fluid
- `!Metallic all on` — stamp metallic **on** for every catalog fluid (overrides, not the gas/liquid default flags)
- `!Metallic all off` — stamp metallic **off** (color) for every catalog fluid the same way
- `!Metallic default` — clear those overrides and restore gas-on / liquid-off defaults. Your Customizer swatch edits are untouched
- `!pc <fluid> liquid|gas` — paint from `mFluidColor` or `mGasColor` (metallic unchanged)
- `!pc default` — clear every custom PC swatch edit (catalog defaults until you paint again)

`all on` / `all off` leave `DefaultGasMetallic` / `DefaultLiquidMetallic` alone until `default`.

This is **not** the Customizer Secondary RGB slot. Changing Secondary in the Customizer stores color data in the SaveGame store; metallic sheen is finish + config.

## Empty / unknown

| Label | Catalog key | Look |
|-------|-------------|------|
| **PC Empty Pipe** | `Neutral` | Matte grey when the network has no fluid |
| **PC ERR0R** | `Fallback` | Magenta (`#FF00FF`) for unknown / unresolved fluids |

Budgeted empty scans catch transitions after drain.

## Config surfaces

| Surface | Use |
|---------|-----|
| `Configs/PipelineColor.cfg` | Host JSON — metallic + color-source overrides, defaults (`CfgSchema`) |
| Console `PipelineColor.Set` | Same keys on authority |
| Chat `!Metallic` / `!pc` / `!pchelp` | Metallic; color source; clear customs; help |

There is **no** SML Mods configuration menu.

## Related

- [Chat commands](chat-commands.md)
- [Console commands](console-commands.md)
- [Multiplayer](multiplayer.md)
- [Troubleshooting](troubleshooting.md)
