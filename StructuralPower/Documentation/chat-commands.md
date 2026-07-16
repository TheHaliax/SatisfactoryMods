# Chat commands

Structural Power registers **UtilityMod-style** `!` commands in regular chat. They run on the **server or listen host** only; pure clients see a Hal system message if they try.

Commands are swallowed from public chat (other mods' `!` commands are unaffected unless they use the same verb).

On join, the server prints: **Hal: Structural Power loaded.**

The same commands are also registered with **SML** (`AChatCommandSubsystem::RegisterCommand`) so **Chat Mk 2** expandable help and `/help` can list them alongside other mods.

## Available commands

`[]` — required argument. All group toggles default **off** and persist to `Configs/StructuralPower.cfg`.

- `!lighting` — toggle **structural lighting**
- `!Generation` — toggle **generators + power storage** on structure
- `!Resources` — toggle **miners / extractors**
- `!Production` — toggle **manufacturers / radar / sink**
- `!Transport` — toggle **wired transport** consumers (stub — no track topology)
- `!Pipes` — toggle **pipeline pumps** (stub — no pipe-run topology)
- `!Belts` — toggle **belts** group (no attach yet)
- `!HoverH [1-10]` — set hoverpack **horizontal** reach multiplier (clamped 1.0–10.0)
- `!HoverV [1-10]` — set hoverpack **vertical** reach multiplier (clamped 1.0–10.0)
- `!pwrhelp` — list commands (same list as this section)

Trace and extended debug are **console-only** (`StructuralPower.Trace`, `StructuralPower.ExtendedDebug`) — not exposed in chat.

## Examples

```
!Generation
!Resources
!lighting
!HoverH 2
!HoverV 1.2
!pwrhelp
```

## Responses

Feedback uses the **Hal:** sender with short readable lines, for example:

- `Structural lighting enabled.`
- `Structural generation enabled.`
- `Hoverpack horizontal reach multiplier set to 2.0.`

## Related settings

| Surface | Use |
|---------|-----|
| `Configs/StructuralPower.cfg` | JSON on server/host — written by chat or console |
| Console `StructuralPower.Set <key> <value>` | Same keys — see [console-commands.md](console-commands.md) |

Per-device **Source/Control** ids for lights, switches, and panels stay in the **world save** (not `.cfg`) — assigned via **I** key Id panel or building tag on switches.
