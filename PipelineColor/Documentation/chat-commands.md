# Chat commands

PipelineColor registers UtilityMod-style `!` commands in regular chat. They run on the **server or listen host** only; pure clients see a Hal system message if they try.

Commands are swallowed from public chat (other mods’ `!` commands are unaffected unless they share the same verb).

The same commands register with **SML** (`AChatCommandSubsystem::RegisterCommand`) so **Chat Mk 2** expandable help and `/help` can list them.

## Available commands

`[]` — required argument.

- `!Metallic [fluid]` — **toggle** metallic finish for that fluid (saved to `Configs/PipelineColor.cfg`)
- `!Metallic all on|off` — set **all** fluids metallic or color
- `!Metallic default` — reset swatch colors + metallic overrides/defaults to mod defaults
- `!pchelp` — list Pipeline Color chat commands

Fluid tokens accept catalog keys or Customizer-style labels (e.g. `Water`, `NitrogenGas`, `PC Nitrogen Gas`). Matching ignores spaces and case. Toggle flips the **effective** state (override or gas/liquid default).

## Examples

```text
!Metallic Water
!Metallic NitrogenGas
!pchelp
```

## Responses

Feedback uses the **Hal:** sender, for example:

- `Water metallic on.`
- `Nitrogen Gas metallic off.`
- Unknown fluid → short error; try `!pchelp`

## Related settings

| Surface | Use |
|---------|-----|
| `Configs/PipelineColor.cfg` | JSON on server/host |
| Console `PipelineColor.Set` | Same keys — see [console-commands.md](console-commands.md) |

Customizer Primary/Secondary colors for PC swatches live in the **world save**, not `.cfg`.
