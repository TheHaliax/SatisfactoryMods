# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `StructuralPower.zip` | Alpakit **Release** output (all targets in one file) | **You upload this once** to SMR |
| `StructuralPower-LinuxServer.zip` etc. | Alpakit per-target slices | Local debugging only; **not** uploaded separately |
| `StructuralPower-LinuxServer.smod` | SMR-hosted download for a target | Servers and ficsit-cli pull this via API |

Upload the **big** `StructuralPower.zip`. SMR unpacks it and serves per-platform `.smod` downloads.

Mod page: [ficsit.app/mod/StructuralPower](https://ficsit.app/mod/StructuralPower)

## Release checklist

1. **Version fields** in `StructuralPower.uplugin` — bump `Version` and `SemVersion` together (ficsit.app requires the SemVer major to match `Version`), and confirm `GameVersion` and the SML dependency.
2. **Alpakit Release** — cook client + Windows Server + Linux Server. Output:
   ```
   StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip
   ```
3. **ficsit.app** → StructuralPower → **New Version** → upload `StructuralPower.zip`, changelog, compatibility **Works** (stable + experimental if tested). Mod page body uses root `README.md` — keep screenshot URLs as `raw.githubusercontent.com/.../main/StructuralPower/Screenshots/...` (not relative paths).
4. **Copy version ID** after approval (needed for server deploy scripts):
   ```powershell
   curl -sL "https://api.ficsit.app/v1/mod/5W9LAp72Xzuwjn/versions"
   ```
   Use the `id` field for version `1.0.0` (e.g. `abc123xyz`).
5. **Local deploy config** — in repo `tools/mod-versions.env`:
   ```
   SAT_STRUCTURAL_POWER_VERSION=<id from step 4>
   ```
6. **Deploy test servers** (all mods from SMR `.smod`):
   ```bash
   ~/sat-server/scripts/deploy-test-mods.sh
   sudo systemctl restart sat-server-test
   ```
   ```powershell
   powershell -File tools/windows-dedicated-server/deploy-remote-mods.ps1
   ```
7. **Verify** — Mod Manager / ficsit profile installs StructuralPower 1.0.0; join test server with same mod set.

## Updating a release

1. Bump `SemVersion` in `.uplugin`.
2. Alpakit Release → new `StructuralPower.zip`.
3. New Version on ficsit.app.
4. Update `SAT_STRUCTURAL_POWER_VERSION` in `tools/mod-versions.env`.
5. Re-run deploy scripts.

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically. No manual zip handling.
