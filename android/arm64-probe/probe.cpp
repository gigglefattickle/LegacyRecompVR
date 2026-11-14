#include <android/log.h>
#include <jni.h>
#include <vulkan/vulkan.h>

#define PROBE_LOG(...) \
    __android_log_print(ANDROID_LOG_INFO, "LegacyRecompArm64Probe", __VA_ARGS__)

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    uint32_t extension_count = 0;
    const VkResult result =
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    if (result != VK_SUCCESS) {
        PROBE_LOG("Vulkan loader query failed: VkResult=%d", static_cast<int>(result));
        return JNI_ERR;
    }

    PROBE_LOG(
        "Android ARM64 native library loaded; Vulkan instance extensions=%u",
        extension_count);
    return JNI_VERSION_1_6;
}
