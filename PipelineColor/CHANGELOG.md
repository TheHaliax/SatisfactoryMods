# PipelineColor — CHANGELOG

## 1.0.0

- First public release for Satisfactory 1.2 / SML 3.12
- Fluid-driven colors on pipelines, junctions, and inline pumps (Neutral when empty)
- Customizer **PipelineColor** category (Parking stencil icon) + fluid swatches + SaveGame color store
- Matching pipe supports pick up the touched pipe’s look (floor / stackable / wall / wall-hole)
- Metallic at apply: gases on by default, liquids off; colored fluids keep pigment; greys map bright→chrome / dark→burnished
- Neutral roughness: metallic `1.0`, matte/color `4.0`
- Chat: `!Metallic <fluid>`, `!Metallic all on`, `!Metallic all off`, `!Metallic default`, `!pchelp` (Chat Mk 2 help)
- Config: `Configs/PipelineColor.cfg` and `PipelineColor.Set` (no SML Mods menu)
