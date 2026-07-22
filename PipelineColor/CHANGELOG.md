# PipelineColor — CHANGELOG

## 1.1.1 — 2026-07-22

- Fix Linux dedicated server SIGILL when a Space Elevator is placed (or its Project
  Assembly logic replicates): the `GetSlotDataForSwatchDesc` hook is no longer installed
  on dedicated servers. The hook only serves the client paint UI; on Linux the hooking
  library builds a broken trampoline for this function's prologue, so any server-side
  Blueprint call crashed the server.
- Fix Linux dedicated crash at map load in swatch store seed (`SeedMissingFromCatalog` →
  catalog `Resolve` / `TMap::Find` hashing a stale fluid `UClass*`): catalog re-keyed by
  fluid stem (`FName`) and soft path string; seeding goes through `ResolveByKey` only;
  `Resolve` rejects invalid `UClass*` before map lookup
- Hardening: never mutate PaintFinish CDOs at apply time; metallic roughness now comes
  from a ClassGen finish flyweight pool + `UPCFinish_MatteNeutral`. Pool classes are also
  generated on clients so replicated paints resolve to the correct finish remotely
- Single `PublishForWorld` per world (PostLoadMap path only; drop duplicate from `OnWorldReady`);
  clients never seed/CDO-inject; Neutral matte reseed only when the entry is absent
- SCIM-safe SaveGame: store paint finish as `PaintFinishPath` (`FString`) instead of `SoftClassPath` (AnthorNet SaveParser has no SoftClass)
- Old saves: Key/Primary/Secondary still load; empty finish paths filled from catalog on `PostLoadGame` (one load+save → SCIM-clean)

## 1.1.0

- Fix inline flow meters on regular MK1/MK2 pipelines (`Build_Pipeline`, `Build_PipelineMK2`) missing fluid color on placement and after mid-run splits (junction/valve → new meter child)
- Repaint flow indicator when it spawns after the parent pipe’s first apply pass (`InvalidateApplied` + deferred `BeginPlay` enqueue)
- `ProcessNow` always syncs the child flow meter even when the parent appearance spec is unchanged

## 1.0.0

- First public release for Satisfactory 1.2 / SML 3.12
- Fluid-driven colors on pipelines, junctions, and inline pumps (Neutral when empty)
- Customizer **PipelineColor** category (Parking stencil icon) + fluid swatches + SaveGame color store
- Matching pipe supports pick up the touched pipe’s look (floor / stackable / wall / wall-hole)
- Metallic at apply: gases on by default, liquids off; colored fluids keep pigment; greys map bright→chrome / dark→burnished
- Neutral roughness: metallic `1.0`, matte/color `4.0`
- Chat: `!Metallic <fluid>`, `!Metallic all on`, `!Metallic all off`, `!Metallic default`, `!pchelp` (Chat Mk 2 help)
- Config: `Configs/PipelineColor.cfg` and `PipelineColor.Set` (no SML Mods menu)
