# Console Commands

Open the in-game console (`~`) on the **server or listen host** (not pure clients).

## StructuralPower.Trace

```
StructuralPower.Trace 1
```

Enables deep `[PWR]` logging in `FactoryGame.log` — placement hooks, hidden links, circuit promotion.

**Default: `0` (off)** for v1.0.0 shipping builds. Turn on when reporting bugs.

To disable:

```
StructuralPower.Trace 0
```

## StructuralPower.EnablePropagation

```
StructuralPower.EnablePropagation 0
```

Disables injection on **new** placements without unloading the mod. Existing hidden links remain until removed in-game.

Re-enable:

```
StructuralPower.EnablePropagation 1
```

## StructuralPower.Diag

```
StructuralPower.Diag
```

Runs a bridge-pole audit: OK / NO_PARENT / NO_COMPONENT / NO_CIRCUIT / LEGACY_UNTRACKED counts.

Use after loading a save to verify tracked poles. Manual diag works on any map; automatic post-load diag skips menu worlds.

## Log location

Windows:

```
%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log
```

Search for `LogStructuralPower` or `[PWR]`.
