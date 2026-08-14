# RemoteStandby

**Version 0.1.1** *(beta)* · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Press **Z** while looking at a factory to toggle its **Standby** button — same
production-paused state as the interact UI, without opening the machine panel.

## How it works

- Look at a factory buildable and press **Z**
- Toggles standby on/off on the server (dedicated-safe)
- Skips when another interact UI is already open

## Requirements

- Satisfactory 1.2 (≥491125)
- [SML](https://ficsit.app/mod/SML) ^3.12.0

## Multiplayer

**Required on remote** — all players need the same mod version (`^0.1.0`). Authority
applies the standby change; clients send the request.

## Source

GPL-3.0 — [github.com/TheHaliax/SatisfactoryMods](https://github.com/TheHaliax/SatisfactoryMods)
