# PipelineColor — CHANGELOG

## 1.2.0 — TBD

- Add Customizer sections for modded fluids: **SatisfactoryPlus** and **RefinedPower**
  subcategories under the PipelineColor category, with swatches and recipes for 46
  Satisfactory Plus and 16 Refined Power fluids/gases (gases metallic by default)
- Soft-gated on mod presence: each modded fluid probes its descriptor class at world
  publish; absent mods contribute no swatches, recipes, subcategories, or SaveGame
  store entries, and their catalog misses log at Verbose instead of Warning
- Register PC customization recipes with the SML ModContentRegistry so Satisfactory
  Plus's recipe clean-up pass no longer strips PC swatch recipes
- Seed default colors from the fluid descriptors themselves: liquids use authored
  `mFluidColor`, gases use `mGasColor` (fluid-color fallback when a gas ships without
  one), and the metallic default derives from the descriptor's `RF_GAS` form — mod
  updates re-tint automatically; the built-in roster table remains the fallback for
  absent classes. Catalog logs a per-fluid drift line when descriptor data disagrees
  with the roster constants
- `!Metallic default` now resets **metallic flags only** (clears overrides, restores
  gas-on / liquid-off defaults) — Customizer swatch edits survive
- New `!pc default` chat command reseeds all Customizer swatch store colors from the
  fluid descriptors (explicit, destructive reset of PC swatch edits)

## 1.1.2 — 2026-07-22

- Fix severe dedicated-server performance drop on large saves (server tickrate falling
  below 30 FPS with CPU spikes and client rubberbanding). Painting a pipe no longer
  iterates the whole world looking for pipe supports: support links resolve once in a
  single seed pass at world load, pipes without supports are remembered instead of
  rescanned, and support parent classes resolve through a GC-safe cache instead of
  four soft-path lookups per world actor
- Cut load-time paint cost per pipe roughly 400x: swatch, paint-finish, and fluid-stem
  lookups now use GC-safe class caches instead of re-parsing soft class paths on every
  apply, and the no-op compare path no longer heap-copies customization data
- Background repaint safety net runs at much lower duty (8 pipes / 5 s instead of
  32 / 0.5 s) and settled pipes skip meter and support work entirely; fluid change,
  drain, placement, and dismantle still repaint immediately through event hooks
- Placing a support against an already-painted pipe repaints immediately through the
  invalidate path

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
