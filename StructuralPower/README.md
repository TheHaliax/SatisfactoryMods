# StructuralPower

**Version 3.0.0** · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Structural Power adds a hidden power network through structural pieces — foundations, walls, ramps, and bridge poles — so you can power outlets and poles without running visible wires along every segment. **v3.0** fully reworks the underlying attach, load, and reconcile stack on vanilla circuit APIs for performance and large-factory compatibility, and ships opt-in **structural lighting** with **named light groups**. **v2.1** added switch gating and hoverpack tether on top of **v2.0** retroactive structure wiring.

### A note on v2.1 (and why v3.0 is worth another try)

**I'm sorry.** **v2.1.0** added switches and hoverpack tether, but the save-load path change **broke legacy support** so badly on many existing bases that first-time use failed entirely. That was never the intent — and it is why this full rework exists. **v2.0.0** had already introduced retroactive wiring (no rebuild). There was **no public v2.2 release**; the next shipped line is **v3.0.0**.

**v3.0.0** rebuilds the stack from the ground up: session/attach funnel, budgeted remesh on load, and reconcile against FactoryGame circuits — aimed at being **extreme lean at runtime** and **compatible with megabases**, while restoring **full legacy / retroactive support**. Existing structures still wire on load; you do not need to rebuild your factory to use the mod again.

**Honest testing note:** my heaviest internal check so far is a save around **~20 MB**. I have not claimed unlimited megabase coverage beyond that. On **legacy / large saves**, expect:

- a **longer load** than a mod-free session of the same world  
- a **short post-load hitch** that scales roughly with save size (remesh / panel-light seed settling)

After that settle, **post-load gameplay performance has generally been net positive** across the saves I run — the cost is front-loaded into load, not paid every frame. If you bounced on v2.1, this is the version worth returning to.

## How it works

- **Retroactive** — existing structures are wired on load; no rebuild required after installing or updating
- Wire one structural outlet to your grid; connected structure shares the bus
- Ground poles, wall outlets, and towers bridge to the nearest powered structure
- Connectivity is rebuilt from world geometry on load — nothing structural is persisted, so saves can't carry stale links
- **Structural lighting** (opt-in) — lights on powered foundations draw from the bus (named groups via lights control panels)
- **Id assign** (**I**) — Source/Control ids on eligible buildables (lights, switches, panels, and other enabled components); used for light groups, switch subnets, and future keyed features
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

Feature releases after the v3.0 foundation. Later stages are **opt-in** on servers (off until you enable them).

### v3.0.0 — Foundation rewrite *(current)*

- Vanilla-first reconcile — processors, transfer-gated bridges, rebuild-from-geometry
- Stable retroactive load after v2.1 save pain
- Structural lighting, I-key Id config, switch subnets, hoverpack tether
- Session/attach funnel + budgeted load remesh (structure polish)
- Server config via cfg / console / chat only

### v3.1 — Machines *(next)*

- **Generators** — coal, fuel, nuclear, geothermal, wind, HUB biomass
- **Power storage** — charge/discharge on structure grid
- **Resources** — miners, water/oil, fracking, geysers
- **Production** — manufacturers, radar, AWESOME Sink
- **Transport / Pipes / Belts** — wired-power stubs first (stations, pumps); full topology in later stages

### v3.2 — Pipe topology

- Pipe runs extend the bus; mid-run pumps

### v3.3 — Belt topology

- Conveyor runs to remote miners

### v3.4 — Hypertube topology

- Launcher draw along hyper runs

### v3.5 — Rail topology

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
