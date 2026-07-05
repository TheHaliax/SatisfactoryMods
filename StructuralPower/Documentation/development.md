# Development

## Repository layout

```
SatisfactoryMods/
  StructuralPower/          ← this mod (ModReference name)
    StructuralPower.uplugin
    Source/StructuralPower/
    Config/
    Resources/
    Screenshots/
    Documentation/
```

## Link into SML StarterProject

Junction or copy `StructuralPower/` into your SML StarterProject `Mods/` folder (same name as the plugin), then build with Alpakit or UBT.

## Dev build (fast iteration)

Use your local pack script or Alpakit **Development** / Win64 Shipping against StarterProject.

Deploy cooked output to:

```
<Satisfactory>/FactoryGame/Mods/StructuralPower
```

## Release (SMR)

Cook all required targets (client + Windows Server + Linux Server) with **Alpakit Release**. Upload the combined zip to SMR — players and servers then use **`.smod`** downloads from the API, not the per-target zips Alpakit also writes.

```
StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip   ← upload this
StructuralPower-LinuxServer.zip   ← local debug only
StructuralPower-WindowsServer.zip   ← local debug only
```

Full checklist: [release.md](release.md).

## Key source areas

| Area | Path |
|------|------|
| Hooks & enqueue | `StructuralPowerRootInstanceModule.cpp` |
| Mod config + SML menu | `UStructuralPowerModConfiguration.cpp`, `FStructuralPowerModConfig.cpp` |
| Chat `!` commands | `FStructuralPowerBangCommands.cpp` |
| Hoverpack bridge | `FStructuralHoverpackBridge.cpp` |
| Switch listener / RCO | `UStructuralPowerSwitchListener.cpp`, `UStructuralPowerRCO.cpp` |
| Poles, circuit & subsystem | `AStructuralPowerGraphSubsystem.cpp` |
| Structural connectivity graph (spatial hash + union-find) | `FStructuralConnectivityGraph.cpp` |
| Lightweight index | `FStructuralLightweightIndex.cpp` |
| Eligibility rules | `FStructuralEligibilityRules.cpp` |
| Factory tick drain | `UStructuralPowerFactoryTickHandler.cpp` |
| Diagnostics | `FStructuralPowerDiagnostics.cpp` |

## License

GPL-3.0-or-later. SPDX headers in source files.
