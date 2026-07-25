# Chat commands

PipelineColor registers UtilityMod-style `!` commands in regular chat. They run on the **server or listen host** only; pure clients see a Hal system message if they try.

Commands are swallowed from public chat (other mods’ `!` commands are unaffected unless they share the same verb).

The same commands register with **SML** (`AChatCommandSubsystem::RegisterCommand`) so **Chat Mk 2** expandable help and `/help` can list them.

## Available commands

`[]` — required argument.

- `!Metallic [fluid]` — **toggle** metallic finish for that fluid (writes one `MetallicOverrides` entry in `Configs/PipelineColor.cfg`)
- `!Metallic all on` — force **every** catalog fluid (incl. Neutral / Fallback) metallic **on** via per-fluid overrides
- `!Metallic all off` — force **every** catalog fluid metallic **off** (color finish) the same way
- `!Metallic default` — clear all metallic overrides and restore gas-on / liquid-off defaults. **Metallic flags only** — Customizer swatch edits stay untouched
- `!pc [fluid] liquid|gas` — paint RGB from `mFluidColor` or `mGasColor` (**metallic unchanged**). Default without override: liquid for all except `FactoryGame_NitrogenGas` → gas
- `!pc default` — clear **all** custom Customizer swatch store entries. Catalog defaults paint until you edit again (metallic cfg untouched)
- `!pchelp` — short command list

`all on` / `all off` do **not** flip `DefaultGasMetallic` / `DefaultLiquidMetallic`. They stamp explicit overrides for the full fluid roster. Use `!Metallic default` to drop those overrides and return to gas/liquid defaults.

Fluid tokens accept catalog keys or Customizer-style labels (e.g. `Water`, `FactoryGame_Water`, `NitrogenGas`, `PC Nitrogen Gas`). Matching ignores spaces and case. Toggle flips the **effective** state (override or gas/liquid default). Synonyms for the second `all` token: `on`/`1`/`true`, `off`/`0`/`false`. Color-source last token synonyms: `liquid`/`fluid`, `gas`.

## Examples

```text
!Metallic Water
!Metallic all on
!Metallic default
!pc Water gas
!pc Nitrogen Gas liquid
!pc default
!pchelp
```

## Responses

Feedback uses the **Hal:** sender. Examples:

| Situation | Message |
|-----------|---------|
| Metallic toggle | `Water metallic on.` / `Water metallic off.` |
| Metallic all | `All metallic on.` / `All metallic off.` |
| Metallic default | `Metallic defaults restored.` |
| Color source | `Water gas color.` / `Water liquid color.` |
| `!pc default` | `Custom swatches cleared.` |
| Client / no authority | `Host only.` |
| Unknown fluid | `Unknown: …` |
| Bad args / catch-all | usage line, or `Unknown. !pchelp` |
| `!pchelp` | two yellow usage lines (`!Metallic …`, `!pc …`) |

## Related settings

| Surface | Use |
|---------|-----|
| `Configs/PipelineColor.cfg` | `CfgSchema`, `MetallicOverrides`, `ColorSourceOverrides`, defaults |
| Console `PipelineColor.Set` | Same keys as cfg |
| Customizer | PC swatch RGB / finish edits (store) |
