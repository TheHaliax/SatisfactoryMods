# PipelineColor

**Version 1.1.2** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Auto-colors pipelines, junctions, inline pumps, and matching pipe supports from the fluid currently in the network. Ships Customizer **PipelineColor** swatches, a SaveGame-backed swatch store, and metallic finishes for gases by default.

## How it works

- **Liquids:** wiki Primary colors + Default paint finish (`UPCSwatchDesc_*`, Customizer **PipelineColor** category)
- **Empty:** Neutral swatch + Matte (metallic optional via `!Metallic Neutral`)
- **Gases:** metallic PaintFinish by default (override per fluid or global defaults)
- **Metallic chromatics:** keep catalog hue/value; neutrals remap along a silver rail (bright → chrome, dark → burnished)
- **Supports:** floor / stackable / wall / wall-hole parents inherit the touched pipe’s fluid look (includes WallPipeSupports-style BP children)
- Updates when fluid changes; empty networks settle after a short scan
- Server/host applies paint; clients receive the result
- Customizer edits persist in the **world save**

## Chat commands

Type on the **server or listen host**. Commands start with `!` and do not appear in public chat. Responses show as **Hal:** system messages.

- `!Metallic <fluid>` — toggle metallic finish for that fluid (saved to cfg)
- `!Metallic all on` — force every catalog fluid metallic on (per-fluid overrides)
- `!Metallic all off` — force every catalog fluid metallic off / color (same)
- `!Metallic default` — clear overrides, restore gas-on / liquid-off defaults, reseed store colors
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

**Required on remote** — all players need the same mod version (`^1.1.2`). Authority applies colors and config. See [Documentation/multiplayer.md](Documentation/multiplayer.md).

## Screenshots

![PipelineColor — fluid-colored pipe rows](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows.jpg)

![PipelineColor — color finishes (matte)](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows-matte.jpg)

![PipelineColor — metallic finishes](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows-metallic.jpg)

![PipelineColor — wall and floor pipe supports](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-supports.jpg)

![PipelineColor — Customizer PipelineColor swatches](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/customizer-pc-swatches.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)

Docs index: [Documentation/README.md](Documentation/README.md)
