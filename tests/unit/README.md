# Unit tests

vqec_vision_inference_plan_test.cpp covers structural plan validation: valid UINT8/FLOAT32,
4K NV12 byte count, invalid geometry/FPS, explicit placement, NaN/zero coefficients,
unsupported quantization/type, invalid paths and queue budgets.

Source added; not compiled or executed in the current turn.
Tests do not prove plugin negotiation, SDK compatibility, golden parity or DMA safety.
