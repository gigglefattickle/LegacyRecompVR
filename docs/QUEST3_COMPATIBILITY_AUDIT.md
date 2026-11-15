# Quest 3 VR Port — Compatibility Audit

## Confirmed repository facts

- The project is a static recompilation of Minecraft: Xbox 360 Edition, but its README is still a WIP.
- `LegacyRecompLib/CMakeLists.txt` generates PPC recompilation sources using `tools/XenonRecomp`.
- The configured input dependency is `LegacyRecompLib/private/tu2.xex`. This is explicit evidence that the current generation pipeline is configured for TU2, **not TU75**.
- The main CMake build requires `VCPKG_ROOT` and currently has Windows/Linux presets only.
- The root build previously applied `-march=sandybridge` to every target, which is not valid for an ARM64 Quest build. The Quest branch now scopes that optimization to x86 processors only.
- The runtime currently selects OS implementation files only for Windows and Linux. Android has no OS backend selected yet.
- The renderer is Vulkan-based and SDL supplies the desktop window/surface path. The current Vulkan interface creates its own Vulkan instance and SDL surface; it is not yet an OpenXR swapchain renderer.
- HID currently maps SDL game-controller events to the emulated Xbox gamepad. Quest Touch controller actions and pose tracking are not implemented.
- The inspected code has no OpenXR integration, Android manifest, Gradle project, or Android packaging target.

## Milestone status

| Milestone | Status | Evidence / blocker |
|---|---|---|
| TU75 compatibility | **Not verified; current config is TU2** | Build dependency points to `LegacyRecompLib/private/tu2.xex`; TU75 needs its own matching XEX, config, generated recompilation output, and validation |
| Android ARM64 build | **Blocked / not yet building** | No Android OS backend or Android build target; vcpkg and third-party Android configuration need work |
| OpenXR integration | **Not started** | No OpenXR loader, instance/session lifecycle, or Android immersive manifest |
| Stereo rendering + head tracking | **Not started** | Existing renderer is a desktop Vulkan interface, not an OpenXR swapchain path |
| Physical controller interactions | **Not started** | Existing HID is SDL gamepad-to-XInput mapping; no OpenXR action sets or tracked controller poses |

## Required next engineering steps

1. Obtain and identify a legally obtained TU75 executable and matching update data. Keep proprietary game files out of Git. Confirm title ID, media ID/build identity, executable hash, and update version before generating code.
2. Extend the XenonRecomp configuration and generated code for that exact TU75 binary. Do not relabel TU2 output as TU75.
3. Add Android platform implementations for logging, filesystem/process/user/version services, lifecycle, and app storage.
4. Create an Android Gradle/NDK application target for `arm64-v8a`, with the Khronos OpenXR Android loader and Quest immersive activity intent.
5. Refactor Vulkan initialization so OpenXR supplies the Vulkan instance/device requirements and the renderer draws to OpenXR-provided eye swapchain images. The current SDL-created window surface cannot simply be reused as the HMD display.
6. Add an OpenXR frame loop, stereo views, per-eye projection, and predicted head pose.
7. Add OpenXR action sets for left/right controller pose, trigger, grip, thumbstick, and buttons; translate these into the game's input abstraction without breaking desktop SDL controls.
8. Build and test on physical Quest 3 hardware. No build or runtime success should be claimed until CI and device tests pass.

## First prototype acceptance criteria

- Android ARM64 debug APK installs and launches on Quest 3.
- OpenXR instance and session start successfully.
- Both eye swapchains render a test scene.
- Head pose updates the view with correct orientation and position.
- Left/right controller poses and buttons are visible in a diagnostic overlay.
- Desktop Windows/Linux configurations continue to build unchanged.
