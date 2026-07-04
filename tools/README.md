# Build & release tooling

Runnable tooling for building, packaging, and shipping the mods in this repo. Only the files
below are tracked; the rest of `tools/` is host-specific scratch and stays gitignored.

| Script | Purpose |
|--------|---------|
| `pack-structuralpower.ps1` | Iterative dev build of the runtime module + deploy to a local install |
| `pack-structuralpower-release.ps1` | Alpakit Release cook → SMR upload zip (client + Win/Linux server) |
| `make-icons.py` | Generate `Icon1024/512/128.png` from a layered control mask |
| `../scripts/check-version.ps1` | Pre-release guard: uplugin ↔ changelog version drift |

Machine paths (engine root, StarterProject, Steam install) are script parameters with defaults
for the author's layout — override them for yours. Defaults:

- `ProjectPath` = `E:\Modding\Satisfactory\StarterProject\FactoryGame.uproject`
- `UeCssRoot` = `C:\Program` (Unreal CSS engine root)
- `GamePath` = `E:\SteamLibrary\steamapps\common\Satisfactory`

## Iterative development

Compile the runtime module and drop the DLL into the local game. Incremental by default
(seconds); pass `-Clean` for a full `FactoryGameSteam` rebuild.

```powershell
powershell -File tools/pack-structuralpower.ps1 -Config Shipping
```

The mod is resolved as the sibling `../StructuralPower` when the script runs from `tools/`, so
no path juggling is needed inside the repo.

## Release (SMR)

1. Bump `Version` **and** `SemVersion` together in `StructuralPower/StructuralPower.uplugin`
   (ficsit.app requires the SemVer major to match `Version`), and update `CHANGELOG.md`.
2. Verify no version drift:

   ```powershell
   powershell -File scripts/check-version.ps1
   ```

3. Cook the upload zip:

   ```powershell
   powershell -File tools/pack-structuralpower-release.ps1
   ```

   Output: `StarterProject/Saved/ArchivedPlugins/StructuralPower/StructuralPower.zip`.
4. Upload that single zip on [ficsit.app](https://ficsit.app/mod/StructuralPower) → New Version.
   SMR unpacks it and serves per-platform `.smod` downloads.

See `StructuralPower/Documentation/release.md` for the full checklist and server-deploy steps.

## Icons

`make-icons.py` renders the three mod icons from one **control mask** registered to the base art,
so the icon never drifts from the shipped version.

```powershell
pip install -r tools/requirements.txt
python tools/make-icons.py
```

Inputs (defaults resolve inside the repo):

- `BaseIcon1024.png` — 1024² background.
- `Mask1024.png` — pure-RGB control map: **blue** = lineart, **red** = mod-name box,
  **green** = version box (boxes are control-only, never drawn).
- Font: Unispace Bold (`--font`); name from the uplugin `FriendlyName`, version `V<major.minor>`.

Rendering: the silhouette (lineart ∪ name ∪ version glyphs) is filled with a vertical gradient
(`--fill-top`/`--fill-bottom`), given a rounded outline in the **inverted** gradient for contrast,
and a duotone split shadow (`--shadow-*`). Composited at 1024, then Lanczos-downscaled to 512/128
and written to `StructuralPower/Resources/`. Tweak the look via the `--fill-*`, `--outline-px`, and
`--shadow-*` flags without touching the art.

## Why the release script builds the editor module first

Alpakit Release cooks through `UnrealEditor-Cmd`, which must load the plugin's **editor**
module. A `-Module=` build doesn't emit the `.modules` manifest the loader needs, and a prior
`git clean` wipes untracked plugin binaries. The release script rebuilds the editor module and
rewrites its manifest (with the project's `BuildId`) before cooking — otherwise the cook fails
with `module 'StructuralPower' could not be found`.
