# Quest camera trace and eye-matrix bridge

## Trace collection

The renderer now creates a trace file on first guest vertex-constant write using SDL_GetPrefPath:

- File: `quest-camera-trace.txt`
- The full resolved path is printed through SDL logging as `Quest camera trace: ...`.
- Trace lines include guest writes (float-word indices and raw IEEE-754 hex words), then draw markers with the active vertex-shader pointer and snapshots of the changed four-word constant registers.
- The trace is capped at 50,000 write records to limit storage use.

Run the game in a normal scene with camera motion, then collect this file from the app's SDL preference directory. Share it for analysis. Register identification is not inferred from code alone; it requires a trace showing values across frames and scenes.

## XR integration API

The following static methods are exposed on `Video` in `gpu/video.h`:

- `BeginQuestEyeFrame(frameSerial)` once per XR frame.
- `SetQuestEyeMatrix(0|1, matrix16)` with the per-eye matrix in the guest shader's expected 16-float layout.
- `SetQuestActiveEye(0|1)` before issuing that eye's draw commands.
- `EndQuestEyeFrame()` after the eye's draw commands.
- `ConfigureQuestCameraMatrixRegister(registerIndex)` only after the register is verified.
- `GetQuestCameraTracePath()` to retrieve the trace file path.

The eye matrix must be converted by the XR caller into the exact matrix convention expected by the game's world shader. This API does not create an OpenXR session, acquire runtime swapchain images, or invoke the eye callbacks itself. Until a caller supplies the headset-derived left/right matrices, the eye override remains inactive. The camera register is intentionally unconfigured by default.

## In-game export button

Open the `Quest VR Tools` panel in the game's ImGui overlay and press `Export camera trace`. The bridge flushes the trace before copying it to `quest-camera-trace.txt` under SDL's external-storage directory (or the current working directory when SDL does not provide one). The full export path is shown in the panel and written to the application log. The trace file remains capped to limit storage use.

The overlay currently uses the existing SDL/ImGui mouse input path. Quest Touch controller pointer/click input is not yet implemented, so the button is present in the rendered UI but is not yet guaranteed to be clickable with Quest controllers.
