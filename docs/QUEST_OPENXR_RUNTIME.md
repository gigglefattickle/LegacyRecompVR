# Quest 3 OpenXR runtime integration

This branch adds the first Android/OpenXR runtime path for the Quest 3 target.

## Android loader dependency

The Android application module must enable Prefab and include the Khronos loader AAR. A Gradle fragment is provided at `android/quest-openxr.gradle`; apply it from the real Android app module's `build.gradle`.

The consuming app also needs an Android activity that initializes SDL before the native entry point calls `Video::CreateHostDevice`. The OpenXR code obtains the JavaVM and Activity through SDL's Android APIs. Add the following categories to the SDL activity's existing launcher intent filter:

```xml
<intent-filter>
    <action android:name="android.intent.action.MAIN" />
    <category android:name="org.khronos.openxr.intent.category.IMMERSIVE_HMD" />
    <category android:name="android.intent.category.LAUNCHER" />
</intent-filter>
```

Do not create a second launcher activity; add the OpenXR category to the activity that actually hosts SDL.

## What this branch implements

- Calls `xrInitializeLoaderKHR` with the SDL JavaVM and Activity.
- Creates an OpenXR instance with `XR_KHR_android_create_instance` and `XR_KHR_vulkan_enable2`.
- Creates the Vulkan instance through `xrCreateVulkanInstanceKHR`.
- Selects the exact Vulkan physical device returned by `xrGetVulkanGraphicsDevice2KHR`.
- Creates Plume's logical Vulkan device through `xrCreateVulkanDeviceKHR`, using Plume's device-create info.
- Creates an OpenXR session and LOCAL/VIEW spaces.
- Creates left/right Vulkan swapchains and acquires/waits/releases one image for each eye each frame.
- Creates Oculus Touch action bindings for aim/grip pose, trigger, squeeze, X/Y/A/B, and thumbsticks.
- Converts the right controller aim ray (left hand fallback) into a head-locked 1280x720 ImGui pointer; trigger click is injected as the primary mouse button.
- Polls OpenXR events and runs wait/begin/end frame from the video present loop.

## Critical rendering limitation

**This is not yet a working stereo game renderer.** Plume's current LegacyRecomp path records one guest command list to one back buffer. The OpenXR runtime now owns two eye swapchains, but the existing guest rendering path has not been refactored to record/submit the scene to both eye images with their independent view/projection transforms. Therefore the present loop intentionally calls `xrEndFrame` with no projection layer. It avoids presenting uninitialized swapchain images.

The current Plume target remains an offscreen 1280x720 texture for this interim session/input integration. Plume now also has a non-owning `VulkanTexture` wrapper for runtime-owned RGBA8/BGRA8 images, and `Video::PrepareQuestEyeRenderTarget()` lazily creates/caches a Plume framebuffer for the acquired image index. These wrappers do not destroy the OpenXR-owned `VkImage`; they only own their Vulkan image views and framebuffer objects. `GetViews()`, `GetEyeImage()`, the eye swapchain accessors, and `Video::GetQuestEyeFrameData()` expose each acquired `VkImage`, its `VkFormat`, dimensions, swapchain/image index, and matching `XrView` to the video/render layer. That is the Plume handoff seam; the actual stereo draw path still has to consume it.

## Android build limitations

This repository still does not contain a complete Android app module, Android SDL activity/lifecycle integration, or the Android OS backend implementations required by the whole LegacyRecomp executable. The CMake changes add the OpenXR source and Prefab package linkage, but they do not by themselves produce an APK.

No Android arm64 build, APK packaging, or Quest hardware run has been performed for this branch. The changes are source-level integration work and require a full Android build environment plus device validation.

## Next renderer milestone

The next substantial change must split or redesign the guest render pass so the one simulation update feeds two eye renders. The current render queue consumes each guest command once and records into one `g_commandLists[g_frame]`; simply switching the target after the command list has ended cannot produce stereo. The renderer must retain/replay only the guest draw-state/draw operations (not duplicate resource uploads, unlocks, or one-shot copies), or refactor guest draw emission into a reusable per-eye recording phase. Each eye phase must bind the cached external framebuffer, apply its own located view/projection camera uniforms, and finish with an explicit color-attachment-to-`XR_LAYOUT_COLOR_ATTACHMENT`/runtime-required transition and GPU completion synchronization before releasing either swapchain image. It must bind each acquired OpenXR VkImage as a Plume render target, use the matching located XrView pose/FOV for each eye, insert the required image layout transitions and synchronization, and submit the two projection views only after GPU completion. Merely calling Present twice would advance/consume the guest render stream incorrectly and is not safe.
