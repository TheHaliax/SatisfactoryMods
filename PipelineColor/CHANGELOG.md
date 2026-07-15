# PipelineColor — CHANGELOG

## 1.0.0

- First public release for Satisfactory 1.2 / SML 3.12
- Fluid-driven colors on pipelines, junctions, and inline pumps (Neutral when empty)
- Customizer **PipelineColor** swatch category (IDs 200–216) + SaveGame swatch store
- Pipe supports matching nearby pipe fluid color (floor / stackable / wall / wall-hole parents)
- Metallic finish at apply time (gases metallic by default; liquids off; overrides via chat / cfg / CVars)
- Chat bangs `!Metallic` (toggle) / `!pchelp` + Chat Mk 2 SML help; config via `Configs/PipelineColor.cfg` and `PipelineColor.Set` (no SML Mods menu)

## 0.1.0

- Scaffold SML GameInstance mod
- Wiki liquid Primary colors + gas paint finishes
- C++ `UPCSwatchDesc_*` defaults (IDs 200–216)
- Authority `SetCustomizationData_Native` apply; fluid hooks + WorldReady scan + empty tick
- Pack: `tools/pack-pipelinecolor.ps1`
