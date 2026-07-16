# Contributing (SatisfactoryMods)

## C++ style

- **clang-format:** [`.clang-format`](.clang-format) — Google base, NASA Power of 10
  overrides for short-form control flow (`AllowShort* = Never/None`, column 100).
  `SortIncludes: Never` — UE requires `*.generated.h` as the **last** include.
- Run: `powershell -File tools/clang-format-first-party.ps1`
- CI: `.github/workflows/clang-format.yml` fails PRs that drift.

## NASA Power of 10 (review / lint — not auto-rewritten)

clang-format cannot enforce these; keep them in reviews:

1. No `goto` / `setjmp` / `longjmp` in game-mod code.
2. Prefer non-recursive algorithms on hot paths; document any intentional recursion.
3. Check return values / validity (`IsValid`, `INDEX_NONE`, false).
4. Minimize variable scope; one job per function; keep functions readable/short.
5. Prefer simple control flow over clever nesting.
6. Compile clean under project warning levels; fix what you touch.
7. Limit preprocessor tricks; prefer constexpr / typed helpers.
8. UE heap/GC is normal — do not invent a free-store ban; avoid unbounded alloc on
   per-frame/remove hot paths (prefer scoped sets, deferred coalesce).

## First-party modules

`StructuralPower`, `PipelineColor`. Do not format Engine / third-party trees.
