# Content / Swatches

v1.0 ships C++ swatch classes for the Customizer **PipelineColor** category. Optional cooked Blueprint mirrors are not required.

Metallic sheen is applied with PaintFinish (+ cfg / chat overrides), not by stuffing white into Secondary RGB.

To add editor-visible Customizer polish later:

1. Create Blueprint subclasses of each `UPCSwatchDesc_*` here.
2. Register them in a customization collection if needed.
3. Point the appearance catalog at the Blueprint classes instead of the native defaults.
