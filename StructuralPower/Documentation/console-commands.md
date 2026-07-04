# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

## StructuralPower.Trace

```
StructuralPower.Trace 1
```

Enables deep `[PWR]` logging in `FactoryGame.log` — placement hooks, hidden links, circuit promotion.

**Default: `0` (off)** in shipping builds. Turn on when reporting bugs.

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

## StructuralPower.Diag

```
StructuralPower.Diag
```

Logs graph stats (structure nodes, components, largest component, tracked poles, tracked lightweights, pending jobs) followed by a bridge-pole audit — total poles and how many have an outlet bus (`WITH_BUS`), are promoted to a circuit (`IN_CIRCUIT`), or have no bus yet (`NO_BUS`).

Use after loading a save to verify tracked poles. Manual diag works on any map; automatic post-load diag skips menu worlds.

## Log location

Windows:

```
%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log
```

Search for `LogStructuralPower` or `[PWR]`.
