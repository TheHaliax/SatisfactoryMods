# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `PipelineColor.zip` | Alpakit **Release** output (all targets) | Upload once to SMR |
| `PipelineColor-Windows.zip` etc. | Per-target slices | Local debugging |
| `PipelineColor-*.smod` | SMR-hosted per target | Players / ficsit-cli |

## Release checklist (1.2.1 — 2026-07-24)

1. **Version fields** in `PipelineColor.uplugin` — `SemVersion` / `VersionName` **1.2.1**, `RemoteVersionRange` **^1.2.0**, `Version` integer **1**, `GameVersion` **>=491125**, SML **^3.12.0**, `IsBetaVersion` **false**.
2. **Icons** — `powershell -File tools/icons.ps1 PipelineColor` → badge **V1.2** on `Resources/Icon*.png` (badge tracks major.minor).
3. **Version guard** — `powershell -File tools/check-version.ps1 -Mod PipelineColor`
4. **Docs** — `CHANGELOG.md`, root `README.md` card, `PipelineColor/README.md`, evergreen `Documentation/*`. Screenshots under `PipelineColor/Screenshots/` with raw `refs/heads/main` URLs in README.
5. **Alpakit Release** — `powershell -File tools/build-mod.ps1 -Mod PipelineColor -Mode Release`. Output under StarterProject `Saved/ArchivedPlugins/PipelineColor/`.
6. **ficsit.app** → New Version → upload combined zip + changelog from `CHANGELOG.md` **1.2.1**. Refresh mod icon from `Resources/Icon512.png` if asked. Merge to `main` before announce so GitHub README/screenshots align.
7. **Verify** — Mod Manager installs **1.2.1**; client and dedicated share `^1.2.0`.
8. **Smoke test** — `!pc Water gas` RGB only; `!Metallic` independent; `!pc default` does not clear metallic cfg; fill/empty pipes; Space Elevator on Linux dedi; save roundtrip.
9. **Mod-compat smoke** — vanilla profile: no SatisfactoryPlus/RefinedPower Customizer sections, `mod soft-available stems=0`, no absent-mod Warnings, store seeds vanilla keys only. SFP+RP profile: both sections appear with their swatches, modded fluid fills paint pipes, SFP does not strip PC recipes (`ModContentRegistry recipes=` in log), gas defaults metallic for modded gases.

## Dev pack (not SMR)

```powershell
powershell -File tools/build-mod.ps1 -Mod PipelineColor
```

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically.
