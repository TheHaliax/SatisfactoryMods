# Development

Contributor notes for the StructuralPower sources in this repository.

## Repository layout

```
SatisfactoryMods/
  StructuralPower/
    StructuralPower.uplugin
    Source/StructuralPower/
    Config/
    Resources/
    Screenshots/
    Documentation/
  tools/                          ← monorepo pack scripts (e.g. pack-structuralpower.ps1)
```

## Link into SML StarterProject

Junction or copy `StructuralPower/` into your SML StarterProject `Mods/` folder (same name as the plugin), then build with Alpakit or UBT.

## Dev build (fast iteration)

```powershell
powershell -File tools/pack-structuralpower.ps1 -Config Shipping
```

Deploy cooked output to:

```
<Satisfactory>/FactoryGame/Mods/StructuralPower
```

## Release (SMR)

Cook all required targets (client + Windows Server + Linux Server) with **Alpakit Release**. Upload the combined zip to SMR.

```
StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip   ← upload this
```

Full checklist: [release.md](release.md).

## Key source areas

| Area | Path under `Source/StructuralPower/` |
|------|--------------------------------------|
| Hooks & enqueue | `StructuralPowerRootInstanceModule.cpp` |
| Mod config | `Config/FStructuralPowerModConfig.cpp` |
| Chat `!` + SML help | `Command/FStructuralPowerBangCommands.cpp`, `StructuralPowerSmlChatCommands.cpp` |
| Hoverpack | `Equipment/FStructuralHoverpackBridge.cpp` |
| Switch listener / RCO | `Network/UStructuralPowerSwitchListener.cpp`, `UStructuralPowerRCO.cpp` |
| Subsystem | `Save/AStructuralPowerGraphSubsystem.*` |
| Graph session + lifecycle | `Core/FStructuralGraphSession.*`, `Graph/FStructuralGraph{Bootstrap,BulkDrain,StructureIngress,Removal,CircuitEcho}.*` |
| Endpoint pipeline | `Processors/FStructuralEndpointCatalog.*`, `FStructuralEndpointDispatcher.*` |
| Processors | `Processors/FStructuralPower*Processor.*`, `FStructuralPowerTransferGate.*`, `FStructuralSwitchBridgeStrategy.*` |
| Site membership | `Connection/FStructuralSiteMembership.*` |
| Attach | `Attach/FStructuralEndpointAttach.*`, `FStructuralBridgeAttach.*`, `FStructuralDeviceAttach.*`, `FStructuralPanelAttach.*` |
| Structure graph | `Graph/FStructuralConnectivityGraph.*` |
| Reconcile / restitch | `Reconcile/FStructuralPowerReconcile.*`, `FStructuralPowerRestitch.*` |
| Circuit ops | `Circuit/FStructuralGraphCircuitOps.*` |
| Routing / Ids | `Routing/FStructuralPowerRouter.*`, `Save/FStructuralGraphIdOps.*` |
| Panel / lights | `Panel/FStructuralPanelControlledSync.*`, light/panel processors |
| Id UI / input | `UI/UStructuralPowerIdConfigWidget.*`, `Input/FStructuralPowerIdInput.*` |
| Factory tick | `Subsystems/UStructuralPowerFactoryTickHandler.*` |
| Diagnostics | `Diagnostics/FStructuralPowerTrace.*` |

Roadmap: [../README.md#roadmap](../README.md#roadmap). Player behavior: [player-guide.md](player-guide.md). History: [../CHANGELOG.md](../CHANGELOG.md).

## Attach path map

| Kind | Place / load | Config gate |
|------|--------------|-------------|
| Pole | `FStructuralBridgeAttach::AttachOnPlace` | always on |
| Storage | same bridge attach (OutletBus host) | `GroupGeneration` |
| Generator | `FStructuralPowerGeneratorProcessor` → bridge attach (OutletBus host) | `GroupGeneration` |
| Extractor | `FStructuralPowerMachineConsumerProcessor` | `GroupResources` |
| Manufacturer | same consumer processor | `GroupProduction` |
| Transport | same consumer processor (stub) | `GroupTransport` |
| Pipe pump | `FStructuralPipeTopology` inject + consumer processor | `GroupPipes` |
| Belts | toggle registry only — no attach | `GroupBelts` |
| Switch | `FStructuralPowerSwitchProcessor` + `FStructuralSwitchBridgeStrategy` | always on |
| Panel | `FStructuralPanelAttach` | `GroupLighting` |
| Light | `FStructuralDeviceAttach::TryAttachConsumer` | `GroupLighting` |

Structure integration goes through `FStructuralSiteMembership` (mount → site → register → strategy). Mount identity is durable **`MountParentId`**; site root is derived via the structure graph. Default structure Ids use human-readable **`Structure N`**. Generators and storage participate in bridge peer mesh like poles.

### Transfer-gated switch path

`FStructuralPowerTransferGate` flips `bStructuralPowerTransferActive` so keyed switch OFF tears down consumer hidden links without destroying structure topology. With `StructuralPower.Trace 1`, check `switch restitch_off_settled` — expect `poweredDirect=0` and `passPanel=0`.

## Trace (developers)

Enable `StructuralPower.Trace 1` in the console (debug only). Log category `LogStructuralPower`; lines often use the `[HALSP]` prefix.

| Pattern | Use |
|---------|-----|
| `switch restitch_*` | Toggle summary — `poweredDirect`, `passPanel` |
| `light path=direct` | Plug HasPower |
| `light path=panel_downstream` | Panel feed + armed |
| `panel ctx=restitch_summary` | Upstream supply snapshot |
| `pole BeginPlay` / `outlet` / `pole wire delta` | Place vs wire-delta paths |

## License

GPL-3.0-or-later. SPDX headers in source files.
