# Satisfactory Mods

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Source for multiple [Satisfactory](https://www.satisfactorygame.com/) mods in one git repository.

Each mod is a top-level folder named after its plugin (`ModReference`). Inside: `.uplugin`, C++ `Source/`, and any `Config/` or `Resources/` shipped with the mod. Build artifacts are local only and not tracked in git.

## Mods

[![StructuralPower icon](StructuralPower/Resources/Icon128.png)](StructuralPower/)

[StructuralPower](StructuralPower/README.md)

| | |
|---|---|
| **Added** | 2026-06-30 |
| **Published** | 2026-07-02 |
| **Version** | 1.0.0 |
| **Status** | Released |

Hidden structural power bus for foundations, walls, and bridge poles. Applies to new placements only — no retroactive scan of existing builds. Graph state persists through the mod subsystem on save/load.

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.

---

## Generative AI Disclosure

This project uses generative AI tools **only as a minor workflow assistant**. The developer is fully responsible for mod design, logic, execution, and stability — all implementation remains hands-on.

### Permitted use

AI is limited to non-structural, repetitive, or auxiliary tasks:

- **Documentation** — README structure, install guides, changelogs, inline comments
- **JSON / boilerplate** — schema skeletons, asset registries, standard UE/SML config templates
- **Debugging** — compile errors, missing includes, stack trace interpretation
- **Reference** — C++ syntax, UE macros, public modding docs

### Human-in-the-loop

Gameplay features, balance, and performance are human-driven:

1. **Structural & logic sovereignty** — AI is not used to design core architecture, complex gameplay logic, or hook mechanics.
2. **Manual review** — No AI output is merged without review, tailoring, and efficiency checks.
3. **Rigorous testing** — The developer compiles, runs, and debugs in-game; release follows manual verification and compatibility testing.
4. **Safety & integrity** — No official game assets, protected code, or proprietary tools are submitted to public AI models.

---

## License

Source code is [GPL-3.0-or-later](LICENSE). See `SPDX-License-Identifier` headers in source files.

Copyright © 2026 Haliax.
