# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

Chat equivalents: see [chat-commands.md](chat-commands.md) (`!Metallic <fluid>`, `all on`, `all off`, `default`, `!pchelp`).

`!Metallic default` also reseeds SaveGame swatch colors from the catalog (not only cfg).

## PipelineColor.Set

```text
PipelineColor.Set <key> <value>
```

Updates a config key and persists to `Configs/PipelineColor.cfg`. Authority only.

| Key | Values | Notes |
|-----|--------|-------|
| `DefaultGasMetallic` | `0` / `1` (also `true` / `false` / `on`) | Gases metallic by default |
| `DefaultLiquidMetallic` | `0` / `1` | Liquids metallic by default |
| `Metallic.<CatalogKey>` | `0` / `1` | Per-fluid override (e.g. `Metallic.Water`) |

## CVars

These mirror the same defaults (loaded from / written to cfg):

- `PipelineColor.DefaultGasMetallic` (default `1`)
- `PipelineColor.DefaultLiquidMetallic` (default `0`)

Prefer `PipelineColor.Set` when you want disk persistence in the same shape as chat.

## Config file path

```text
<Satisfactory>/Configs/PipelineColor.cfg
```

Example:

```json
{
  "DefaultGasMetallic": true,
  "DefaultLiquidMetallic": false,
  "MetallicOverrides": {
    "Water": true
  }
}
```

## Related

- Player behavior: [player-guide.md](player-guide.md)
- Multiplayer authority: [multiplayer.md](multiplayer.md)
