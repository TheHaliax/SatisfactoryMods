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
  tools/
    build-mod.ps1          ← clang → version → icons → build → deploy
    check-version.ps1
    icons.ps1
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
powershell -File tools/build-mod.ps1 -Mod PipelineColor
```

Pipeline: clang (optional skip) → version guard → icons (default on; `-NoIcons`) → Quick Shipping build → deploy to Steam `FactoryGame/Mods/PipelineColor` (unless `-NoCopy`).

Release:

```powershell
powershell -File tools/build-mod.ps1 -Mod PipelineColor -Mode Release
```

## Key source areas

| Area | Path under `Source/PipelineColor/` |
|------|--------------------------------------|
| Lifecycle / hooks | `PipelineColorRootInstanceModule.cpp` |
| Mod config / CVars / color source | `Config/FPCPipelineColorModConfig.*` |
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
powershell -File tools/check-version.ps1 -Mod PipelineColor
```

`SemVersion`, `Version` major, `RemoteVersionRange` major, and top `CHANGELOG.md` heading must match.

## Related

- [Release (SMR)](release.md)
- [Player guide](player-guide.md)
- [CHANGELOG](../CHANGELOG.md)
