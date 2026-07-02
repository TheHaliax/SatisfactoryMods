# Multiplayer

## Client requirement

StructuralPower sets **`RequiredOnRemote: true`**. Every player joining a session must have the same mod version installed.

Version range in the plugin: `^1.0.0` (compatible 1.0.x).

## Listen server / host

The host runs the graph subsystem and placement hooks. Clients do not simulate structural propagation.

## Dedicated server

Release builds must include server targets:

- **Windows Server** (`FactoryServer-Win64-Shipping`)
- **Linux Server** (`FactoryServer-Linux-Shipping`)

Configure these in Alpakit / `Config/Alpakit.ini` before cooking the SMR zip.

Upload the **release** zip to ficsit.app — not a dev Shipping client-only pack.

## Save compatibility

Graph state lives in the save via the mod subsystem. Server and clients loading the same save see the same persisted graph after rebuild on load.

## Troubleshooting MP

| Symptom | Check |
|---------|--------|
| Mod missing on join | All parties on SMR 1.0.0, SML ^3.12.0 |
| Power works SP, not MP | Server build includes StructuralPower DLL for server target |
| Desync / no propagation | Host-only hooks; verify host has mod, not just clients |
