# Quest 3 Android platform bring-up status

Target: Minecraft: Xbox 360 Edition TU75, standalone Meta Quest 3, first-person VR with tracked hands.

## Changes in this branch

- Added Android implementations for the existing OS abstraction: SDL/logcat logging, app-private working-directory handling, Android API-level reporting, and safe no-op behavior for desktop-only media/console/process-launch features.
- Selected those Android source files in `LegacyRecomp/CMakeLists.txt`.
- Guarded the x86-only `cpuid.h` include so an ARM64 compiler does not try to include an x86 header.
- Added the Android POSIX include branch to the precompiled header.

## What this milestone does not prove

This is source-level platform groundwork only. No Android NDK build, APK, install, OpenXR session, or Quest hardware run has been completed.

The existing OpenXR branch already contains loader/session setup, Touch action bindings, and swapchain acquisition, but its own runtime notes explicitly state that it does not yet submit a stereo projection layer. The guest renderer still records one command list to one target.

## TU75 is still a separate blocker

The current recompilation configuration is not TU75:

- `LegacyRecompLib/config/LCE.toml` points to `../private/tu2.xex`.
- It uses `LCE_TU2_switch_tables.toml`.
- `LegacyRecompLib/CMakeLists.txt` generates PPC sources from `private/tu2.xex`.

Do not rename TU2 data or generated output to TU75. TU75 needs the matching, user-supplied executable/update data, verified build identity, TU75 switch tables/configuration, and freshly generated PPC sources. Keep proprietary game files out of Git.

## Remaining blockers before a first Quest APK

1. Add the Android app/Gradle module and SDL Android activity/lifecycle integration; the current repository has only an OpenXR Gradle dependency fragment.
2. Complete Android dependency/toolchain configuration (including host DXC shader compilation and Android-compatible vcpkg dependencies).
3. Replace the hard-coded desktop XEX path in `LegacyRecomp/main.cpp` with a proper user-data path/configuration after the TU75 input contract is defined.
4. Finish Android storage/import handling for the user's legally obtained game files.
5. Build ARM64 and fix compiler/linker errors.
6. Complete the OpenXR stereo renderer before claiming visible Minecraft gameplay.

## Validation checklist

- [ ] Android ARM64 CMake configure succeeds.
- [ ] Native target compiles and links.
- [ ] Android APK packages and installs.
- [ ] App launches through SDL's Android activity.
- [ ] OpenXR loader and session initialize on Quest 3.
- [ ] Left/right eye images receive a valid projection layer.
- [ ] TU75 executable identity matches the generated recompilation sources.
