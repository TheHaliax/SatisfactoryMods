# StructuralPower

**Version 2.0.0** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Structural Power adds a hidden power network through structural pieces — foundations, walls, ramps, and bridge poles — so you can power outlets and poles without running visible wires along every segment.

## How it works

- **Retroactive** — existing structures are wired on load; no rebuild required after installing or updating
- Wire one structural outlet to your grid; connected structure shares the bus
- Ground poles, wall outlets, and towers bridge to the nearest powered structure
- Connectivity is rebuilt from world geometry on load — nothing structural is persisted, so saves can't carry stale links

## Roadmap

Feature releases after the base bus. Later categories are **opt-in** on servers (off until you enable them), except where noted for v2.1.

### v2.0.0 — Structural bus *(current)*

- Hidden power through foundations, walls, ramps, and connected geometry
- Bridge poles, wall outlets, and towers join the bus without wiring every piece
- Retroactive on load — no rebuild after install or update
- One wire to a structural outlet can power the whole connected build

### v2.1 — Switches & hoverpack *(in development)*

- **Power switches** on structures gate power in and out of a build (on by default) — wire once at the edge, switch controls the slab
- **Hoverpack** tethers from **powered structure** nearby — fly above, below, or beside your base without peppering poles; reach adjustable on server (default 2× vanilla pole range)
- Persistent **server config** and live console toggles for hosts and dedicated operators

### v2.2 — Lighting

- **Lights** draw from the structural bus — no daisy-chain wiring on every foundation
- **Light control panels** with **named groups** — multiple independent light zones on one structure

### v2.3 — Generators & storage

- **Generators** (coal, fuel, nuclear, geothermal, wind, etc.) feed the bus directly
- **Power storage** charges and discharges on the structure grid
- **HUB biomass generators** included when they unlock in progression

### v2.4 — Machines & utilities

- **Extractors** — miners, water pumps, resource-well equipment
- **Manufacturers** — constructors through packagers and variable-power machines
- **Transport** — train/truck/drone stations, jump pads, portals, elevators
- **Misc** — radar tower, AWESOME Sink

### v2.5 — Pipes

- **Pipe runs** extend the structural bus along fluid networks
- **Pipeline pumps** on those runs draw without a separate wire to each pump

### v2.6 — Belts, rails & hypertubes

- **Conveyor runs** carry the bus to remote miners and mid-line buildings
- **Railways** on powered foundations extend reach (train power stays vanilla — separate from third rail)
- **Hypertube lines** power launchers along the tube run for long-distance QoL

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

Works on client and all dedicated servers (Windows and Linux).

Dedicated-server settings and console commands will be documented here in a future release.

![StructuralPower in-game — foundation and pole on structural bus](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/StructuralPower/Screenshots/gameplay-foundation-pole.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)
