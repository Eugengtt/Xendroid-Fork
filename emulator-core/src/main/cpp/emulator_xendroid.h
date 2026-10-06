// SPDX-License-Identifier: WTFPL

#ifndef xendroid_EMULATOR_xendroid_H
#define xendroid_EMULATOR_xendroid_H

#include <jni.h>
#include <string>
#include <vector>

extern jclass g_class_Emulator;

extern jobject g_context;

extern std::vector<std::string> g_launch_args;
extern std::string g_uri_info_list_file_path;
extern std::string g_native_lib_dir;

#endif //xendroid_EMULATOR_xendroid_H
