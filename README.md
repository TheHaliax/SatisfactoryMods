# Satisfactory Mods

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Open-source mods for [Satisfactory](https://www.satisfactorygame.com/). Install released builds from [ficsit.app](https://ficsit.app) — not from this repository.

Each mod has its own folder and README with features, requirements, and roadmap.

## Mods

[![StructuralPower icon](StructuralPower/Resources/Icon128.png)](StructuralPower/)

[StructuralPower](StructuralPower/README.md)

| | |
|---|---|
| **Added** | 2026-06-30 |
| **Published** | 2026-07-02 |
| **Updated** | 2026-08-13 |
| **Previous** | **3.1.1** — SCIM-safe save format |
| **Version** | **3.1.2** *(current)* — recook CL 502094 |
| **In development** | **3.2** — [Belt topology](StructuralPower/README.md#v32--belt-topology-in-development) ([roadmap](StructuralPower/README.md#roadmap)) |

Hidden structural power bus for foundations, walls, ramps, and bridge poles. Retroactive — existing builds are wired on load. **v3.0:** vanilla-first rewrite, lighting, Id panel, switches, hoverpack. **v3.1:** opt-in machines + pipe topology (gen/storage/resources/production, supports→pumps). **v3.2 (in development):** belt topology. Config via cfg / console / `!` chat.

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.

---

[![PipelineColor icon](PipelineColor/Resources/Icon128.png)](PipelineColor/)

[PipelineColor](PipelineColor/README.md)

| | |
|---|---|
| **Added** | 2026-07-15 |
| **Published** | 2026-07-15 |
| **Updated** | 2026-08-13 |
| **Previous** | **1.3.0** — dynamic registry, `OwnerMod_Stem`, schema-4 / cfg-2 |
| **Version** | **1.3.1** *(current)* — rebuild CL 502094 (anniversary vtable / dedi hook) |

Fluid-driven colors for pipelines, junctions, pumps, and matching pipe supports. Dynamic Customizer **PipelineColor** swatches per loaded fluid, SaveGame **custom-only** store, metallic gases by default. **v1.3.1:** cook vs game CL 502094 (fixes dedi `OnFluidDescriptorSet` hook crash). **v1.3.0:** `OwnerMod_Stem` keys, schema-4 store / cfg-2 reseed, `!pc` color source, **PC Empty Pipe** + **PC ERR0R** magenta. Config via cfg / console / chat.

Requires Satisfactory 1.2 (≥491125) and SML ^3.12.0.

---

[![RemoteStandby icon](RemoteStandby/Resources/Icon128.png)](RemoteStandby/)

[RemoteStandby](RemoteStandby/README.md)

| | |
|---|---|
| **Added** | 2026-07-23 |
| **Version** | **0.1.1** *(beta)* |

Press **Z** while looking at a factory to toggle Standby (`SetIsProductionPaused`). Dedicated-safe RCO. Log `[HALRS]`.

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
