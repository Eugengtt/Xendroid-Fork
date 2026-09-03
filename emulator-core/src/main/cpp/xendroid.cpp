// SPDX-License-Identifier: WTFPL

#include <jni.h>
#include <android/log.h>

#define LOGI(...) {}
#define LOGW(...) {}
#define LOGE(...) {}
#define PR {}
#define PRE(f) {}

int register_Emulator(JNIEnv* env);
int register_Emulator$Config(JNIEnv* env);

int register_xendroid_Emulator(JNIEnv* env);

JavaVM* g_jvm;

extern "C" __attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM* vm, void* reserved){

    g_jvm = vm;

    JNIEnv* env = NULL;
    int result=-1;

    LOGW("JNI_OnLoad ");
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("GetEnv failed");
        goto bail;
    }

    if(register_Emulator(env) != JNI_OK){
        LOGE("register_Emulator failed");
        goto bail;
    }

    if(register_Emulator$Config(env) != JNI_OK){
        LOGE("register_Emulator$Config failed");
        goto bail;
    }

    if(register_xendroid_Emulator(env) != JNI_OK){
        LOGE("register_xendroid_Emulator failed");
        goto bail;
    }
    result = JNI_VERSION_1_6;

    LOGW("JNI_OnLoad OK");

    bail:
    return result;
}