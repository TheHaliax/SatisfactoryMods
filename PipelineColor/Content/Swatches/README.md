# Content / Swatches

Native C++ swatch / recipe classes: **PC Empty Pipe**, **PC ERR0R**, plus ClassGen
`PCSwatch_*` / `PCRecipe_*` per discovered fluid (`FPCDynamicSwatchRegistry`). Optional cooked
Blueprint mirrors are not required.

Metallic sheen is PaintFinish (+ cfg / chat overrides), not Secondary RGB white.

To add editor-visible Customizer polish later:

1. Create Blueprint subclasses of `UPCSwatchDescBase` / Neutral / Fallback here.
2. Register them in a customization collection if needed.
3. Point publisher / organization at those Blueprint classes instead of native defaults.
