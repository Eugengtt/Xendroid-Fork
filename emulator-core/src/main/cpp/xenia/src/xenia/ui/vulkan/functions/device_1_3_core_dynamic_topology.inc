// Dynamic primitive topology + restart setters used in Xenia (Stage 2 EDS3
// topology). These are CORE entry points (loaded by core name via the promoted
// loader region), NOT EDS3 extension functions:
//  - vkCmdSetPrimitiveTopology was promoted to core 1.3 from
//    VK_EXT_extended_dynamic_state (#268).
//  - vkCmdSetPrimitiveRestartEnable was promoted to core 1.3 from
//    VK_EXT_extended_dynamic_state2 (#378).
// Only #included in the apiVersion >= 1.3 promoted loader block, under the
// eds3_topology gate (which itself requires apiVersion >= 1.3 plus the EDS3
// extension's dynamicPrimitiveTopologyUnrestricted property). The EDS3 extension
// is only needed for that property; the setters themselves are core.
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetPrimitiveTopologyEXT,
                               vkCmdSetPrimitiveTopology)
XE_UI_VULKAN_FUNCTION_PROMOTED(vkCmdSetPrimitiveRestartEnableEXT,
                               vkCmdSetPrimitiveRestartEnable)
