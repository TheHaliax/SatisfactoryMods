# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

Chat equivalents: see [chat-commands.md](chat-commands.md) (`!lighting`, `!HoverH`, `!HoverV`).

## StructuralPower.Set

```
StructuralPower.Set <key> <value>
```

Updates a config key and persists to `Configs/StructuralPower.cfg`. Authority only.

| Key | Values | Notes |
|-----|--------|-------|
| `GroupLighting` | `0` / `1` | Structural lighting (v2.2; chat: `!lighting`) |
| `HoverpackStructuralHorizontalMultiplier` | `1.0`–`10.0` (default `1.2`) | Chat: `!HoverH` |
| `HoverpackStructuralVerticalMultiplier` | `1.0`–`10.0` (default `1.2`) | Chat: `!HoverV` |
| `HoverpackStructuralRadiusMultiplier` | `1.0`–`10.0` (default `1.2`) | Legacy alias — sets **both** H and V |
| `Trace` | `0` / `1` | `[HALSP]` logging (debug; console only) |
| `ExtendedDebug` | `0` / `1` | Vanilla power/circuit hooks + fat logs (debug; console only) |

## StructuralPower.Trace

```
StructuralPower.Trace 1
```

Enables deep `[HALSP]` logging in `FactoryGame.log` — placement hooks, hidden links, circuit promotion.

**Default: `0` (off)** in shipping builds. Turn on when reporting bugs.

To disable:

```
StructuralPower.Trace 0
```

## StructuralPower.ExtendedDebug

```
StructuralPower.ExtendedDebug 1
```

Extra vanilla power/circuit instrumentation. **Default: `0` (off).**

## StructuralPower.GroupLighting

```
StructuralPower.GroupLighting 1
```

Enables structural lighting (same as `!lighting` toggle). **Default: `0` (off).**

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

JSON; written when `StructuralPower.Set` or chat commands change values.

## Log location

Windows:

```
%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log
```

Search for `LogStructuralPower` or `[HALSP]`.
