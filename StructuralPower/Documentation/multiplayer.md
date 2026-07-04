# Multiplayer

## Client requirement

StructuralPower sets **`RequiredOnRemote: true`**. Every player joining a session must have the same mod version installed.

Version range in the plugin: `^2.0.0` (compatible 2.x).

## Listen server / host

The host runs the graph subsystem and placement hooks. Clients do not simulate structural propagation.

## Dedicated server

Release builds must include server targets:

- **Windows Server** (`FactoryServer-Win64-Shipping`)
- **Linux Server** (`FactoryServer-Linux-Shipping`)

Configure these in Alpakit / `Config/Alpakit.ini` before cooking the SMR zip.

Upload the **Alpakit Release** zip (`StructuralPower.zip`) to [ficsit.app](https://ficsit.app/mod/StructuralPower). SMR serves server `.smod` files from that upload — not the smaller per-target zips in `ArchivedPlugins/`.

## Save compatibility

Nothing structural is written to the save. The host rebuilds the connectivity graph from world geometry on load, so any save opens cleanly regardless of which version last touched it — no stale or "wireless" links carry over.

## Troubleshooting MP

| Symptom | Check |
|---------|--------|
| Mod missing on join | All parties on SMR 2.0.0, SML ^3.12.0 |
| Power works SP, not MP | Server build includes StructuralPower DLL for server target |
| Desync / no propagation | Host-only hooks; verify host has mod, not just clients |
