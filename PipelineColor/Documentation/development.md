# Development

Contributor notes for PipelineColor sources in this repository.

## Code comments vs documentation

| Surface | Role |
|---------|------|
| **`Source/`** | Prefer clear code; comments only for engine traps (apply/replication, load order). Teaching prose belongs in `Documentation/`. |
| **`Documentation/`** | Player + contributor what / why / how |

No design-resolution ids or milestone notes in `Source/`. SPDX headers stay.

## Repository layout

```
SatisfactoryMods/
  PipelineColor/
    PipelineColor.uplugin
    Source/PipelineColor/
    Config/
    Resources/
    Screenshots/
    Documentation/
  tools/                          ← pack-pipelinecolor.ps1, Invoke-ModIcons.ps1
```

## Link into SML StarterProject

Junction or copy `PipelineColor/` into StarterProject `Mods/PipelineColor`, then build with Alpakit / UBT.

Typical junction:

```
E:\Modding\Satisfactory\StarterProject\Mods\PipelineColor
  → C:\Users\Haliax\Documents\GitHub\SatisfactoryMods\PipelineColor
```

## Dev build

```powershell
powershell -File tools/pack-pipelinecolor.ps1 -Config Shipping
```

Deploy cooked output to:

```
<Satisfactory>/FactoryGame/Mods/PipelineColor
```

Icons regenerate via `tools/Invoke-ModIcons.ps1 -ModRoot PipelineColor` (also from pack scripts when wired).

## Key source areas

| Area | Path under `Source/PipelineColor/` |
|------|--------------------------------------|
| Lifecycle / hooks | `PipelineColorRootInstanceModule.cpp` |
| Mod config / CVars | `Config/FPCPipelineColorModConfig.*` |
| Chat bang + SML help | `Command/FPCBangCommands.cpp`, `PipelineColorSmlChatCommands.cpp` |
| World session / scan | `Session/UPCWorldSubsystem.*` |
| Apply | `Application/FCustomizationApplicator.cpp` |
| Metallic post | `Appearance/FPCMetallicColorCorrection.h`, `FPCMetallicFlag.h` |
| Catalog / specs | `Appearance/FPCFluidAppearanceCatalog.*`, `FPCAppearanceSpec.h` |
| Swatch descs / publish | `Swatches/*` |
| SaveGame store | `Store/APCSwatchStoreSubsystem.*`, `FPCSwatchSlotDispatch.*` |
| Chat RCO | `Network/UPCChatRCO.*` |
| Support touch | `Content/FPipeSupportTouch.*` |
| Fluid key resolve | `Content/FPipeFluidKeyResolver.*` |

## Version guard

```powershell
powershell -File scripts/check-version.ps1
```

`SemVersion`, `Version` major, `RemoteVersionRange` major, and top `CHANGELOG.md` heading must match.

## Related

- [Release (SMR)](release.md)
- [Player guide](player-guide.md)
- [CHANGELOG](../CHANGELOG.md)
