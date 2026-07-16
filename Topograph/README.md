# Topograph

Headless world topography bake for Satisfactory dedicated servers.

Produces surface DEM (`uint64` samples), sparse 4 m volume bands, resource registry, and (via Rust/Python) topo PNGs + Blender meshes. Intended for rail/truck megaprint planning and a future Factorio-like layered map UI.

| | |
|---|---|
| **Version** | 0.1.0 |
| **Game** | Satisfactory 1.2 (≥491125) |
| **SML** | ^3.12.0 |
| **Host** | Dedicated server (Linux test / Windows) |

## Features (0.1)

- Auto-start on dedicated server (`bAutoStartOnDedicatedServer`)
- Rocky Desert ROI default (wiki grids X0Y3–X1Y4); optional full-world AABB
- World Partition streaming anchor (one macro-tile + halo), unload after tile
- Probe global Z0, then 12.5 UU surface raster + 256×4 m sparse bands
- Resource node registry (type, purity, foundation cell) + pixel parent bits
- Console: `Topograph.Start` / `Topograph.Stop` / `Topograph.Status`

## Build / deploy

```powershell
powershell -File tools/pack-topograph.ps1
```

Linux dedi: pack LinuxServer target separately when ready; copy Mods/Topograph to laptop `sat-server-test`.

## Encode tiles (Rust)

```bash
cd tools/topograph-codec
cargo run --release -- --input-dir <Saved/Topograph/rocky_desert> --output-dir <out>
```

## Blender / layer stub (Python)

```bash
python tools/topograph-python/export_blender.py --tile-dir <out/tile_0000_0000> --out mesh
python tools/topograph-python/layer_viewer_stub.py --tile-dir <out/tile_0000_0000>
```

## Output

Under `Saved/Topograph/rocky_desert/` (configurable):

- `tile_XXXX_YYYY.raw` — UE POD handoff
- `resources.json` — node registry
- After codec: `surface.bin.zst`, `volume.bin.zst`, `topo_*.png`, `meta.json`
