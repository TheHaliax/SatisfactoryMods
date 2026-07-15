# Content / Swatches

v1.0 ships C++ `UPCSwatchDesc_*` classes (IDs 200–216). Customizer **PipelineColor** category uses these at runtime; optional cooked BP mirrors are not required.

Metallic sheen is chosen at **apply time** (`IsMetallicForKey` / PaintFinish), not via Secondary RGB white.

To add editor-visible customizer polish later:

1. Create Blueprint subclasses of each `UPCSwatchDesc_*` here.
2. Register them in a `UFGFactoryCustomizationCollection` and inject via game
   state / ContentLib if needed.
3. Point `FPCFluidAppearanceCatalog` at the BP classes instead of native CDOs.
