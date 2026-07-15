# PipelineColor

**Version 1.0.0** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Auto-colors pipelines, junctions, inline pumps, and matching pipe supports from the fluid currently in the network. Ships Customizer **PipelineColor** swatches, a SaveGame-backed swatch store, and metallic finishes for gases by default.

## How it works

- **Liquids:** wiki Primary colors + Default paint finish (`UPCSwatchDesc_*`, Customizer **PipelineColor** category)
- **Empty:** Neutral color swatch + Matte finish
- **Gases:** metallic PaintFinish by default (override per fluid or global defaults)
- **Supports:** floor / stackable / wall / wall-hole parents inherit the touched pipe’s fluid look (includes WallPipeSupports-style BP children)
- Updates on fluid descriptor changes; empty networks settle via budgeted scan
- Authority apply via `SetCustomizationData_Native` (replicates to clients)
- Customizer edits persist in the **world save** (SaveGame store / RCO)

## Chat commands

Type on the **server or listen host**. Commands start with `!` and do not appear in public chat. Responses use **Hal:** system messages.

- `!Metallic <fluid>` — toggle metallic finish for that fluid (saved to cfg)
- `!pchelp` — list Pipeline Color chat commands

Same verbs register with SML for **Chat Mk 2** expandable help. Fluid names match Customizer labels (e.g. `Water`, `PC Nitrogen Gas`).

## Config

No SML Mods menu. Edit `Configs/PipelineColor.cfg` on the host, use console `PipelineColor.Set`, or chat. See [Documentation/console-commands.md](Documentation/console-commands.md).

| Key | Default | Notes |
|-----|---------|-------|
| `DefaultGasMetallic` | `true` | Gases metallic when no per-fluid override |
| `DefaultLiquidMetallic` | `false` | Liquids metallic when no per-fluid override |
| `MetallicOverrides` | `{}` | Per catalog key (`Water`, `NitrogenGas`, …) |

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

**Required on remote** — all players need the same mod version (`^1.0.0`). Authority applies colors and config. See [Documentation/multiplayer.md](Documentation/multiplayer.md).

## Screenshots

![PipelineColor — fluid-colored pipe rows](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows.jpg)

![PipelineColor — wall and floor pipe supports](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-supports.jpg)

![PipelineColor — Customizer PipelineColor swatches](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/customizer-pc-swatches.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)

Docs index: [Documentation/README.md](Documentation/README.md)
