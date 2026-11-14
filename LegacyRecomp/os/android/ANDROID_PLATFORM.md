# Android storage and diagnostics

The Android adapter stores mutable application data under SDL's app-private internal
storage directory. It creates these directories on startup:

- `game/` — imported runtime/game files
- `saves/` — save data
- `config/` — settings
- `imports/` — files staged for import
- `logs/legacyrecomp.log` — persistent application log
- `cache/` — disposable cache data

App-private storage is sandboxed by Android and does not require
`READ_EXTERNAL_STORAGE`, `WRITE_EXTERNAL_STORAGE`, or all-files access. Do not
request broad storage permissions just to access these directories.

Importing files from shared storage should use Android's Storage Access Framework
(`ACTION_OPEN_DOCUMENT`) and copy the user-selected content into `imports/`.
That picker needs an Android activity/JNI result bridge; it is intentionally not
simulated by requesting obsolete broad storage permissions.

The native logger writes to both logcat and the persistent log file. Fatal native
signals and uncaught C++ termination write a minimal crash marker before the
fatal signal is re-raised so Android can still produce its normal tombstone.
