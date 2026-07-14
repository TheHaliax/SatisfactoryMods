# Release (SMR)

Players and dedicated servers install **`.smod` files from [ficsit.app](https://ficsit.app)** — not zips from git.

## Internal test deploy (Haliax)

Test hosts run **our Alpakit release zips**, not SMR pulls (unless you later wire `SAT_STRUCTURAL_POWER_VERSION`).

1. `powershell -File tools/pack-structuralpower-release.ps1`
2. **Client:** `StructuralPower-Windows.zip` → Steam `FactoryGame/Mods/StructuralPower`
3. **Linux `10.0.0.11:7256`:** `scp` `StructuralPower-LinuxServer.zip` → `STRUCTURAL_POWER_ZIP=~/sat-server/StructuralPower-LinuxServer.zip` + `deploy-test-mods.sh` + `systemctl restart sat-server-test`
4. **Windows `207.244.250.178:7777`:** FTP `StructuralPower-WindowsServer.zip` → `FactoryGame/Mods/StructuralPower` → restart via hosting panel

SML + UtilityMod on laptop script still download from SMR API; only StructuralPower uses the zip override today.

## Zip vs smod

| Artifact | What it is | Who uses it |
|----------|------------|-------------|
| `StructuralPower.zip` | Alpakit **Release** output (all targets in one file) | **You upload this once** to SMR |
| `StructuralPower-LinuxServer.zip` etc. | Alpakit per-target slices | Local debugging only; **not** uploaded separately |
| `StructuralPower-LinuxServer.smod` | SMR-hosted download for a target | Servers and ficsit-cli pull this via API |

Upload the **big** `StructuralPower.zip`. SMR unpacks it and serves per-platform `.smod` downloads.

Mod page: [ficsit.app/mod/StructuralPower](https://ficsit.app/mod/StructuralPower)

## Release checklist (3.0.0)

1. **Version fields** in `StructuralPower.uplugin` — `SemVersion` / `VersionName` **3.0.0**, `RemoteVersionRange` **^3.0.0**, `Version` integer **3** (major), `GameVersion` **>=491125**, SML **^3.12.0**.
2. **Icons** — `python tools/make-icons.py` (reads `.uplugin` → badge **V3.0** on `Icon1024/512/128.png`). Commit `StructuralPower/Resources/`.
3. **Version guard** — `powershell -File scripts/check-version.ps1`
4. **Docs** — `CHANGELOG.md`, root `README.md` (version table), `StructuralPower/README.md`, evergreen `Documentation/*` (player-guide, commands, troubleshooting). Icons in same commit as version bump.
5. **Alpakit Release** — cook client + Windows Server + Linux Server. Output:
   ```
   StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip
   ```
6. **ficsit.app** → StructuralPower → **New Version** → upload `StructuralPower.zip`, changelog from `CHANGELOG.md` **3.0.0** (product welcome-back prose lives in `README.md`, not the changelog), compatibility **Works** (tested 495413 stable SP). Upload refreshed **mod icon** from `Resources/Icon512.png` if the SMR form asks. Merge `development` → `main` before announce so GitHub README/icon align. Gameplay screenshot in `StructuralPower/README.md` uses `refs/heads/main/.../Screenshots/...` — updates on `main` only.
7. **Deploy test servers** — follow [Internal test deploy](#internal-test-deploy-haliax) above (our zips). Optional SMR parity: copy version ID after approval if you want laptop script to `fetch_smr` StructuralPower instead of `STRUCTURAL_POWER_ZIP`:
   ```powershell
   curl -sL "https://api.ficsit.app/v1/mod/5W9LAp72Xzuwjn/versions"
   ```
   Store as env `SAT_STRUCTURAL_POWER_VERSION=<id>` for laptop deploy scripts if used. Not required for routine testing.
8. **Verify** — Mod Manager / ficsit profile installs StructuralPower **3.0.0**; join test server with same mod set.
9. **Smoke test** — load legacy save (pre-v3.0); lights/switches/panels; **I** key; `!lighting`; save/reload; switch subnet ON/OFF; hoverpack tether.

## Updating a release

1. Bump `SemVersion` in `.uplugin`.
2. `python tools/make-icons.py` — refresh `Resources/Icon*.png`.
3. Alpakit Release → new `StructuralPower.zip`.
4. New Version on ficsit.app.
5. Re-run [internal test deploy](#internal-test-deploy-haliax) to all three targets.
6. Optionally update `SAT_STRUCTURAL_POWER_VERSION` if using SMR-pull laptop deploy.

## Players

Install via [Satisfactory Mod Manager](https://ficsit.app) or ficsit-cli — they download `.smod` automatically. No manual zip handling.
