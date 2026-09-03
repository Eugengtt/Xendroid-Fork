// VK_EXT_extended_dynamic_state (#268) functions used in Xenia.
// Promoted to Vulkan 1.3 core.
// Only the EDS1 setters needed for Stage 1 of the extended-dynamic-state
// pipeline-permutation reduction. vkCmdSetPrimitiveTopology and
// vkCmdSetDepthBoundsTestEnable are intentionally omitted (topology stays
// static in Stage 1; there is no depth-bounds field in the pipeline key).
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetCullModeEXT, vkCmdSetCullMode)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetFrontFaceEXT, vkCmdSetFrontFace)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetDepthTestEnableEXT, vkCmdSetDepthTestEnable)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetDepthWriteEnableEXT,
                               vkCmdSetDepthWriteEnable)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetDepthCompareOpEXT, vkCmdSetDepthCompareOp)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetStencilTestEnableEXT,
                               vkCmdSetStencilTestEnable)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetStencilOpEXT, vkCmdSetStencilOp)
