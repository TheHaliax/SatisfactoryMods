# Development

## Code comments vs documentation

| Surface | Role |
|---------|------|
| **`Source/`** | Almost no comments — only non-obvious engine traps inline. If behavior needs explanation, it belongs in docs. |
| **`Documentation/`** (this folder) | Player + contributor **what**, **why**, **how** |
| **`development/`** (gitignored, local) | As-built architecture, log semantics, ship gate — agents read [DOCUMENTATION.md](../../development/DOCUMENTATION.md) first |

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
| Mod config + SML menu | `UStructuralPowerModConfiguration.cpp`, `FStructuralPowerModConfig.cpp` |
| Chat `!` commands | `FStructuralPowerBangCommands.cpp` |
| Hoverpack bridge | `FStructuralHoverpackBridge.cpp` |
| Switch listener / RCO | `UStructuralPowerSwitchListener.cpp`, `UStructuralPowerRCO.cpp` |
| Poles, circuit & subsystem | `AStructuralPowerGraphSubsystem.cpp` |
| Reconcile / restitch | `FStructuralPowerReconcile.cpp`, `FStructuralPowerRestitch.cpp` |
| Circuit graph ops | `FStructuralGraphCircuitOps.cpp` |
| Site echo state | `FStructuralSiteState.cpp` |
| Cross-site graph | `FStructuralCrossSiteGraph.cpp` |
| Id widget helpers | `FStructuralPowerIdShellBuilder.cpp`, `FStructuralPowerIdModalHost.cpp`, … |
| **Processors** | `FStructuralPowerSwitchProcessor`, `FStructuralPowerPanelProcessor`, `FStructuralPowerLightProcessor`, `FStructuralPowerPoleProcessor`, `FStructuralPowerTransferGate`, `FStructuralPowerBridgeProcessor` |
| **Connection points** | `FStructuralPoleConnectionPoint`, `FStructuralSwitchConnectionPoint`, `FStructuralPanelConnectionPoint` |
| Structural connectivity graph (spatial hash + union-find) | `FStructuralConnectivityGraph.cpp` |
| Lightweight index | `FStructuralLightweightIndex.cpp` |
| Eligibility rules | `FStructuralEligibilityRules.cpp` |
| Factory tick drain | `UStructuralPowerFactoryTickHandler.cpp` |
| Diagnostics | `FStructuralPowerDiagnostics.cpp`, `FStructuralPowerTrace.cpp` |
| **v2.2 — I-key input** | `FStructuralPowerIdInput.cpp` |
| **v2.2 — Id panel UI** | `UStructuralPowerIdConfigWidget.cpp`, `UStructuralPowerIdOptionManager.cpp` |
| **v2.2 — Panel attach / control bus** | `FStructuralPanelAttach.cpp` |
| **v2.2 — Keyed panel sync** | `FStructuralPanelControlledSync.cpp`, `UStructuralPowerPanelListener.cpp` |
| **v2.2 — Light consumer attach** | `FStructuralDeviceAttach.cpp` |
| **v2.2 — Routing / id pools** | `FStructuralPowerRouter.cpp`, `EStructuralChannel.h`, `CollectIdsOnComponent` |

Feature notes: [v2.2.md](v2.2.md). Roadmap: [../README.md#roadmap](../README.md#roadmap).

### Transfer-gated switch path (v2.2)

**What:** Keyed switch OFF tears down consumer hidden links without removing structure topology.  
**Why:** Old toggle path remeshed whole sites → stalls + circuit storms.  
**How:** `FStructuralPowerTransferGate` flips `bStructuralPowerTransferActive`; verify with `restitch_off_settled` (`passPanel=0`, `poweredDirect=0`). Full architecture: local `development/CURRENT-STATE.md` §7 (gitignored).

## HALSP trace (developers)

Enable `StructuralPower.Trace 1`, mod menu **Debug → PWR trace logging**, or `!tracetoggle`.

| Prefix | Use |
|--------|-----|
| `[HALSP] switch restitch_*` | Toggle behavior summary — `poweredDirect`, `passPanel` are pass probes |
| `[HALSP] light path=direct` | `lit=` = plug HasPower |
| `[HALSP] light path=panel_downstream` | `pass=` = `panelFed AND armedOn` |
| `[HALSP] panel ctx=restitch_summary` | `panelFed=` = upstream supplies at snapshot |
| `[HALSP] hook … panel_subnet_sync detail=` | Echo skip reason — see local `LOG-INTERPRETATION.md` |

**OFF result:** read `switch restitch_off_settled` — expect `poweredDirect=0 passPanel=0`.  
`armedPanel>0` with `passPanel=0` after OFF is normal (panel still armed, no feed).

Local agent tooling (not shipped): repo `development/` — `DOCUMENTATION.md`, `LOG-INTERPRETATION.md`, `analyze-halsp-log.ps1` (gitignored).

## License

GPL-3.0-or-later. SPDX headers in source files.
