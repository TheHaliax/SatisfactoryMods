# Topograph tools README

| Tool | Path | Role |
|------|------|------|
| Pack | `tools/pack-topograph.ps1` | UBT Shipping → game Mods |
| Codec | `tools/topograph-codec` | `.raw` → surface/volume/PNG |
| Python | `tools/topograph-python` | Blender OBJ + layer stub |

## WP spike notes (dedi)

On Linux test dedi with only Topograph (+ SML):

1. Strip other mods; fresh/empty session.
2. Confirm log: `Topograph v0.1 root module initialized` then `Auto-start bake`.
3. Watch `Streaming anchor → tile` and `IsStreamingCompleted` / soft timeout.
4. Expect `Saved/Topograph/rocky_desert/tile_*.raw` + `resources.json`.

Cell isolation: one `UWorldPartitionStreamingSourceComponent` with custom radius shape; neighbor halo expected. Gate via component `IsStreamingCompleted` and optional `UFGWorldPartitionRuntimeSpatialHash::IsCellContainingWorldLocationLoaded`.
