# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `StructuralPower.zip` | Alpakit **Release** output (all targets in one file) | Upload once to SMR |
| `StructuralPower-LinuxServer.zip` etc. | Per-target slices | Local debugging; not uploaded separately |
| `StructuralPower-*.smod` | SMR-hosted download for a target | Players / servers via Mod Manager or ficsit-cli |

Upload the combined `StructuralPower.zip`. SMR unpacks it and serves per-platform `.smod` downloads.

Mod page: [ficsit.app/mod/StructuralPower](https://ficsit.app/mod/StructuralPower)

## Release checklist (3.1.0)

1. **Version fields** in `StructuralPower.uplugin` — `SemVersion` / `VersionName` **3.1.0**, `RemoteVersionRange` **^3.0.0**, `Version` integer **3** (major), `GameVersion` **>=491125**, SML **^3.12.0**.
2. **Icons** — `tools/Invoke-ModIcons.ps1` (also from pack scripts) → badge **V3.1** on `Resources/Icon*.png`. Commit with the version bump.
3. **Version guard** — `powershell -File scripts/check-version.ps1`
4. **Docs** — `CHANGELOG.md`, root `README.md`, `StructuralPower/README.md`, `Documentation/*`.
5. **Alpakit Release** — client + Windows Server + Linux Server:
   ```
   StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip
   ```
6. **ficsit.app** → New Version → upload `StructuralPower.zip` + changelog **3.1.0**. Refresh mod icon from `Resources/Icon512.png` if asked. Merge `development` → `main` so GitHub README/screenshots match.
7. **Verify** — Mod Manager installs **3.1.0**; client and dedicated share the same mod set.
8. **Smoke test** — load save; machine/pipe groups; lights/switches/panels; Id panel; hoverpack; save/reload.

## Updating a release

1. Bump `SemVersion` in `.uplugin`.
2. Refresh icons (`Invoke-ModIcons.ps1` or pack scripts).
3. Alpakit Release → new `StructuralPower.zip`.
4. New Version on ficsit.app.

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli.
