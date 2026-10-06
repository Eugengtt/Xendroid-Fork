package xendroid.hardware;

// Created by xendroid on 2025/7/21.
// SPDX-License-Identifier: WTFPL
public class ProcessorInfo {
    static {
        java.lang.System.loadLibrary("hardware_ProcessorInfo");
    }
    public static native String gpu_get_physical_device_name_vk();
}
