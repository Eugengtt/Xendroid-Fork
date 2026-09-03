/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/gpu/d3d12/dxc_compiler.h"

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/ui/d3d12/d3d12_provider.h"

DEFINE_bool(dxil_debug, false,
            "Compile guest DXIL shaders unoptimized and with debug info for "
            "source-level shader debugging (e.g. in RenderDoc). When off "
            "(default), shaders are optimized.",
            "D3D12");

namespace xe {
namespace gpu {
namespace d3d12 {

DxcCompiler::DxcCompiler(const ui::d3d12::D3D12Provider& provider)
    : provider_(provider) {}

DxcCompiler::~DxcCompiler() {
  if (utils_) {
    utils_->Release();
    utils_ = nullptr;
  }
  if (compiler_) {
    compiler_->Release();
    compiler_ = nullptr;
  }
}

bool DxcCompiler::Initialize() {
  // Create the DXC compiler interface.
  // IDxcCompiler3 requires DXC version 1.5+ (released ~2020).
  HRESULT hr =
      provider_.DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
  if (FAILED(hr)) {
    XELOGE(
        "DxcCompiler: Failed to create IDxcCompiler3 (HRESULT {:08X}) - "
        "dxcompiler.dll may be too old, need version 1.5+. Download latest "
        "from https://github.com/microsoft/DirectXShaderCompiler/releases",
        static_cast<unsigned int>(hr));
    return false;
  }

  // Create the DXC utils interface for blob management.
  hr = provider_.DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
  if (FAILED(hr)) {
    XELOGE("DxcCompiler: Failed to create IDxcUtils instance (HRESULT {:08X})",
           static_cast<unsigned int>(hr));
    compiler_->Release();
    compiler_ = nullptr;
    return false;
  }

  XELOGI("DxcCompiler: DXIL shader compilation available");
  return true;
}

bool DxcCompiler::Compile(const std::string& hlsl_source,
                          const std::string& entry_point,
                          const std::string& target,
                          std::vector<uint8_t>& dxil_out,
                          std::string* error_message) {
  if (!IsAvailable()) {
    if (error_message) {
      *error_message = "DXC compiler not available";
    }
    return false;
  }

  // Convert entry point and target to wide strings.
  std::wstring entry_point_wide(entry_point.begin(), entry_point.end());
  std::wstring target_wide(target.begin(), target.end());

  // Set up compilation arguments.
  std::vector<LPCWSTR> arguments = {
      L"-E",
      entry_point_wide.c_str(),
      L"-T",
      target_wide.c_str(),
      // Flush float32 denormals.
      L"-denorm",
      L"ftz",
      DXC_ARG_WARNINGS_ARE_ERRORS,
  };
  if (cvars::dxil_debug) {
    // Unoptimized with debug info for source-level debugging in RenderDoc.
    arguments.push_back(L"-Od");
    arguments.push_back(L"-Zi");
  } else {
    // Optimized (DXC defaults to -O3). Force IEEE strictness. DXC fast-math
    // reassociation miscompiles guest shaders that rely on precise float
    // semantics. Strip reflection metadata to shrink the DXIL blob. The runtime
    // uses translator-gathered bindings, not reflection.
    arguments.push_back(L"-Gis");
    arguments.push_back(L"-Qstrip_reflect");
  }

  // Create source buffer.
  DxcBuffer source_buffer;
  source_buffer.Ptr = hlsl_source.c_str();
  source_buffer.Size = hlsl_source.size();
  source_buffer.Encoding = DXC_CP_UTF8;

  // Compile the shader.
  IDxcResult* result = nullptr;
  HRESULT hr = compiler_->Compile(&source_buffer, arguments.data(),
                                  static_cast<UINT32>(arguments.size()),
                                  nullptr,  // No include handler
                                  IID_PPV_ARGS(&result));

  if (FAILED(hr)) {
    if (error_message) {
      *error_message = "DXC Compile call failed";
    }
    return false;
  }

  // Check compilation status.
  HRESULT status;
  result->GetStatus(&status);

  if (FAILED(status)) {
    // Retrieve error messages.
    IDxcBlobUtf8* errors = nullptr;
    if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors),
                                    nullptr)) &&
        errors && errors->GetStringLength() > 0) {
      if (error_message) {
        *error_message = errors->GetStringPointer();
      }
      errors->Release();
    } else {
      if (error_message) {
        *error_message = "Unknown compilation error";
      }
    }
    result->Release();
    return false;
  }

  // Retrieve the compiled shader object.
  IDxcBlob* shader_blob = nullptr;
  if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_blob),
                               nullptr)) ||
      !shader_blob) {
    if (error_message) {
      *error_message = "Failed to retrieve compiled shader";
    }
    result->Release();
    return false;
  }

  // Copy the DXIL bytecode to the output vector.
  const uint8_t* shader_data =
      static_cast<const uint8_t*>(shader_blob->GetBufferPointer());
  size_t shader_size = shader_blob->GetBufferSize();
  dxil_out.assign(shader_data, shader_data + shader_size);

  shader_blob->Release();
  result->Release();

  return true;
}

bool DxcCompiler::Disassemble(const std::vector<uint8_t>& dxil,
                              std::string& disassembly_out) {
  if (!IsAvailable()) {
    return false;
  }

  DxcBuffer dxil_buffer;
  dxil_buffer.Ptr = dxil.data();
  dxil_buffer.Size = dxil.size();
  dxil_buffer.Encoding = DXC_CP_ACP;  // Binary data

  IDxcResult* result = nullptr;
  HRESULT hr = compiler_->Disassemble(&dxil_buffer, IID_PPV_ARGS(&result));

  if (FAILED(hr)) {
    return false;
  }

  HRESULT status;
  result->GetStatus(&status);

  if (FAILED(status)) {
    result->Release();
    return false;
  }

  IDxcBlobUtf8* disasm_blob = nullptr;
  if (FAILED(result->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&disasm_blob),
                               nullptr)) ||
      !disasm_blob) {
    result->Release();
    return false;
  }

  disassembly_out = disasm_blob->GetStringPointer();

  disasm_blob->Release();
  result->Release();

  return true;
}

}  // namespace d3d12
}  // namespace gpu
}  // namespace xe
