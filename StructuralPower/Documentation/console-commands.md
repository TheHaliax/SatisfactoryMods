# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

Chat equivalents: see [chat-commands.md](chat-commands.md) (`!lighting`, `!HoverH`, `!HoverV`, `!tracetoggle`).

## StructuralPower.Set

```
StructuralPower.Set <key> <value>
```

Updates a mod config key, mirrors the pause menu, and marks `Configs/StructuralPower.cfg` dirty. Authority only.

| Key | Values | Notes |
|-----|--------|-------|
| `EnablePropagation` | `0` / `1` | Master bus toggle (Debug section) |
| `Trace` | `0` / `1` | `[PWR]` logging (Debug; chat: `!tracetoggle`) |
| `GatePowerSwitches` | `0` / `1` | Structural switch gating (Debug) |
| `PowerSwitchManualGroups` | `0` / `1` | `1` = Mode B keyed subnets; `0` = Mode A whole-component (Debug) |
| `GroupLighting` | `0` / `1` | Structural lighting (v2.2; Debug; chat: `!lighting`) |
| `EnableHoverpackStructural` | `0` / `1` | Hoverpack tether (Debug) |
| `HoverpackStructuralHorizontalMultiplier` | `1.0`–`10.0` | Chat: `!HoverH` |
| `HoverpackStructuralVerticalMultiplier` | `1.0`–`10.0` | Chat: `!HoverV` |
| `HoverpackStructuralRadiusMultiplier` | `1.0`–`10.0` | Legacy alias — sets **both** H and V |

## StructuralPower.Trace

```
StructuralPower.Trace 1
```

Enables deep `[PWR]` logging in `FactoryGame.log` — placement hooks, hidden links, circuit promotion.

**Default: `0` (off)** in shipping builds. Turn on when reporting bugs. Same as `!tracetoggle` or the **Debug** toggle in the mod menu.

To disable:

```
StructuralPower.Trace 0
```

## StructuralPower.EnablePropagation

```
StructuralPower.EnablePropagation 0
```

Disables structural power propagation without unloading the mod. Existing hidden links remain until removed in-game.

Re-enable:

```
StructuralPower.EnablePropagation 1
```

## StructuralPower.GatePowerSwitches / PowerSwitchManualGroups / GroupLighting / EnableHoverpackStructural

CVars mirror the **Debug** section in the mod menu:

```
StructuralPower.GatePowerSwitches 1
StructuralPower.PowerSwitchManualGroups 1
StructuralPower.GroupLighting 1
StructuralPower.EnableHoverpackStructural 1
StructuralPower.HoverpackStructuralHorizontalMultiplier 1.5
StructuralPower.HoverpackStructuralVerticalMultiplier 1.5
```

## StructuralPower.Diag

```
StructuralPower.Diag
```

Logs graph stats (structure nodes, components, largest component, tracked poles, tracked lightweights, pending jobs) followed by a bridge-pole audit — total poles and how many have an outlet bus (`WITH_BUS`), are promoted to a circuit (`IN_CIRCUIT`), or have no bus yet (`NO_BUS`).

Use after loading a save to verify tracked poles. Manual diag works on any map; automatic post-load diag skips menu worlds.

## Config file

Server/host:

```
<Satisfactory>/Configs/StructuralPower.cfg
```

SML-managed JSON; written when the mod menu or `StructuralPower.Set` / chat commands change values.

## Log location

Windows:

```
%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log
```

Search for `LogStructuralPower` or `[PWR]`.
