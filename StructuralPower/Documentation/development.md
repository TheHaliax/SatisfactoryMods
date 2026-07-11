# Development

## Code comments vs documentation

| Surface | Role |
|---------|------|
| **`Source/`** | Default **no comments** — only non-obvious engine traps inline (FG circuit side-effects, load asserts, MP client gaps). If behavior needs teaching, put it here or in local `development/ARCHITECTURE.md`. |
| **`Documentation/`** (this folder) | Player + contributor **what**, **why**, **how** |
| **`development/`** (gitignored, local) | As-built architecture, log semantics, ship gate — agents read [DOCUMENTATION.md](../../development/DOCUMENTATION.md) first |

**Policy (POLICY §13):** no class blurbs, no `DR-*` refs, no milestone notes in `Source/`. SPDX headers stay.

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

```powershell
powershell -File tools/pack-structuralpower.ps1 -Config Shipping
```

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
| Mod config | `FStructuralPowerModConfig.cpp` |
| Chat `!` + SML help | `FStructuralPowerBangCommands.cpp`, `StructuralPowerSmlChatCommands.cpp` |
| Hoverpack bridge | `FStructuralHoverpackBridge.cpp` |
| Switch listener / RCO | `UStructuralPowerSwitchListener.cpp`, `UStructuralPowerRCO.cpp` |
| Poles, circuit & subsystem | `AStructuralPowerGraphSubsystem.cpp` |
| Reconcile / restitch | `FStructuralPowerReconcile.cpp`, `FStructuralPowerRestitch.cpp` |
| Circuit graph ops | `FStructuralGraphCircuitOps.cpp` |
| Site echo state | `FStructuralSiteState.cpp` |
| Cross-site graph | `FStructuralCrossSiteGraph.cpp` |
| Id widget helpers | `FStructuralPowerIdShellBuilder.cpp`, `FStructuralPowerIdModalHost.cpp`, … |
| **Processors** | `FStructuralPowerSwitchProcessor`, `FStructuralPowerPanelProcessor`, `FStructuralPowerLightProcessor`, `FStructuralPowerPoleProcessor`, `FStructuralPowerStorageProcessor`, `FStructuralPowerTransferGate`, `FStructuralPowerBridgeProcessor` |
| **Bridge attach (pole + storage)** | `Attach/FStructuralBridgeAttach.cpp` — shared `IntegrateOnPlace` place/load path |
| **Connection points** | `FStructuralPoleConnectionPoint`, `FStructuralSwitchConnectionPoint`, `FStructuralPanelConnectionPoint`, `FStructuralStorageConnectionPoint` |
| Structural connectivity graph (spatial hash + union-find) | `FStructuralConnectivityGraph.cpp` |
| Lightweight index | `FStructuralLightweightIndex.cpp` |
| Eligibility rules | `FStructuralEligibilityRules.cpp` |
| Factory tick drain | `UStructuralPowerFactoryTickHandler.cpp` |
| Diagnostics | `FStructuralPowerDiagnostics.cpp`, `FStructuralPowerTrace.cpp` |
| **I-key input / Id panel** | `FStructuralPowerIdInput.cpp`, `UStructuralPowerIdConfigWidget.cpp`, `UStructuralPowerIdOptionManager.cpp` |
| **Panel attach / control bus** | `FStructuralPanelAttach.cpp`, `FStructuralPanelControlledSync.cpp`, `UStructuralPowerPanelListener.cpp` |
| **Light consumer attach** | `FStructuralDeviceAttach.cpp` |
| **Routing / id pools** | `FStructuralPowerRouter.cpp`, `EStructuralChannel.h`, `CollectIdsOnComponent` |
| **Id RCO apply** | `Network/UStructuralPowerRCO.cpp` |
| **Reload safety** | `StripPersistedEndpointModComponents`, pre-`BeginPlay` hooks in `StructuralPowerRootInstanceModule.cpp` |

Roadmap: [../README.md#roadmap](../README.md#roadmap). Player-facing behavior: [player-guide.md](player-guide.md). Release history: [../CHANGELOG.md](../CHANGELOG.md).

### Source/Control extension model

Lighting uses **Source/Control** on `Light` and `Switch` channels. The router reserves `Generator`, `Extractor`, `Manufacturer`, `Transport`, and `Misc` for future opt-in features.

| Mechanism | Reuse |
|-----------|-------|
| `FStructuralEndpointOverrides` (save) | Any buildable |
| `CollectIdsOnComponent` id pools | Per-feature namespace buckets |
| `ResolveSubnetFeedConnector` | Switch-gated sources |
| `FStructuralDeviceAttach` | Consumer hidden-link pattern |
| Pre-`BeginPlay` mod component strip | Transient outlet bus |

Lighting-specific: `PanelControlBus`, `FStructuralPanelControlledSync`, panel listener — not reused by generators; same graph/RCO/hook patterns apply.

### Bridge attach (pole + storage)

**What:** `FStructuralBridgeAttach::AttachOnPlace` — one place/load pipeline for non-toggle bridges.  
**Why:** Runtime poles skipped `ProcessOutlet` when build hooks missed; wire-delta duplicated partial integrate.  
**How:**

```
BeginPlay / OnBuildEffectFinished / bulk drain
  → ProcessOutlet → pole|storage processor
  → FStructuralBridgeAttach::AttachOnPlace
  → IntegrateOnPlace (link FG visible ports → SP#, peer mesh, promote)
```

Wire delta: no `ParentId` membership yet → full `ProcessPoleEndpoint` once; else thin link/mesh in connection point. **Switch** keeps `ApplyStructureMembership` + bridge strategy; **panel** unchanged (`FStructuralPanelAttach`, `GroupLighting` gate).

`bStructuralPowerTransferActive = false` suspends new consumer wiring only — tracked topology stays until explicit tear-down.

### Transfer-gated switch path

**What:** Keyed switch OFF tears down consumer hidden links without removing structure topology.  
**Why:** Old toggle path remeshed whole sites → stalls + circuit storms.  
**How:** `FStructuralPowerTransferGate` flips `bStructuralPowerTransferActive`; verify with `restitch_off_settled` (`passPanel=0`, `poweredDirect=0`). Full architecture: local `development/ARCHITECTURE.md` (gitignored).

## HALSP trace (developers)

Enable `StructuralPower.Trace 1` in the console (debug only).

| Prefix | Use |
|--------|-----|
| `[HALSP] switch restitch_*` | Toggle behavior summary — `poweredDirect`, `passPanel` are pass probes |
| `[HALSP] light path=direct` | `lit=` = plug HasPower |
| `[HALSP] light path=panel_downstream` | `pass=` = `panelFed AND armedOn` |
| `[HALSP] panel ctx=restitch_summary` | `panelFed=` = upstream supplies at snapshot |
| `[HALSP] hook … panel_subnet_sync detail=` | Echo skip reason — see local `LOG-INTERPRETATION.md` |
| `[HALSP] outlet Build_PowerPole` | Runtime pole place — bus↔visible + site integrate |
| `[HALSP] pole BeginPlay` | Pole enqueue when build-effect hook missed |
| `[HALSP] pole wire delta` | Thin delta after membership exists |

**OFF result:** read `switch restitch_off_settled` — expect `poweredDirect=0 passPanel=0`.  
`armedPanel>0` with `passPanel=0` after OFF is normal (panel still armed, no feed).

Local agent tooling (not shipped): repo `development/` — `DOCUMENTATION.md`, `LOG-INTERPRETATION.md`, `analyze-halsp-log.ps1` (gitignored).

## License

GPL-3.0-or-later. SPDX headers in source files.
