# Chat commands

Structural Power registers **UtilityMod-style** `!` commands in regular chat. They run on the **server or listen host** only; pure clients see a Hal system message if they try.

Commands are swallowed from public chat (other mods' `!` commands are unaffected unless they use the same verb).

On join, the server prints: **Hal: Structural Power loaded.**

## Available commands

`[]` — required argument.

- `!lighting` — toggle **structural lighting** (v2.2 M3; default off; saved to mod config)
- `!HoverH [1-10]` — set hoverpack **horizontal** reach multiplier (clamped 1.0–10.0; saved to `Configs/StructuralPower.cfg`)
- `!HoverV [1-10]` — set hoverpack **vertical** reach multiplier (clamped 1.0–10.0; saved to mod config)
- `!tracetoggle` — debug: toggle verbose `[HALSP]` trace logging in `FactoryGame.log`
- `!pwrhelp` — list commands (same list as this section)

## Examples

```
!lighting
!HoverH 2
!HoverV 1.5
!tracetoggle
!pwrhelp
```

## Responses

Feedback uses the **Hal:** sender with short readable lines, for example:

- `Structural lighting enabled.`
- `Hoverpack horizontal reach multiplier set to 2.0.`
- `PWR trace logging enabled.`

## Related settings

| Surface | Use |
|---------|-----|
| Pause → Mods → Structural Power | Hover multipliers (main panel); propagation, switches, lighting, hoverpack tether, trace ( **Debug** section, collapsed by default) |
| `Configs/StructuralPower.cfg` | SML-persisted JSON on server |
| Console `StructuralPower.Set <key> <value>` | Same keys as mod config — see [console-commands.md](console-commands.md) |

Per-device **Source/Control** ids for lights, switches, and panels stay in the **world save** (not `.cfg`) — assigned via **I** key Id panel or building tag on switches.
