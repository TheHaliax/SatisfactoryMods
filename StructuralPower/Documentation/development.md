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
  Link-Mods.ps1             ← junction into StarterProject Mods/
```

## Link into SML StarterProject

From repo root:

```powershell
.\Link-Mods.ps1
```

Creates junctions under `StarterProject\Mods\StructuralPower`.

## Dev build (fast iteration)

Use your local pack script or Alpakit **Development** / Win64 Shipping against StarterProject.

Deploy cooked output to:

```
<Satisfactory>/FactoryGame/Mods/StructuralPower
```

## Release zip (SMR)

Cook all required targets (client + Windows Server + Linux Server). Output typically:

```
StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip
```

Upload that zip to [ficsit.app](https://ficsit.app).

## Key source areas

| Area | Path |
|------|------|
| Hooks & enqueue | `StructuralPowerRootInstanceModule.cpp` |
| Graph & save | `AStructuralPowerGraphSubsystem.cpp` |
| Lightweight index | `FStructuralLightweightIndex.cpp` |
| Eligibility rules | `FStructuralEligibilityRules.cpp` |
| Factory tick drain | `UStructuralPowerFactoryTickHandler.cpp` |
| Diagnostics | `FStructuralPowerDiagnostics.cpp` |

## License

GPL-3.0-or-later. SPDX headers in source files.
