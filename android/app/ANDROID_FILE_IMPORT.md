# Android game-file import

The SAF picker is implemented in `StoragePicker.java`. It opens Android's
system document picker, supports selecting multiple files, takes persistable
read grants, and copies the selected content into the app-private
`files/imports/` directory. It requests no broad external-storage permission.

## Activity integration

Use `com.legacyrecomp.android.LegacyRecompActivity` as the Android launcher
activity (it extends SDLActivity) and include both Java files in the Android
application's Java source set. The Android app/Gradle packaging target is not
currently part of the native-library CI workflow, so this source is not yet
compiled into an APK.

Call `openGameFilePicker()` from the app's import action. The activity receives
the copied private paths through `onGameFilesImported(String[])`. Connect that
callback to the game's existing content validation/mount flow before attempting
to boot. Do not treat a successful copy as proof that the selected files are a
valid game dump.

No READ_EXTERNAL_STORAGE, WRITE_EXTERNAL_STORAGE, or MANAGE_EXTERNAL_STORAGE
permission is required for this SAF flow.
