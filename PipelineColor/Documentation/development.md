# Development

Contributor notes for PipelineColor sources in this repository.

## Repository layout

```text
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

```text
<StarterProject>/Mods/PipelineColor
  → <this-repo>/PipelineColor
```

## Dev build

```powershell
powershell -File tools/pack-pipelinecolor.ps1 -Config Shipping
```

Deploy cooked output to:

```text
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
| Fluid roster | `Appearance/FPCFluidRoster.*` |
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
