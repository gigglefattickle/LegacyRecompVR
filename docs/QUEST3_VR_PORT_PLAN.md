# Quest 3 VR Port — Bootstrap Plan

## Goal
Bring the Legacy Console Edition recompilation project toward a native Meta Quest 3 standalone VR build, preserving the Xbox 360-era presentation and gameplay.

This document is a technical plan, not a claim that the game currently builds or runs on Android/Quest.

## Target and constraints
- Target device: Meta Quest 3, standalone Android ARM64.
- Initial game target: Minecraft: Xbox 360 Edition TU75, subject to confirming the exact supported executable/recompilation inputs.
- Keep the original game executable, assets, and user-provided data out of this repository. Users must supply legally obtained game data locally.
- Preserve the existing desktop build while adding Android/Quest as a separate platform target.
- Do not assume the current renderer, input layer, audio, filesystem, or networking are Android-ready until audited.

## Milestones

### 0. Baseline and TU75 verification
- Record the upstream commit and local build instructions.
- Identify the exact title/update identifiers and executable format expected by the recompilation pipeline.
- Verify whether TU75 is supported by the current generated recompilation sources; do not infer support from the project name.
- Produce a clean desktop build and capture the first reproducible build log.

### 1. Android ARM64 bring-up
- Audit platform-specific code: windowing, graphics context, audio, input, filesystem, threading, and dynamic loading.
- Add an Android Gradle/NDK packaging layer without changing desktop defaults.
- Build a minimal ARM64 app that launches the existing engine shell on Quest.
- Add on-device logging and crash diagnostics.

### 2. OpenXR presentation and tracking
- Integrate OpenXR for Quest runtime startup, session lifecycle, swapchains, stereo views, and frame timing.
- Render the existing scene to two eye views with correct per-eye projection.
- Add 6DOF head pose tracking and recentering.
- Validate frame pacing and thermal/GPU performance before adding gameplay features.

### 3. VR interaction
- Map Quest controllers to game actions.
- Add configurable snap/smooth turning and comfort options.
- Implement VR hands, block interaction, mining, placement, and item use.
- Design inventory/crafting interactions that remain faithful to the Legacy UI while usable in VR.

### 4. Audio, saves, and multiplayer
- Validate spatial audio and Android audio focus.
- Keep worlds and settings in app-private storage with import/export support for user-owned saves.
- Audit the existing network stack before attempting online multiplayer.
- First validate local/LAN sessions; only then investigate compatibility with existing services and community servers.

### 5. Quest packaging and performance
- Package a signed debug APK for sideload testing.
- Test on physical Quest 3 hardware.
- Profile CPU/GPU, memory, thermal throttling, and frame timing.
- Set a stable VR frame-rate target and reduce rendering cost as required.

## Immediate engineering checklist
- [ ] Inspect repository tree, build system, and upstream base.
- [ ] Confirm exact TU75 recompilation support and required user-supplied inputs.
- [ ] Establish a reproducible desktop build.
- [ ] Identify renderer and platform abstraction boundaries.
- [ ] Prototype Android ARM64 launch before integrating OpenXR.
- [ ] Add OpenXR stereo rendering and head tracking.
- [ ] Add controller input and VR interaction.

## Definition of the first useful prototype
A Quest 3 debug APK that starts reliably, enters an OpenXR session, tracks the headset in 6DOF, and displays a simple test scene in stereo. This milestone deliberately comes before full Minecraft gameplay or multiplayer.
