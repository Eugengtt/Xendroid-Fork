/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_REDIST_INSTALLER_WX_H_
#define XENIA_UI_REDIST_INSTALLER_WX_H_

#include <filesystem>

namespace xe {
namespace ui {

// Runtime-dependency installer (prompt + download + extract). Currently the
// Windows-only Direct3D 12 DLLs; on other platforms the entry points are
// no-ops.

// If dxcompiler.dll + dxil.dll aren't loadable from d3d12_dir or the system,
// prompts once and downloads them into d3d12_dir (loaded in-process, no
// restart). Returns true if present already or after a successful install.
bool EnsureShaderCompilerRuntime(const std::filesystem::path& d3d12_dir);

// Called when the in-box runtime lacks Shader Model 6.6 and Agility isn't
// active. Prompts once, downloads D3D12Core.dll into d3d12_dir, and restarts
// (the opt-in must precede device creation, so it can't apply in-process). On
// success it relaunches and never returns, otherwise returns false.
bool EnsureAgilityRuntime(const std::filesystem::path& d3d12_dir);

// Called when --d3d12_debug is set but the debug layer (D3D12SDKLayers.dll)
// couldn't be enabled. Fetches it from the Agility SDK (with D3D12Core.dll,
// since it only loads under the Agility runtime) into d3d12_dir and restarts.
// Same return contract as EnsureAgilityRuntime.
bool EnsureDebugLayer(const std::filesystem::path& d3d12_dir);

// If the Vulkan loader (vulkan-1.dll) isn't loadable from vulkan_dir or the
// system, prompts once and downloads it into vulkan_dir (loaded in-process, no
// restart). Returns true if present already or after a successful install. The
// loader still needs a GPU Vulkan driver behind it. This supplies only the
// loader, not a device.
bool EnsureVulkanLoader(const std::filesystem::path& vulkan_dir);

// If the Microsoft Visual C++ runtime isn't installed (a missing/outdated one
// crashes deep in the CRT during startup), prompts once, downloads
// vc_redist from Microsoft, verifies it carries a Microsoft Authenticode
// signature, runs it elevated, and restarts. Returns true if already present;
// on a successful install it relaunches and never returns; on decline or any
// failure it returns false and startup should continue.
bool EnsureVCRuntime();

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_REDIST_INSTALLER_WX_H_
