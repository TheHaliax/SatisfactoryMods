# Troubleshooting

## Pipes stay default / unpainted

1. Confirm PipelineColor loads — look for `PipelineColor v1.2.1` and `[HALPC]` in `FactoryGame.log`.
2. Fill the network with a known fluid (Water, Fuel, …) and wait a short settle.
3. Empty networks use **PC Empty Pipe** — that can look like “no paint” compared to bright fluids.
4. Grep `FactoryGame.log` for `[HALPC]` / `LogPipelineColor`.

## Pipes are bright magenta

**PC ERR0R** / unknown fluid path. Confirm the fluid is a catalogued pipe fluid. Grep `[HALPC]` for catalog miss / fallback lines. `!pc default` reseeds swatch store colors from descriptors after cfg/color-source changes.

## Supports do not match pipe color

Supports must be a **fluid support parent** the mod soft-`IsA`s (floor / stackable / wall / wall-hole). Bare hyper/pole parents that are not fluid supports are skipped on purpose. Place or nudge the support against a painted pipe so touch detection can run after pipe ProcessNow.

## Wrong RGB (liquid vs gas look)

1. Check `ColorSourceOverrides` in `Configs/PipelineColor.cfg` or run `!pc <fluid> liquid|gas`.
2. Default without override: liquid for all keys except Nitrogen Gas → gas.
3. Color source does **not** flip metallic — use `!Metallic` for finish.

## Metallic not applying

1. Check defaults: gases on, liquids off unless `DefaultLiquidMetallic` or a per-key override.
2. Turn one fluid on: `!Metallic <fluid>` (toggle) or `PipelineColor.Set Metallic.<Key> 1`. Force everything metallic: `!Metallic all on`. Force everything color: `!Metallic all off`.
3. Confirm cfg wrote under `Configs/PipelineColor.cfg` (`MetallicOverrides` for `all on` / `all off` / toggles).
4. Metallic is **PaintFinish at apply**, not Secondary RGB white in Customizer.
5. Reset mess: `!Metallic default` clears overrides (back to gas-on / liquid-off defaults) without touching swatch edits; `!pc default` reseeds all swatch colors from fluid data (resets Customizer edits).

## SatisfactoryPlus / RefinedPower sections missing

1. Sections only exist when the matching mod is installed on the **authority** (host or dedicated). Vanilla installs show only the **Default** section — by design.
2. Grep `FactoryGame.log` for `mod soft-available stems=`. `stems=0` means no SFP/RP fluid descriptors resolved; check the mods are actually enabled.
3. `catalog ready (... mod fluids)` shows how many modded fluid classes loaded.
4. PC recipes are registered with the SML ModContentRegistry at publish. If the log shows `ModContentRegistry missing — SFP may scrub`, Satisfactory Plus's clean-up pass may remove PC swatch recipes; report with the log slice.

## Customizer swatch edits do not stick

Save the session after editing. Store is authority SaveGame; pure clients need the host to save. Remote players must have the mod installed. Pipe *runtime* paint still follows fluid auto-color even if you paint a pipe with the gun — use Customizer PC swatches for stored per-fluid colors.

## SCIM / Interactive Map cannot open save

PipelineColor persists Customizer colors on the swatch store actor. Older builds wrote paint finish as SoftClassPath; AnthorNet SCIM does not parse that type. Current builds use a string path (`PaintFinishPath`). Load the save in-game once (colors migrate; finish may fall back to catalog defaults), save again, then open in SCIM.

## Chat commands ignored

Authority only. Pure clients get `Host only.` Try `!pchelp` on the host. Verbs: `!Metallic …`, `!pc <fluid> liquid|gas`, `!pc default`.

## Reporting issues

Include:

- Game/SML/mod versions
- Client vs dedicated vs listen
- Relevant `FactoryGame.log` slice around load and a paint change
- Whether pipes, supports, or Customizer failed
