package com.legacyrecomp.android;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/** SAF-based game-file import. Files are copied into the app-private imports directory. */
public final class StoragePicker {
    public static final int REQUEST_IMPORT_GAME_FILES = 0x4C52;

    private StoragePicker() {}

    public static void open(Activity activity) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                        Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        activity.startActivityForResult(intent, REQUEST_IMPORT_GAME_FILES);
    }

    /** Call from the Activity's onActivityResult. Returns copied app-private paths. */
    public static String[] handleResult(Activity activity, int requestCode,
                                        int resultCode, Intent data) {
        if (requestCode != REQUEST_IMPORT_GAME_FILES ||
            resultCode != Activity.RESULT_OK || data == null) {
            return new String[0];
        }

        ArrayList<Uri> selected = new ArrayList<>();
        if (data.getClipData() != null) {
            for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                selected.add(data.getClipData().getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            selected.add(data.getData());
        }

        File destination = new File(activity.getFilesDir(), "imports");
        if (!destination.exists() && !destination.mkdirs()) return new String[0];

        List<String> copied = new ArrayList<>();
        for (Uri uri : selected) {
            try {
                activity.getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                String name = queryDisplayName(activity, uri);
                File output = uniqueFile(destination, safeName(name));
                try (InputStream input = activity.getContentResolver().openInputStream(uri);
                     FileOutputStream stream = new FileOutputStream(output)) {
                    if (input == null) continue;
                    byte[] buffer = new byte[64 * 1024];
                    int count;
                    while ((count = input.read(buffer)) != -1) stream.write(buffer, 0, count);
                    stream.getFD().sync();
                }
                copied.add(output.getAbsolutePath());
            } catch (Exception ignored) {
                // One inaccessible item must not prevent the remaining selections importing.
            }
        }
        return copied.toArray(new String[0]);
    }

    private static String queryDisplayName(Activity activity, Uri uri) {
        try (Cursor cursor = activity.getContentResolver().query(
                uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) return cursor.getString(column);
            }
        } catch (Exception ignored) {}
        return "game-file";
    }

    private static String safeName(String name) {
        String cleaned = name == null ? "game-file" : name.replaceAll("[^A-Za-z0-9._-]", "_");
        if (cleaned.isEmpty() || cleaned.equals(".") || cleaned.equals("..")) return "game-file";
        return cleaned;
    }

    private static File uniqueFile(File directory, String name) {
        File result = new File(directory, name);
        int suffix = 1;
        while (result.exists()) {
            int dot = name.lastIndexOf('.');
            String stem = dot > 0 ? name.substring(0, dot) : name;
            String extension = dot > 0 ? name.substring(dot) : "";
            result = new File(directory, stem + "_" + suffix++ + extension);
        }
        return result;
    }
}
