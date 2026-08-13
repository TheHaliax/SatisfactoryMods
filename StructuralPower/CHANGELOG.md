# Changelog

## 3.1.2 — 2026-08-13

Compatibility fix for **Satisfactory 1.2.4 (CL 502094, UE 5.6.1)**. No gameplay or save-format
changes. Verified on a Linux dedicated server: clean world load, 42 pipe supports injected, bulk
drain completed.

- **Fatal hook failure on launch** — CL502094 compiles `AFGBuildableCircuitBridge::OnCircuitsRebuilt`
  and `AFGBuildablePowerPole::OnPowerConnectionChanged` into bodies too short for funchook to
  detour. SML escalates a failed hook to `Fatal`, so the game died in `UGameInstance::Init` with
  `funchook failed: Too short instructions` before reaching the main menu. Both signals are
  re-driven from non-virtual functions that still detour cleanly: circuit rebuilds from
  `AFGCircuitSubsystem::SetCircuitBridgesModified`, and pole wire deltas from
  `UFGCircuitConnectionComponent::AddConnection` / `RemoveConnection`, resolving the pole from the
  connection component's owner. The rebuild signal names no bridge, so every switch is re-evaluated
  on it; authority filtering and deferral in `HandleSwitchCircuitsRebuilt` are unchanged. The two
  matching diagnostics hooks are dropped — they only logged under extended debug.

- **Dedicated-server crash during world load** — `FStructuralPipeTopology` made virtual calls on
  `AFGBuildablePipeBase` and `AFGBuildablePipeline`. Virtual calls bind by vtable *index*, fixed
  when the mod compiles, while ordinary calls bind by *symbol name* through the PLT at load. The
  mod compiles against a FactoryGame stub and binds to the shipped FactoryGame at runtime, so
  drift between the two shifts every index computed from those headers while symbol-resolved calls
  keep working. On CL502094 those calls landed on neighbouring vtable slots:

  | Called | Arrived in |
  |---|---|
  | `GetSplineComponent()` | `AFGBuildablePipeBase::CreateClearanceData` |
  | `GetPipeConnections()` | `AFGBuildablePipelinePump::OnFluidDescriptorSet` |

  `GetPipeConnections` and `OnFluidDescriptorSet` are declared on consecutive lines in
  `FGBuildablePipeline.h` — an off-by-one slot. Disassembly of the shipped `.so` showed the faulting
  line as `call *0xb58(%rax)`, an indirect call, with every symbol-resolved call in the same loop
  unaffected. Object layout is not affected by this skew, since adding or removing virtuals changes
  vtable contents rather than field offsets — which is why property reads were always fine.

  Both call sites are removed. `FindTouchedPipe` takes the pipeline's `USplineComponent` from the
  actor's component set with `TInlineComponentArray` and measures against it with
  `USplineComponent::FindLocationClosestToWorldLocation`. `CollectConnectedConductors` drops its
  per-class connection chain entirely — every component it fetched was already in the component
  array gathered a line earlier. Neither function contains an indirect call in the built binary.

- **Latent build break** — `FStructuralPowerPoleProcessor.cpp` used
  `UFGStructuralPowerConnectionComponent` without including its header, compiling only because
  Unreal's unity build placed it beside a file that did.

## 3.1.1 — 2026-07-22

- **SCIM-safe save format** — Id-panel defaults and overrides persist as flat records
  (string class paths); legacy struct-keyed maps load once and save empty. Fixes
  satisfactory-calculator.com "readStructProperty" upload failure on saves containing
  StructuralPower. Existing saves migrate on first load + save.

## 3.1.0 — 2026-07-16

Opt-in machine groups and **pipe topology** on the structural bus (default **off**). Generators and power storage host a site bus on foundations; resource and production consumers attach to the same structure root. Fluid pipe runs inject from supports and machine pipe ports; inline pumps draw from the bus. Transport is a wired-consumer stub; belts are toggle-only.

- **Generation** — `!Generation` / `GroupGeneration`; gens (coal, fuel, nuclear, geo, wind, alien booster) + power storage as OutletBus hosts; HUB biomass; no pole required for gen attach on structure
- **Resources** — `!Resources` / `GroupResources`; miners, water/oil, fracking, geysers attach as consumers when a bus host exists on the structure
- **Production** — `!Production` / `GroupProduction`; manufacturers, radar, AWESOME Sink
- **Pipes** — `!Pipes` / `!pipe` / `GroupPipes`; `FStructuralPipeTopology` unions fluid conductors (pipelines, attachments, pipe reservoirs; hypertubes excluded); fluid pipe supports + machine pipe ports inject structure power into runs; inline pumps consume from the bus
- **Transport** — `!Transport` / `GroupTransport` stub: wired stations attach; **no** track topology yet
- **Belts** — `!Belts` / `GroupBelts` toggle persistence only (no attach)
- **Factory place** — `AFGBuildableFactory::BeginPlay` enqueue so miners/gens are not missed when build-effect hooks skip
- **Remove path** — mass foundation dismantle batches UF reunite next tick; lightweight spatial unindex is O(cells) per pad (no full-grid rebuild)
- **Config** — machine group keys via cfg / `StructuralPower.Set` / `!` chat (same scaffold as lighting)

## 3.0.0 — 2026-07-09

Architecture rewrite on vanilla circuit APIs — processors, transfer-gated bridges, rebuild-from-geometry, budgeted remesh. Restores stable retroactive load after the 2.1 save-path regression.

- **Reconcile** — promote/demote and panel downstream via FactoryGame circuit APIs; no parallel power sim
- **Processors** — light, panel, switch, bridge, and site-bus paths split into dedicated processors + reconcile facade
- **Connection model** — bridge pairs, transfer-gated wire/toggle, integrate-on-place for eligible buildables
- **Structural lighting** (opt-in, default off) — lights on powered structure draw from the bus; `!lighting` / `GroupLighting` cfg or console
- **Lights control panels** — keyed zones per structure; panel `Control` names a light group; lights match via `Source`; per-panel control bus so vanilla E no longer bleeds to every light on the structure
- **Id panel (I)** — Source/Control ids on eligible buildables (global — light groups, switch subnets, …); structure-scoped dropdowns; overrides in world save (RCO), not `.cfg`
- **Switch subnets** — Mode B keyed subnets by default; keyed consumers follow switch ON/OFF
- **Hoverpack tether** — reach multipliers default **1.2×** (was 1.5×); clamp 1.0–10.0
- **Config** — `Configs/StructuralPower.cfg`, console `StructuralPower.Set`, and `!` chat only; **SML pause-menu UI removed**
- **Load / save** — bulk load drain, spatial parent resolve, indexed restitch; mod bus components stripped before bridge BeginPlay; topology rebuilt from live geometry every load
- **GroupLighting reconcile** — toggling structural lighting demotes/suspends keyed transfer cleanly
- **Logging** — `[HALSP]` prefix; trace/extended debug console-only (no `!tracetoggle`)
- **Bridge attach** — shared place/load for poles and power storage; switch wire-delta/bulk-load parity with runtime placement; pole BeginPlay enqueue when `OnBuildEffectFinished` missed

## 2.1.0 — 2026-07-04

Switches, hoverpack structural tether, SML pause-menu config, and UtilityMod-style chat commands.

- **Power switches:** structural gating with Mode B keyed subnets by default (Mode A whole-component via config)
- **Hoverpack:** virtual tether from powered structure geometry; separate horizontal/vertical reach multipliers (default 1.5×, clamp 1.0–10.0)
- **Config:** SML mod menu (Mods → Structural Power) + `Configs/StructuralPower.cfg`; debug toggles in collapsible **Debug** section
- **Chat:** `!HoverH`, `!HoverV`, `!tracetoggle`, `!pwrhelp` — Hal system replies; does not block other mods' `!` commands
- **Console:** `StructuralPower.Set <key> <value>` mirrors mod config keys
- Per-building switch **Ids** remain in world save (RCO), not `.cfg`

## 2.0.0 — 2026-07-03

Retroactive support for existing structures and a rebuilt connectivity engine.
The structural graph is now derived from live world geometry on load instead of
being persisted, so bases built before installing (or before an earlier version)
are powered correctly without rebuilding. Structural pieces are decoupled from
the power circuit — only bridge poles join the circuit — which removes the
load-time stutter and the false "power everywhere" behaviour seen in earlier
builds.

- Retroactive: existing foundations, walls, ramps, and poles are wired on load — no rebuild required
- Pole-to-pole architecture: structural pieces form a data-only connectivity graph (spatial hash + union-find); poles carry power between components and are promoted to the game circuit only when a pole in the component is actually powered
- Rebuild-from-geometry on load: structural links are no longer saved, eliminating stale/"wireless" links carried over from contaminated saves
- Performance: incremental component merge/split on build and demolish; no O(N²) circuit rebuilds while loading
- Retains the persistent-power fix and broad mesh support from 1.1.0

## 1.1.0 — 2026-07-03

More structure support and a fix for persistent power: a structure
disconnected from the active grid no longer stays powered. Incompatible with
structures built by earlier versions — only new builds from this update onward
behave correctly.

- Broader structural mesh support (thin / edge / beam / ramp segments propagate reliably)
- Persistent power fixed: disconnecting a structure from the active grid now drops its power instead of leaving it lit
- No retroactive fix for existing modded structures; rebuild affected structures after updating

## 1.0.0 — initial release

Hidden power network through connected foundations, walls, ramps, and bridge poles. Wire one point to your grid; the structural mesh shares the circuit without cables on every segment.

- New placements only (no retroactive save scan)
- Bridge poles link visible outlets to the hidden bus
- Graph persists on save/load
- MP: required on clients; dedicated server targets included

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.
