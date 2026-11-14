package com.legacyrecomp.android;

import android.content.Intent;
import org.libsdl.app.SDLActivity;

/**
 * SDL Android Activity with the Storage Access Framework result hook.
 * Set this class as the application activity in the Android manifest.
 */
public class LegacyRecompActivity extends SDLActivity {
    public void openGameFilePicker() {
        StoragePicker.open(this);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == StoragePicker.REQUEST_IMPORT_GAME_FILES) {
            String[] paths = StoragePicker.handleResult(this, requestCode, resultCode, data);
            onGameFilesImported(paths);
        }
    }

    /** Override or connect this callback to the native game-content registration flow. */
    protected void onGameFilesImported(String[] appPrivatePaths) {
        // Intentionally no implicit launch: the game must validate its required files first.
    }
}
