# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `PipelineColor.zip` | Alpakit **Release** output (all targets) | Upload once to SMR |
| `PipelineColor-Windows.zip` etc. | Per-target slices | Local debugging |
| `PipelineColor-*.smod` | SMR-hosted per target | Players / ficsit-cli |

## Release checklist (1.3.1 — 2026-08-13)

1. **Version fields** in `PipelineColor.uplugin` — `SemVersion` / `VersionName` **1.3.1**, `RemoteVersionRange` **^1.3.0**, `Version` integer **1**, `GameVersion` **>=491125**, SML **^3.12.0**, `IsBetaVersion` **false**.
2. **Icons** — `powershell -File tools/icons.ps1 PipelineColor` → badge **V1.3** on `Resources/Icon*.png` (badge tracks major.minor).
3. **Version guard** — `powershell -File tools/check-version.ps1 -Mod PipelineColor`
4. **Docs** — `CHANGELOG.md`, root `README.md` card, `PipelineColor/README.md`, evergreen `Documentation/*`. Screenshots under `PipelineColor/Screenshots/` with raw `refs/heads/main` URLs in README.
5. **Alpakit Release** — `powershell -File tools/build-mod.ps1 -Mod PipelineColor -Mode Release`. Output under StarterProject `Saved/ArchivedPlugins/PipelineColor/`.
6. **ficsit.app** → New Version → upload combined zip + changelog from `CHANGELOG.md` **1.3.1**. Refresh mod icon from `Resources/Icon512.png` if asked. Merge to `main` before announce so GitHub README/screenshots align.
7. **Verify** — Mod Manager installs **1.3.1**; client and dedicated share `^1.3.0`.
8. **Smoke test** — Customizer icons colored (catalog fill); `!pc Water gas` RGB only; `!Metallic` independent; `!pc default` clears customs only (not metallic cfg); fill/empty pipes; Space Elevator on Linux dedi; save roundtrip after schema-4 nuke.
9. **Mod-compat smoke** — vanilla profile: Default section + Neutral/Fallback; no Refined Power / SFP subcategory if those mods absent. With RP/SFP: `dynamic swatches by owner:` lists them; modded fluids paint; SFP does not strip PC recipes (`ModContentRegistry recipes=` in log); gases metallic by default.

## Dev pack (not SMR)

```powershell
powershell -File tools/build-mod.ps1 -Mod PipelineColor
```

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically.
