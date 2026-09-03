// XE_NO_MSL
// No-op pixel shader, compiled for DXIL and SPIR-V. Two uses:
//   - Async pipeline hot-swap placeholder, bound while the real pixel shader
//     compiles in the background so the draw does not have to be skipped.
//   - Pixel-shader-less depth-only draws, where depth still needs to be written.
// Writes no color output, so it never modifies the color render targets.

void main() {}
