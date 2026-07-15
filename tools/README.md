# Build & release tooling

Runnable tooling for building, packaging, and shipping the mods in this repo.

| Script | Purpose |
|--------|---------|
| `pack-structuralpower.ps1` | Iterative SP runtime build + deploy (icons auto) |
| `pack-pipelinecolor.ps1` | Iterative PipelineColor runtime build + deploy (icons auto) |
| `pack-structuralpower-release.ps1` | Alpakit Release cook → SMR zip (icons auto before cook) |
| `Invoke-ModIcons.ps1` | Shared wrapper: regen `Icon1024/512/128.png` for a mod |
| `make-icons.py` | Icon compositor (base + mask + font + uplugin badge) |
| `../scripts/check-version.ps1` | Pre-release guard: uplugin ↔ changelog version drift |

Machine paths (engine root, StarterProject, Steam install) are script parameters with defaults
for the author's layout — override them for yours. Defaults:

- `ProjectPath` = `E:\Modding\Satisfactory\StarterProject\FactoryGame.uproject`
- `UeCssRoot` = `C:\Program` (Unreal CSS engine root)
- `GamePath` = `E:\SteamLibrary\steamapps\common\Satisfactory`

## Iterative development

Compile the runtime module and drop the DLL into the local game. Incremental by default
(seconds); pass `-Clean` for a full `FactoryGameSteam` rebuild. Icons regenerate from
`.uplugin` SemVersion before Resources copy (pass `-SkipIcons` to skip).

```powershell
powershell -File tools/pack-structuralpower.ps1 -Config Shipping
powershell -File tools/pack-pipelinecolor.ps1 -Config Shipping
```

The mod is resolved as the sibling `../<ModName>` when the script runs from `tools/`, so
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

Bundled assets (do not swap palettes casually — defaults are the locked look):

| File | Role |
|------|------|
| `BaseIcon1024.png` | Background |
| `Mask1024.png` | Blue lineart · red name box · green version box |
| `UnispaceBd.ttf` | Badge font (copy of Windows Unispace Bold) |

```powershell
pip install -r tools/requirements.txt   # once
powershell -File tools/Invoke-ModIcons.ps1 -ModRoot StructuralPower
# or:
python tools/make-icons.py --uplugin PipelineColor/PipelineColor.uplugin --out-dir PipelineColor/Resources
```

`FriendlyName` + `SemVersion` → badge **V\<major.minor\>**. Fill/shadow CLI flags exist for
experiments; pack scripts never pass them — script defaults only.

## Why the release script builds the editor module first

Alpakit Release cooks through `UnrealEditor-Cmd`, which must load the plugin's **editor**
module. A `-Module=` build doesn't emit the `.modules` manifest the loader needs, and a prior
`git clean` wipes untracked plugin binaries. The release script rebuilds the editor module and
rewrites its manifest (with the project's `BuildId`) before cooking — otherwise the cook fails
with `module 'StructuralPower' could not be found`.
