# Troubleshooting

## Pipes stay default / unpainted

1. Confirm PipelineColor loads — look for `PipelineColor v1.0.0` and `[HALPC]` in `FactoryGame.log`.
2. Fill the network with a known fluid (Water, Fuel, …) and wait a short settle.
3. Empty networks use Neutral — that can look like “no paint” compared to bright fluids.
4. Grep `FactoryGame.log` for `[HALPC]` / `LogPipelineColor`.

## Supports do not match pipe color

Supports must be a **fluid support parent** the mod soft-`IsA`s (floor / stackable / wall / wall-hole). Bare hyper/pole parents that are not fluid supports are skipped on purpose. Place or nudge the support against a painted pipe so touch detection can run after pipe ProcessNow.

## Metallic not applying

1. Check defaults: gases on, liquids off unless `DefaultLiquidMetallic` or a per-key override.
2. Turn one fluid on: `!Metallic <fluid>` (toggle) or `PipelineColor.Set Metallic.<Key> 1`. Force everything metallic: `!Metallic all on`. Force everything color: `!Metallic all off`.
3. Confirm cfg wrote under `Configs/PipelineColor.cfg` (`MetallicOverrides` for `all on` / `all off` / toggles).
4. Metallic is **PaintFinish at apply**, not Secondary RGB white in Customizer.
5. Reset mess: `!Metallic default` reseeds store colors and clears overrides (back to gas-on / liquid-off defaults).

## Customizer swatch edits do not stick

Save the session after editing. Store is authority SaveGame; pure clients need the host to save. Remote players must have the mod installed.

## Chat commands ignored

Authority only. Pure clients get a Hal message. Try `!pchelp` on the host. Verbs: `!Metallic <fluid>`, `!Metallic all on`, `!Metallic all off`, `!Metallic default`.

## Reporting issues

Include:

- Game/SML/mod versions
- Client vs dedicated vs listen
- Relevant `FactoryGame.log` slice around load and a paint change
- Whether pipes, supports, or Customizer failed
