# StructuralPower

**Version 3.1.3** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Structural Power adds a hidden power network through structural pieces — foundations, walls, ramps, and bridge poles — so you can power outlets and poles without running visible wires along every segment. **v3.1** adds opt-in **machine groups** and **pipe topology** (generators, storage, resources, production; fluid supports and machine pipe ports → inline pumps; transport stub). **v3.0** reworked attach, load, and reconcile on vanilla circuit APIs with opt-in **structural lighting** and **named light groups**.

## How it works

- **Retroactive** — existing structures are wired on load; no rebuild required after installing or updating
- Wire one structural outlet to your grid; connected structure shares the bus
- Ground poles, wall outlets, and towers bridge to the nearest powered structure
- Connectivity is rebuilt from world geometry on load — nothing structural is persisted, so saves can't carry stale links
- **Machines** (opt-in, default off) — generators and power storage attach on structure; miners and manufacturers draw from the same bus when a bus host is present
- **Pipes** (opt-in, default off) — fluid pipe supports and machine pipe ports inject structure power into connected pipe runs; inline pumps draw from that bus (not hypertubes)
- **Structural lighting** (opt-in) — lights on powered foundations draw from the bus (named groups via lights control panels)
- **Id assign** (**I**) — Source/Control ids on eligible buildables (lights, switches, panels, and other enabled components)
- **Power switches** gate keyed subnets on structures — optional pole bridge; Mode B keyed subnets by default
- **Hoverpack** tethers from nearby **powered structure** geometry — adjustable horizontal/vertical reach on the server

## Chat commands

Type in the in-game chat on the **server or listen host** (dedicated-server operators with authority). Commands start with `!` and do not appear in public chat. Responses show as **Hal:** system messages.

Available commands (`[]` — required argument):

- `!lighting` — toggle **structural lighting** (default off)
- `!Generation` — toggle **generators + power storage** on structure (default off)
- `!Resources` — toggle **miners / extractors** (default off)
- `!Production` — toggle **manufacturers / radar / sink** (default off)
- `!Transport` — toggle **wired transport** consumers — stub (default off)
- `!Pipes` / `!pipe` — toggle **structural pipe bus** (supports + machine pipe ports → pumps; default off)
- `!Belts` — toggle **belts** group — no attach yet (default off)
- `!HoverH [1-10]` — set hoverpack **horizontal** reach multiplier (saved to cfg)
- `!HoverV [1-10]` — set hoverpack **vertical** reach multiplier (saved to cfg)
- `!pwrhelp` — list Structural Power chat commands

Settings persist to `Configs/StructuralPower.cfg` on the server/host. Change via chat, console (`StructuralPower.Set`), or edit the cfg file directly. Chat Mk 2 expandable help lists the same commands when installed.

## Roadmap

Feature releases after the v3.0 foundation. Stages are **opt-in** on servers (off until you enable them).

### v3.0.0 — Foundation rewrite *(previous)*

- Vanilla-first reconcile — processors, transfer-gated bridges, rebuild-from-geometry
- Stable retroactive load after v2.1 save pain
- Structural lighting, I-key Id config, switch subnets, hoverpack tether
- Faster / more stable load remesh on large saves
- Server config via cfg / console / chat only

### v3.1 — Machines + pipes *(current, 3.1.3)*

**3.1.3** simplifies the pipe-support spline lookup. **3.1.2** recooks vs CL 502094 (protected `OnBuildEffectFinished` hook). **3.1.1** fixes satisfactory-calculator.com save uploads (SCIM-safe persistence of
Id-panel names and overrides).

- **Generators** — coal, fuel, nuclear, geothermal, wind, alien booster, HUB biomass
- **Power storage** — charge/discharge on structure grid
- **Resources** — miners, water/oil, fracking, geysers
- **Production** — manufacturers, radar, AWESOME Sink
- **Pipes** — fluid supports + machine pipe ports inject runs; inline pumps on bus (`!Pipes` / `!pipe`)
- **Transport** — wired-power stub (stations, elevators)
- **Belts** — toggle only (no attach)

### v3.2 — Belt topology *(in development)*

- Conveyor runs to remote miners

### v3.3 — Hypertube topology

- Launcher draw along hyper runs

### v3.4 — Rail topology

- Rail bed coupling; train power stays vanilla third rail

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

Works on client and all dedicated servers (Windows and Linux). **Required on remote** — all players need the same mod version (`^3.0.0`).

Server operators: edit `Configs/StructuralPower.cfg` on the dedicated host, or use console / chat when connected as authority. See [Documentation/multiplayer.md](Documentation/multiplayer.md) and [Documentation/chat-commands.md](Documentation/chat-commands.md).

![StructuralPower in-game — foundation and pole on structural bus](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/StructuralPower/Screenshots/gameplay-foundation-pole.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)
