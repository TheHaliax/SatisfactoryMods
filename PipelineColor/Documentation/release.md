# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `PipelineColor.zip` | Alpakit **Release** output (all targets) | Upload once to SMR |
| `PipelineColor-Windows.zip` etc. | Per-target slices | Local debugging |
| `PipelineColor-*.smod` | SMR-hosted per target | Players / ficsit-cli |

## Release checklist (1.0.0)

1. **Version fields** in `PipelineColor.uplugin` — `SemVersion` / `VersionName` **1.0.0**, `RemoteVersionRange` **^1.0.0**, `Version` integer **1**, `GameVersion` **>=491125**, SML **^3.12.0**, `IsBetaVersion` **false**.
2. **Icons** — `tools/Invoke-ModIcons.ps1 -ModRoot PipelineColor` → badge **V1.0** on `Resources/Icon*.png`.
3. **Version guard** — `powershell -File scripts/check-version.ps1`
4. **Docs** — `CHANGELOG.md`, root `README.md` card, `PipelineColor/README.md`, evergreen `Documentation/*`. Screenshots under `PipelineColor/Screenshots/` with raw `refs/heads/main` URLs in README.
5. **Alpakit Release** — cook client + Windows Server + Linux Server. Output under StarterProject `Saved/ArchivedPlugins/PipelineColor/`.
6. **ficsit.app** → New Version → upload combined zip + changelog from `CHANGELOG.md` **1.0.0**. Refresh mod icon from `Resources/Icon512.png` if asked. Merge to `main` before announce so GitHub README/screenshots align.
7. **Verify** — Mod Manager installs **1.0.0**; client and dedicated share `^1.0.0`.
8. **Smoke test** — fill/empty pipes; Customizer PC swatches + save/reload; supports against painted pipes; `!Metallic` / `all on` / `all off` / `default` / `!pchelp`; gas metallic, liquid default; chromatics stay distinct under metallic.

## Dev pack (not SMR)

```powershell
powershell -File tools/pack-pipelinecolor.ps1 -Config Shipping
```

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically.
