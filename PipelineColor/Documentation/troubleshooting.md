# Troubleshooting

## Pipes stay default / unpainted

1. Confirm PipelineColor loads — look for `PipelineColor v1.3.0` and `[HALPC]` in `FactoryGame.log`.
2. Fill the network with a known fluid (Water, Fuel, …) and wait a short settle.
3. Empty networks use **PC Empty Pipe** — that can look like “no paint” compared to bright fluids.
4. Grep `FactoryGame.log` for `[HALPC]` / `LogPipelineColor` (`dynamic swatches ready`).

## Pipes are bright magenta

**PC ERR0R** / unknown fluid path. Confirm the fluid is a liquid/gas descriptor the registry can discover. Grep `[HALPC]` for catalog miss / fallback lines. `!pc default` clears custom swatch store entries after cfg/color-source experiments.

## Customizer swatches are solid black

Icons read colors from the SaveGame store **or** catalog defaults. If everything is black after an upgrade, confirm you are on **1.3.0+** (catalog fill on store miss). Unedited fluids should still show descriptor colors in the grid.

## Supports do not match pipe color

Supports must be a **fluid support parent** the mod soft-`IsA`s (floor / stackable / wall / wall-hole). Bare hyper/pole parents that are not fluid supports are skipped on purpose. Place or nudge the support against a painted pipe so touch detection can run after pipe ProcessNow.

## Wrong RGB (liquid vs gas look)

1. Check `ColorSourceOverrides` in `Configs/PipelineColor.cfg` or run `!pc <fluid> liquid|gas`.
2. Default without override: liquid for all keys except `FactoryGame_NitrogenGas` → gas.
3. Color source does **not** flip metallic — use `!Metallic` for finish.
4. Prefixed keys: `PipelineColor.Set ColorSource.FactoryGame_Water gas` (chat still accepts `Water`).

## Metallic not applying

1. Check defaults: gases on, liquids off unless `DefaultLiquidMetallic` or a per-key override.
2. Turn one fluid on: `!Metallic <fluid>` (toggle) or `PipelineColor.Set Metallic.FactoryGame_Water 1`. Force everything metallic: `!Metallic all on`. Force everything color: `!Metallic all off`.
3. Confirm cfg wrote under `Configs/PipelineColor.cfg` (`CfgSchema` 2, `MetallicOverrides`).
4. Metallic is **PaintFinish at apply**, not Secondary RGB white in Customizer.
5. Reset mess: `!Metallic default` clears overrides (back to gas-on / liquid-off defaults) without touching swatch edits; `!pc default` clears all custom swatch edits.

## SatisfactoryPlus / RefinedPower (or other mod) sections missing

1. Sections are created from discovered fluids — subcategory name = mod **FriendlyName**. Authority must have the mod enabled.
2. Grep `FactoryGame.log` for `dynamic swatches by owner:` — expect a count for that mod (e.g. Refined Power). Also check `assets=` in `dynamic swatches ready` (AssetRegistry soft-load).
3. Vanilla-only installs show **Default** (FactoryGame fluids) plus Neutral / Fallback — by design.
4. PC recipes are registered with the SML ModContentRegistry at publish. If the log shows `ModContentRegistry missing — SFP may scrub`, Satisfactory Plus's clean-up pass may remove PC swatch recipes; report with the log slice.

## Customizer swatch edits do not stick

Save the session after editing. Store is authority SaveGame (**only custom** entries). Pure clients need the host to save. Remote players must have the mod installed. Pipe *runtime* paint still follows fluid auto-color even if you paint a pipe with the gun — use Customizer PC swatches for stored per-fluid colors.

Pre-1.3.0 saves nuke the old store once (schema 4). Re-edit colors you care about.

## SCIM / Interactive Map cannot open save

PipelineColor persists Customizer colors on the swatch store actor. Older builds wrote paint finish as SoftClassPath; AnthorNet SCIM does not parse that type. Current builds use a string path (`PaintFinishPath`). Load the save in-game once, save again, then open in SCIM. Schema 4 stores are sparse (customs only).

## Chat commands ignored

Authority only. Pure clients get `Host only.` Try `!pchelp` on the host. Verbs: `!Metallic …`, `!pc <fluid> liquid|gas`, `!pc default`.

## Reporting issues

Include:

- Game/SML/mod versions
- Client vs dedicated vs listen
- Relevant `FactoryGame.log` slice around load and a paint change (`dynamic swatches ready` / `by owner`)
- Whether pipes, supports, or Customizer failed
