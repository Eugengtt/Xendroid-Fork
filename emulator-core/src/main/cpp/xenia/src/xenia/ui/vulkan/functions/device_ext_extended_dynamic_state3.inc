// VK_EXT_extended_dynamic_state3 (#456) functions used in Xenia.
// NOT promoted to any Vulkan core version - this is a pure EXT, so the setters
// are always loaded by their EXT name via plain XE_UI_VULKAN_FUNCTION (never the
// promoted macro). See device_ext_extended_dynamic_state.inc for the EDS1
// (#268) counterpart.
//
// Stage 2 of the extended-dynamic-state pipeline-permutation reduction: the
// rasterization sub-feature setters (depth clamp enable, polygon mode) and the
// per-render-target BLEND setters needed to move blend enable / equation / color
// write mask out of the baked pipeline key into dynamic state. The logic-op
// setter (vkCmdSetLogicOpEnableEXT) is intentionally omitted: there is no Xenos
// register source for logic op (RB_COLORCONTROL has no logicop field), the baked
// logicOpEnable always stays VK_FALSE, so making it dynamic yields zero
// permutation reduction and is left baked. The primitive topology + restart
// setters are core 1.3/1.2 and live in device_1_3_core_dynamic_topology.inc.
// Each setter is gated at use time on its corresponding sub-feature bool.
XE_UI_VULKAN_FUNCTION(vkCmdSetDepthClampEnableEXT)
XE_UI_VULKAN_FUNCTION(vkCmdSetPolygonModeEXT)
XE_UI_VULKAN_FUNCTION(vkCmdSetColorBlendEnableEXT)
XE_UI_VULKAN_FUNCTION(vkCmdSetColorBlendEquationEXT)
XE_UI_VULKAN_FUNCTION(vkCmdSetColorWriteMaskEXT)
