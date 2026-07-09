# StructuralPower

**Version 3.0.0** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Structural Power adds a hidden power network through structural pieces — foundations, walls, ramps, and bridge poles — so you can power outlets and poles without running visible wires along every segment. **v3.0** rewrites attach and reconcile on vanilla circuit APIs. **v2.x** added switch gating, hoverpack tether, and opt-in **structural lighting** with **named light groups**.

> **Welcome back if v2.1 put you off:** **v2.0.0** retroactively wired existing saves on load — no rebuild required. **v2.1.0** added switches and hoverpack but changed save-load processing; on many bases that **broke first-time use entirely** (unintentional — sorry). **v2.2.0** shipped lighting and partial load fixes, but the old attach model still had edge cases. **v3.0.0** rewrites the power stack around FactoryGame circuit APIs while **keeping retroactive support** — if you bounced on v2.1 or an unstable v2.2 build, this is the version worth another shot.

## How it works

- **Retroactive** — existing structures are wired on load; no rebuild required after installing or updating
- Wire one structural outlet to your grid; connected structure shares the bus
- Ground poles, wall outlets, and towers bridge to the nearest powered structure
- Connectivity is rebuilt from world geometry on load — nothing structural is persisted, so saves can't carry stale links
- **Structural lighting** (opt-in) — lights on powered foundations draw from the bus; use **I** to assign Source/Control ids for groups and switch subnets
- **Power switches** gate keyed subnets on structures — optional pole bridge; Mode B keyed subnets by default
- **Hoverpack** tethers from nearby **powered structure** geometry — adjustable horizontal/vertical reach on the server

## Chat commands

Type in the in-game chat on the **server or listen host** (dedicated-server operators with authority). Commands start with `!` and do not appear in public chat. Responses show as **Hal:** system messages.

Available commands (`[]` — required argument):

- `!lighting` — toggle **structural lighting** (default off)
- `!HoverH [1-10]` — set hoverpack **horizontal** reach multiplier (saved to cfg)
- `!HoverV [1-10]` — set hoverpack **vertical** reach multiplier (saved to cfg)
- `!pwrhelp` — list Structural Power chat commands

Settings persist to `Configs/StructuralPower.cfg` on the server/host. Change via chat, console (`StructuralPower.Set`), or edit the cfg file directly. Chat Mk 2 expandable help lists the same commands when installed.

## Roadmap

Feature releases after the v3.0 foundation. Later categories are **opt-in** on servers (off until you enable them).

### v3.0.0 — Foundation rewrite *(current · SMR/deploy pending)*

- Vanilla-first reconcile — processors, transfer-gated bridges, rebuild-from-geometry
- Stable retroactive load after v2.1 save pain
- Structural lighting, I-key Id config, switch subnets, hoverpack tether
- Server config via cfg / console / chat only

### v3.1 — Machines *(in development)*

- **Generators** — coal, fuel, nuclear, geothermal, wind, HUB biomass
- **Power storage** — charge/discharge on structure grid
- **Resources** — miners, water/oil, fracking, geysers
- **Production** — manufacturers, radar, AWESOME Sink
- **Transport / Pipes / Belts** — wired-power stubs first (stations, pumps); full topology in later releases

### Later — logistics topology

- **Pipes** — pipe runs extend the bus; mid-run pumps
- **Belts** — conveyor runs to remote miners
- **Hypertubes** — launcher draw along hyper runs
- **Rails** — bed coupling; train power stays vanilla third rail

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

Works on client and all dedicated servers (Windows and Linux). **Required on remote** — all players need the same mod version (`^3.0.0`).

Server operators: edit `Configs/StructuralPower.cfg` on the dedicated host, or use console / chat when connected as authority. See [Documentation/multiplayer.md](Documentation/multiplayer.md) and [Documentation/chat-commands.md](Documentation/chat-commands.md).

![StructuralPower in-game — foundation and pole on structural bus](https://raw.githubusercontent.com/TheHaliax/SatisfactoryMods/refs/heads/main/StructuralPower/Screenshots/gameplay-foundation-pole.jpg)

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)
