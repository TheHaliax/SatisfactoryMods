# Multiplayer

## Client requirement

StructuralPower sets **`RequiredOnRemote: true`**. Every player joining a session must have the same mod version installed.

Version range in the plugin: `^3.0.0` (compatible 3.x clients on the 3.0 line).

## Listen server / host

The host runs the graph subsystem and placement hooks. Clients do not simulate structural propagation.

Mod config and chat commands apply on the **authority** (host). Clients cannot change server settings — host or dedicated operator uses cfg, console, or chat.

**I-key Id panel** runs on the local client; apply sends RCO to server. All players need matching **3.1.0** (or compatible `^3.0.0`) for the panel UI and machine groups.

## Dedicated server

Release builds must include server targets:

- **Windows Server** (`FactoryServer-Win64-Shipping`)
- **Linux Server** (`FactoryServer-Linux-Shipping`)

Configure these in Alpakit / `Config/Alpakit.ini` before cooking the SMR zip.

Upload the **Alpakit Release** zip (`StructuralPower.zip`) to [ficsit.app](https://ficsit.app/mod/StructuralPower). SMR serves server `.smod` files from that upload — not the smaller per-target zips in `ArchivedPlugins/`.

### Server config path

```
<Satisfactory>/Configs/StructuralPower.cfg
```

Edit via console `StructuralPower.Set`, chat `!` commands when connected as authority, or edit the cfg file on the server host. See [chat-commands.md](chat-commands.md).

## Save compatibility

Nothing structural is written to the save. The host rebuilds the connectivity graph from world geometry on load, so any save opens cleanly regardless of which version last touched it — no stale or "wireless" links carry over.

Per-device **Source/Control** overrides (lights, switches, panels) are stored in the save (graph subsystem). Mod runtime bus components are stripped on load and recreated.

## Troubleshooting MP

| Symptom | Check |
|---------|--------|
| Mod missing on join | All parties on SMR 3.1.0, SML ^3.12.0 |
| Power works SP, not MP | Server build includes StructuralPower DLL for server target |
| Desync / no propagation | Host-only hooks; verify host has mod, not just clients |
| Chat commands ignored | Run on server/listen host; pure client gets Hal error message |
| Id panel apply no effect | Server authority; RCO must run on host — check log for `[HALSP] Id panel apply` |
| Hoverpack tether wrong | Multipliers are server CVars — host sets via `!HoverH` / `!HoverV` or cfg |
