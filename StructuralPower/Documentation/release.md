# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Internal test deploy (Haliax)

Test hosts run **our Alpakit release zips**, not SMR pulls (unless you later wire `SAT_STRUCTURAL_POWER_VERSION`).

1. `powershell -File tools/pack-structuralpower-release.ps1`
2. **Client:** `StructuralPower-Windows.zip` → Steam `FactoryGame/Mods/StructuralPower`
3. **Linux `10.0.0.11:7256`:** `scp` `StructuralPower-LinuxServer.zip` → `STRUCTURAL_POWER_ZIP=~/sat-server/StructuralPower-LinuxServer.zip` + `deploy-test-mods.sh` + `systemctl restart sat-server-test`
4. **Windows `207.244.250.178:7777`:** FTP `StructuralPower-WindowsServer.zip` → `FactoryGame/Mods/StructuralPower` (local gitignored `tools/windows-dedicated-server/`) → restart via hosting panel

SML + UtilityMod on laptop script still download from SMR API; only StructuralPower uses the zip override today.

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
3. **ficsit.app** → StructuralPower → **New Version** → upload `StructuralPower.zip`, changelog (from `CHANGELOG.md`), compatibility **Works** (stable + experimental if tested). Mod page body uses root `README.md` — keep screenshot URLs as `raw.githubusercontent.com/.../main/StructuralPower/Screenshots/...` (not relative paths).
4. **Deploy test servers** — follow [Internal test deploy](#internal-test-deploy-haliax) above (our zips). Optional SMR parity: copy version ID after approval if you want laptop script to `fetch_smr` StructuralPower instead of `STRUCTURAL_POWER_ZIP`:
   ```powershell
   curl -sL "https://api.ficsit.app/v1/mod/5W9LAp72Xzuwjn/versions"
   ```
   Store in gitignored `tools/mod-versions.env`: `SAT_STRUCTURAL_POWER_VERSION=<id>`. Not required for routine testing.
5. **Verify** — Mod Manager / ficsit profile installs StructuralPower 2.1.0; join test server with same mod set.

## Updating a release

1. Bump `SemVersion` in `.uplugin`.
2. Alpakit Release → new `StructuralPower.zip`.
3. New Version on ficsit.app.
4. Re-run [internal test deploy](#internal-test-deploy-haliax) to all three targets.
5. Optionally update `SAT_STRUCTURAL_POWER_VERSION` if using SMR-pull laptop deploy.

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically. No manual zip handling.
