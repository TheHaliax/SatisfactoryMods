# Satisfactory Mods

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Source for multiple [Satisfactory](https://www.satisfactorygame.com/) mods in one git repository.

Each mod is a top-level folder named after its plugin (`ModReference`). Inside: `.uplugin`, C++ `Source/`, and any `Config/` or `Resources/` shipped with the mod. Build artifacts are local only and not tracked in git.

Use `Link-Mods.ps1` to junction mod folders into your SML StarterProject `Mods/` directory before building with Alpakit or UBT.

## Mods

### [StructuralPower](StructuralPower/)

| | |
|---|---|
| **Added** | 2026-06-30 |
| **Published** | 2026-07-02 |
| **Version** | 1.0.0 |
| **Status** | Released |

Hidden structural power bus for foundations, walls, and bridge poles. Applies to new placements only — no retroactive scan of existing builds. Graph state persists through the mod subsystem on save/load.

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.

---

## License

Source code is [GPL-3.0-or-later](LICENSE). See `SPDX-License-Identifier` headers in source files.

Copyright © 2026 Haliax.
