# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

Chat equivalents: see [chat-commands.md](chat-commands.md) (`!Metallic <fluid>`, `all on`, `all off`, `default`, `!pc <fluid> liquid|gas`, `!pc default`, `!pchelp`).

`!Metallic default` resets metallic cfg only; `!pc <fluid> liquid|gas` picks color field only; `!pc default` clears custom SaveGame swatch edits (catalog defaults).

## PipelineColor.Set

```text
PipelineColor.Set <key> <value>
```

Updates a config key and persists to `Configs/PipelineColor.cfg`. Authority only.

| Key | Values | Notes |
|-----|--------|-------|
| `DefaultGasMetallic` | `0` / `1` (also `true` / `false` / `on`) | Gases metallic by default |
| `DefaultLiquidMetallic` | `0` / `1` | Liquids metallic by default |
| `Metallic.<CatalogKey>` | `0` / `1` | Per-fluid override (e.g. `Metallic.FactoryGame_Water`) |
| `ColorSource.<CatalogKey>` | `liquid` / `gas` (also `fluid`) | Which descriptor RGB field; does not change metallic |

## CVars

These mirror the same defaults (loaded from / written to cfg):

- `PipelineColor.DefaultGasMetallic` (default `1`)
- `PipelineColor.DefaultLiquidMetallic` (default `0`)

Prefer `PipelineColor.Set` when you want disk persistence in the same shape as chat.

## Config file path

```text
<Satisfactory>/Configs/PipelineColor.cfg
```

Example (`CfgSchema` 2 — prefixed keys):

```json
{
  "CfgSchema": 2,
  "DefaultGasMetallic": true,
  "DefaultLiquidMetallic": false,
  "MetallicOverrides": {
    "FactoryGame_Water": true
  },
  "ColorSourceOverrides": {
    "FactoryGame_NitrogenGas": "gas",
    "FactoryGame_Water": "gas"
  }
}
```

Loading a cfg without `CfgSchema` ≥ 2 **full-reseeds** (clears old short-key overrides, writes defaults including `FactoryGame_NitrogenGas` → gas).

## Related

- Player behavior: [player-guide.md](player-guide.md)
- Multiplayer authority: [multiplayer.md](multiplayer.md)
