#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <AndroidGui/GameTitleLoader.h>
#include <AndroidGui/GameIconLoader.h>
#include <AndroidGui/AndroidGuiWrapper.h>
#include <Cafe/HW/Latte/Renderer/Renderer.h>
#include <Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h>
#include "AndroidGui/Utils.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "input/InputManager.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"


#include "JNIUtils.h"

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIUtils::g_jvm = vm;
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurface(JNIEnv *env, jclass clazz, jobject surface,
                                             jboolean is_main_canvas) {
    WindowHandleInfo *windowHandleInfo;
    if (is_main_canvas)
        windowHandleInfo = &gui_getWindowInfo().canvas_main;
    else
        windowHandleInfo = &gui_getWindowInfo().canvas_pad;
    if (windowHandleInfo->handle) {
        ANativeWindow_release(static_cast<ANativeWindow *>(windowHandleInfo->handle));
        windowHandleInfo->handle = nullptr;
    }
    if (surface)
        windowHandleInfo->handle = ANativeWindow_fromSurface(env, surface);
    gui_getWindowInfo().window_main.handle = windowHandleInfo->handle;
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setSurfaceSize(JNIEnv *env, jclass clazz, jint width, jint height,
                                                 jboolean is_main_canvas) {
    WindowHandleInfo *windowHandleInfo;
    if (is_main_canvas)
        windowHandleInfo = &gui_getWindowInfo().canvas_main;
    else
        windowHandleInfo = &gui_getWindowInfo().canvas_pad;
    gui_getWindowInfo().width = width;
    gui_getWindowInfo().height = height;
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_startGame(JNIEnv *env, jclass clazz, jlong title_id) {
    gui_startGame(*reinterpret_cast<TitleId *>(&title_id));
}

std::shared_ptr<GameTitleLoadedCallback> g_gameTitleLoadedCallback;

class AndroidGameTitleLoadedCallback : public GameTitleLoadedCallback {
    jmethodID m_onGameTitleLoadedMID;
    JNIUtils::Scopedjobject m_gameTitleLoadedCallbackObj;
public:
    AndroidGameTitleLoadedCallback(jmethodID onGameTitleLoadedMID,
                                   jobject gameTitleLoadedCallbackObj)
            : m_onGameTitleLoadedMID(onGameTitleLoadedMID),
              m_gameTitleLoadedCallbackObj(gameTitleLoadedCallbackObj) {}

    void onTitleLoaded(const Game &game) override {
        JNIUtils::ScopedJNIENV env;
        jstring name = env->NewStringUTF(game.name.c_str());
        jlong titleId = *reinterpret_cast<const jlong *>(&game.titleId);
        env->CallVoidMethod(*m_gameTitleLoadedCallbackObj, m_onGameTitleLoadedMID, titleId, name);
        env->DeleteLocalRef(name);
    }
};

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGameTitleLoadedCallback(JNIEnv *env, jclass clazz,
                                                             jobject game_title_loaded_callback) {
    jclass gameTitleLoadedCallbackClass = env->GetObjectClass(game_title_loaded_callback);
    jmethodID onGameTitleLoadedMID = env->GetMethodID(gameTitleLoadedCallbackClass,
                                                      "onGameTitleLoaded",
                                                      "(JLjava/lang/String;)V");
    env->DeleteLocalRef(gameTitleLoadedCallbackClass);
    g_gameTitleLoadedCallback = std::make_shared<AndroidGameTitleLoadedCallback>(
            onGameTitleLoadedMID,
            game_title_loaded_callback);
    gui_setOnGameTitleLoaded(g_gameTitleLoadedCallback);
}

std::shared_ptr<GameIconLoadedCallback> g_gameIconLoadedCallback;

class AndroidGameIconLoadedCallback : public GameIconLoadedCallback {
    jmethodID m_onGameIconLoadedMID;
    JNIUtils::Scopedjobject m_gameTitleLoadedCallbackObj;
public:
    AndroidGameIconLoadedCallback(jmethodID onGameIconLoadedMID,
                                  jobject gameTitleLoadedCallbackObj)
            : m_onGameIconLoadedMID(onGameIconLoadedMID),
              m_gameTitleLoadedCallbackObj(gameTitleLoadedCallbackObj) {}

    void onIconLoaded(TitleId titleId, const Image &iconData) override {
        JNIUtils::ScopedJNIENV env;
        jlong jTitleId = *reinterpret_cast<jlong *>(&titleId);
        jintArray jIconData = env->NewIntArray(iconData.m_width * iconData.m_height);
        env->SetIntArrayRegion(jIconData, 0, iconData.m_width * iconData.m_height,
                               reinterpret_cast<const jint *>(iconData.m_image));
        env->CallVoidMethod(*m_gameTitleLoadedCallbackObj, m_onGameIconLoadedMID, jTitleId,
                            jIconData, iconData.m_width, iconData.m_height);
    }
};

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setGameIconLoadedCallback(JNIEnv *env, jclass clazz,
                                                            jobject game_icon_loaded_callback) {
    jclass gameIconLoadedCallbackClass = env->GetObjectClass(game_icon_loaded_callback);
    jmethodID gameIconLoadedMID = env->GetMethodID(gameIconLoadedCallbackClass,
                                                   "onGameIconLoaded",
                                                   "(J[III)V");
    env->DeleteLocalRef(gameIconLoadedCallbackClass);
    g_gameIconLoadedCallback = std::make_shared<AndroidGameIconLoadedCallback>(gameIconLoadedMID,
                                                                               game_icon_loaded_callback);
    gui_setOnGameIconLoaded(g_gameIconLoadedCallback);
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_requestGameIcon(JNIEnv *env, jclass clazz, jlong title_id) {
    TitleId titleId = *reinterpret_cast<TitleId *>(&title_id);
    gui_requestGameIcon(titleId);
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_reloadGameTitles(JNIEnv *env, jclass clazz) {
    gui_reloadGameTitles();
}
extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeActiveSettings(JNIEnv *env, jclass clazz,
                                                           jstring data_path, jstring cache_path) {
    std::string dataPath = JNIUtils::JStringToString(env, data_path);
    std::string cachePath = JNIUtils::JStringToString(env, cache_path);
    ActiveSettings::LoadOnce({}, dataPath, dataPath, cachePath, dataPath);
}

int mainEmulatorHLE();

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeEmulation(JNIEnv *env, jclass clazz) {
    NetworkConfig::LoadOnce();
    mainEmulatorHLE();
    InputManager::instance().load();
    InitializeGlobalVulkan();
    createCemuDirectories();
}
extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializerRenderer(JNIEnv *env, jclass clazz) {
    g_renderer = std::make_unique<VulkanRenderer>();
}
extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeRendererSurface(JNIEnv *env, jclass clazz,
                                                            jboolean is_main_canvas) {
    int width, height;
    if (is_main_canvas)
        gui_getWindowPhysSize(width, height);
    else
        gui_getPadWindowPhysSize(width, height);
    VulkanRenderer::GetInstance()->InitializeSurface(
            {width, height},
            is_main_canvas);

}