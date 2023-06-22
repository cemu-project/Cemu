#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <AndroidGui/GameTitleLoader.h>
#include <AndroidGui/GameIconLoader.h>
#include <AndroidGui/AndroidGuiWrapper.h>
#include <Cafe/HW/Latte/Renderer/Renderer.h>
#include <Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h>
#include <Common/unix/FilesystemAndroid.h>
#include "AndroidGui/Utils.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "input/InputManager.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "AndroidGui/AndroidAudio.h"

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
        env->DeleteLocalRef(jIconData);
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


class AndroidFilesystemCallbacks : public FilesystemAndroid::FilesystemCallbacks {
    jmethodID m_openContentUriMid;
    jmethodID m_listFilesMid;
    jmethodID m_isDirectoryMid;
    jmethodID m_isFileMid;
    jmethodID m_existsMid;
    JNIUtils::Scopedjclass m_fileUtilClass;
    std::function<void(JNIEnv *)> m_function = nullptr;
    std::atomic_bool m_functionFinished;
    std::mutex m_functionMutex;
    std::condition_variable m_functionCV;
    std::thread m_thread;
    std::mutex m_threadMutex;
    std::condition_variable m_threadCV;
    std::atomic_bool m_continue = true;

    bool callBooleanFunction(const std::filesystem::path &uri, jmethodID methodId) {
        std::unique_lock functionLock(m_functionMutex);
        m_functionFinished = false;
        bool condition;
        {
            std::lock_guard threadLock(m_threadMutex);
            m_function = [&, this](JNIEnv *env) {
                jstring uriString = env->NewStringUTF(uri.c_str());
                condition = env->CallStaticBooleanMethod(*m_fileUtilClass, methodId, uriString);
                env->DeleteLocalRef(uriString);
            };
        }
        m_threadCV.notify_one();
        m_functionCV.wait(functionLock, [this]() -> bool { return m_functionFinished; });
        return condition;
    }

public:
    AndroidFilesystemCallbacks() {
        JNIUtils::ScopedJNIENV env;
        m_fileUtilClass = JNIUtils::Scopedjclass("info/cemu/Cemu/FileUtil");
        m_openContentUriMid = env->GetStaticMethodID(*m_fileUtilClass, "openContentUri",
                                                     "(Ljava/lang/String;)I");
        m_listFilesMid = env->GetStaticMethodID(*m_fileUtilClass, "listFiles",
                                                "(Ljava/lang/String;)[Ljava/lang/String;");
        m_isDirectoryMid = env->GetStaticMethodID(*m_fileUtilClass, "isDirectory",
                                                  "(Ljava/lang/String;)Z");
        m_isFileMid = env->GetStaticMethodID(*m_fileUtilClass, "isFile",
                                             "(Ljava/lang/String;)Z");
        m_existsMid = env->GetStaticMethodID(*m_fileUtilClass, "exists",
                                             "(Ljava/lang/String;)Z");
        m_thread = std::thread([this]() {
            JNIUtils::ScopedJNIENV env;
            while (m_continue) {
                std::unique_lock threadLock(m_threadMutex);
                m_threadCV.wait(threadLock, [&] {
                    return m_function || !m_continue;
                });
                if (!m_continue)
                    return;
                m_function(*env);
                m_function = nullptr;
                m_functionFinished = true;
                m_functionCV.notify_one();
            }
        });
    }

    ~AndroidFilesystemCallbacks() {
        m_continue = false;
        m_threadCV.notify_one();
        m_thread.join();
    }

    int openContentUri(const std::filesystem::path &uri) override {
        std::unique_lock functionLock(m_functionMutex);
        m_functionFinished = false;
        int fd;
        {
            std::lock_guard threadLock(m_threadMutex);
            m_function = [&, this](JNIEnv *env) {
                jstring uriString = env->NewStringUTF(uri.c_str());
                fd = env->CallStaticIntMethod(*m_fileUtilClass, m_openContentUriMid, uriString);
                env->DeleteLocalRef(uriString);
            };
        }
        m_threadCV.notify_one();
        m_functionCV.wait(functionLock, [this]() -> bool { return m_functionFinished; });
        return fd;
    }

    std::vector<std::filesystem::path> listFiles(const std::filesystem::path &uri) override {
        std::unique_lock functionLock(m_functionMutex);
        m_functionFinished = false;
        std::vector<std::filesystem::path> paths;
        {
            std::lock_guard threadLock(m_threadMutex);
            m_function = [&, this](JNIEnv *env) {
                jstring uriString = env->NewStringUTF(uri.c_str());
                jobjectArray pathsObjArray = static_cast<jobjectArray>(env->CallStaticObjectMethod(
                        *m_fileUtilClass,
                        m_listFilesMid, uriString));
                env->DeleteLocalRef(uriString);
                jsize arrayLength = env->GetArrayLength(pathsObjArray);
                paths.reserve(arrayLength);
                for (jsize i = 0; i < arrayLength; i++) {
                    jstring pathStr = static_cast<jstring>(env->GetObjectArrayElement(
                            pathsObjArray,
                            i));
                    paths.push_back(JNIUtils::JStringToString(env, pathStr));
                    env->DeleteLocalRef(pathStr);
                }
                env->DeleteLocalRef(pathsObjArray);
            };
        }
        m_threadCV.notify_one();
        m_functionCV.wait(functionLock, [this]() -> bool { return m_functionFinished; });
        return paths;
    }

    bool isDirectory(const std::filesystem::path &uri) override {
        return callBooleanFunction(uri, m_isDirectoryMid);
    }

    bool isFile(const std::filesystem::path &uri) override {
        return callBooleanFunction(uri, m_isFileMid);
    }

    bool exists(const std::filesystem::path &uri) override {
        return callBooleanFunction(uri, m_existsMid);
    }
};

std::shared_ptr<AndroidFilesystemCallbacks> g_androidFilesystemCallbacks = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_initializeEmulation(JNIEnv *env, jclass clazz) {
    g_androidFilesystemCallbacks = std::make_shared<AndroidFilesystemCallbacks>();
    FilesystemAndroid::setFilesystemCallbacks(g_androidFilesystemCallbacks);
    NetworkConfig::LoadOnce();
    mainEmulatorHLE();
    InputManager::instance().load();
    InitializeGlobalVulkan();
    createCemuDirectories();
    g_config.Load();
    auto &config = g_config.data();
    AndroidAudio::createAudioDevice(IAudioAPI::AudioAPI::Cubeb,
                                    config.tv_channels,
                                    config.tv_volume);
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
extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_setDPI(JNIEnv *env, jclass clazz, jfloat dpi) {
    auto &windowInfo = gui_getWindowInfo();
    windowInfo.dpi_scale = windowInfo.pad_dpi_scale = dpi;
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_clearSurface(JNIEnv *env, jclass clazz, jboolean is_main_canvas) {
    VulkanRenderer::GetInstance()->ClearSurface(is_main_canvas);
}
extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_recreateRenderSurface(JNIEnv *env, jclass clazz,
                                                        jboolean is_main_canvas) {
    VulkanRenderer::GetInstance()->NotifySurfaceChanged(is_main_canvas);
}

extern "C"
JNIEXPORT void JNICALL
Java_info_cemu_Cemu_NativeLibrary_addGamePath(JNIEnv *env, jclass clazz, jstring uri) {
    gui_addGamePath(JNIUtils::JStringToString(env, uri));
}