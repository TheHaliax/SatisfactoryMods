# PipelineColor

**Version 1.3.1** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Auto-colors pipelines from network fluid. Dynamic Customizer swatches, custom-only store, metallic gases by default.

## How it works

- **Discovery:** every loaded `RF_LIQUID` / `RF_GAS` item descriptor (vanilla + mods) → ClassGen swatch + recipe; Customizer subcategory = mod FriendlyName (e.g. **Default**, **Refined Power**)
- **Catalog keys:** `OwnerMod_Stem` (e.g. `FactoryGame_Water`, `RefinedPower_RP_…`). UI labels use item display names
- **Liquids / gases:** RGB from `mFluidColor` / `mGasColor` (`!pc <fluid> liquid|gas`); gases metallic by default (`!Metallic`)
- **Empty:** **PC Empty Pipe** (`Neutral`) + Matte
- **Unknown:** **PC ERR0R** (`Fallback`) + magenta (`#FF00FF`)
- **Store:** SaveGame keeps only Customizer-edited swatches; unedited fluids use live catalog defaults (Customizer icons still show catalog colors)
- **Supports:** floor / stackable / wall / wall-hole parents inherit the touched pipe’s fluid look
- Server/host applies paint; clients receive the result

## Chat commands

Type on the **server or listen host**. Commands start with `!` and do not appear in public chat. Responses show as **Hal:** system messages.

- `!Metallic <fluid>` / `all on` / `all off` / `default` — metallic flags only (cfg)
- `!pc <fluid> liquid|gas` — RGB color source (metallic untouched)
- `!pc default` — clear custom SaveGame swatch edits (catalog defaults)
- `!pchelp` — short list

Same verbs register with SML for **Chat Mk 2** expandable help. Full list: [Documentation/chat-commands.md](Documentation/chat-commands.md).

## Config

No SML Mods menu. Edit `Configs/PipelineColor.cfg` on the host, use console `PipelineColor.Set`, or chat. See [Documentation/console-commands.md](Documentation/console-commands.md).

| Key | Default | Notes |
|-----|---------|-------|
| `CfgSchema` | `2` | Prefixed catalog keys; older cfg full-reseeds on load |
| `DefaultGasMetallic` | `true` | Gases metallic when no per-fluid override |
| `DefaultLiquidMetallic` | `false` | Liquids metallic when no per-fluid override |
| `MetallicOverrides` | `{}` | Per catalog key (`FactoryGame_Water`, …) |
| `ColorSourceOverrides` | `{}` | Per catalog key (`liquid` / `gas`); default seed includes `FactoryGame_NitrogenGas` → gas |

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

**Required on remote** — all players need the same mod version (`^1.3.0`). Authority applies colors and config. See [Documentation/multiplayer.md](Documentation/multiplayer.md).

## Screenshots

![PipelineColor — fluid-colored pipe rows](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows.jpg)

![PipelineColor — color finishes (matte)](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows-matte.jpg)

![PipelineColor — metallic finishes](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-fluid-rows-metallic.jpg)

![PipelineColor — wall and floor pipe supports](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/gameplay-supports.jpg)

![PipelineColor — Customizer PipelineColor swatches](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/PipelineColor/Screenshots/customizer-pc-swatches.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)

Docs index: [Documentation/README.md](Documentation/README.md)
