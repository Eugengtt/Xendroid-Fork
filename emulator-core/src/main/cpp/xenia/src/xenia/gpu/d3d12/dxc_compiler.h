/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_D3D12_DXC_COMPILER_H_
#define XENIA_GPU_D3D12_DXC_COMPILER_H_

#include <string>
#include <vector>

#include "xenia/ui/d3d12/d3d12_api.h"

namespace xe {
namespace ui {
namespace d3d12 {
class D3D12Provider;
}  // namespace d3d12
}  // namespace ui

namespace gpu {
namespace d3d12 {

// Wrapper around the DirectX Shader Compiler (DXC) for compiling HLSL to DXIL.
// This enables SM 6.x features such as barycentric interpolation.
class DxcCompiler {
 public:
  explicit DxcCompiler(const ui::d3d12::D3D12Provider& provider);
  ~DxcCompiler();

  // Initializes the DXC compiler interfaces. Returns true on success.
  bool Initialize();

  // Returns true if DXC compilation is available.
  bool IsAvailable() const { return compiler_ != nullptr && utils_ != nullptr; }

  // Compile HLSL source code to DXIL bytecode.
  // target: Shader target profile, e.g., "vs_6_0", "ps_6_0", "gs_6_0", "ds_6_0"
  // Returns true on success, with DXIL bytecode in dxil_out.
  // On failure, error_message (if provided) contains the error description.
  bool Compile(const std::string& hlsl_source, const std::string& entry_point,
               const std::string& target, std::vector<uint8_t>& dxil_out,
               std::string* error_message = nullptr);

  // Get disassembly of DXIL bytecode for debugging.
  bool Disassemble(const std::vector<uint8_t>& dxil,
                   std::string& disassembly_out);

 private:
  const ui::d3d12::D3D12Provider& provider_;
  IDxcCompiler3* compiler_ = nullptr;
  IDxcUtils* utils_ = nullptr;
};

}  // namespace d3d12
}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_D3D12_DXC_COMPILER_H_
