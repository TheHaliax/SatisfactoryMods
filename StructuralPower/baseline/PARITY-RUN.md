# Parity run checklist — modular pipeline refactor

Baseline tag: `sp-parity-baseline-aea9f1e` @ commit `aea9f1e`

## Automated

Run StarterProject editor automation filter `StructuralPower.*` after cook of StructuralPower + StructuralPowerTests.

## Manual matrix (VERIFICATION-GATE.md)

- Structure bus + pole mesh + wire promote
- Switch 0-wire / 1-wire / 2-wire + toggle + Control id
- GroupLighting on/off + panel keyed subnet + SearchDownstreamCircuit release
- Bulk load WT save
- Hoverpack structural tether
- Dedicated: `207.244.250.178:7777` + laptop `10.0.0.11:7256`

## Perf

`compare-halsp-perf-deltas.ps1` vs `StructuralPower/baseline/` capture after smoke run.
