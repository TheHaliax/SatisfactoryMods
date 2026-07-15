# Chat commands

PipelineColor registers UtilityMod-style `!` commands in regular chat. They run on the **server or listen host** only; pure clients see a Hal system message if they try.

Commands are swallowed from public chat (other mods’ `!` commands are unaffected unless they share the same verb).

The same commands register with **SML** (`AChatCommandSubsystem::RegisterCommand`) so **Chat Mk 2** expandable help and `/help` can list them.

## Available commands

`[]` — required argument.

- `!Metallic [fluid]` — **toggle** metallic finish for that fluid (writes one `MetallicOverrides` entry in `Configs/PipelineColor.cfg`)
- `!Metallic all on` — force **every** catalog fluid (incl. Neutral / Fallback) metallic **on** via per-fluid overrides
- `!Metallic all off` — force **every** catalog fluid metallic **off** (color finish) the same way
- `!Metallic default` — clear all metallic overrides, restore gas-on / liquid-off defaults, and reseed Customizer store colors from catalog
- `!pchelp` — list Pipeline Color chat commands

`all on` / `all off` do **not** flip `DefaultGasMetallic` / `DefaultLiquidMetallic`. They stamp explicit overrides for the full fluid roster. Use `!Metallic default` to drop those overrides and return to gas/liquid defaults.

Fluid tokens accept catalog keys or Customizer-style labels (e.g. `Water`, `NitrogenGas`, `PC Nitrogen Gas`). Matching ignores spaces and case. Toggle flips the **effective** state (override or gas/liquid default). Synonyms for the second `all` token: `on`/`1`/`true`, `off`/`0`/`false`.

## Examples

```text
!Metallic Water
!Metallic NitrogenGas
!Metallic all on
!Metallic all off
!Metallic default
!pchelp
```

## Responses

Feedback uses the **Hal:** sender, for example:

- `Water metallic on.`
- `Nitrogen Gas metallic off.`
- `All fluids metallic on.`
- `All fluids metallic off (color).`
- `Pipeline Color defaults restored (colors + metallic).`
- Unknown fluid → short error; try `!pchelp`

## Related settings

| Surface | Use |
|---------|-----|
| `Configs/PipelineColor.cfg` | JSON on server/host |
| Console `PipelineColor.Set` | Same keys — see [console-commands.md](console-commands.md) |

Customizer Primary/Secondary colors for PC swatches live in the **world save**, not `.cfg`.
