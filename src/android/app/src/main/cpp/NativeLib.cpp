#include "JNIUtils.h"

extern "C" [[maybe_unused]] JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, [[maybe_unused]] void* reserved)
{
	JNIUtils::SetJavaVM(vm);
	return JNI_VERSION_1_6;
}
