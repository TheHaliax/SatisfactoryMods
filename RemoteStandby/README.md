# RemoteStandby

**Version 0.1.0** *(beta)* · Satisfactory 1.2 (≥491125) · SML ^3.12.0

Press **Z** while looking at a factory to toggle its **Standby** button (same
`SetIsProductionPaused` state as the interact UI). Works on dedicated servers via RCO.

## Requirements

- Satisfactory 1.2 (≥491125)
- SML ^3.12.0

**Required on remote** — `^0.1.0`.

## Build

```powershell
powershell -File tools/build-mod.ps1 -Mod RemoteStandby
powershell -File tools/build-mod.ps1 -Mod RemoteStandby -Mode Release
```

Build order: clang-format check → version → icons → UBT/Alpakit.

## License

GPL-3.0-or-later
