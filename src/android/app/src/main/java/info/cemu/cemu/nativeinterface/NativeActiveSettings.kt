package info.cemu.cemu.nativeinterface

object NativeActiveSettings {
    @JvmStatic
    external fun initializeActiveSettings(
        userDataPath: String,
        configPath: String,
        dataPath: String,
        cachePath: String,
    )

    @JvmStatic
    external fun setNativeLibDir(nativeLibDir: String)

    @JvmStatic
    external fun setInternalDir(internalDir: String)

    @JvmStatic
    external fun getMLCPath(): String

    @JvmStatic
    external fun getUserDataPath(): String

    @JvmStatic
    external fun hasRequiredOnlineFiles(): Boolean
}
