# fOS recovery build and deployment

The recovery sketch must be compiled with the same ESP32-S3 board, flash size,
Arduino-ESP32 Core and partition layout as the main fOS sketch.

For the current project use Arduino-ESP32 Core 3.3.12, export the compiled
binary from `recovery/recovery.ino`, and publish it as:

`Crowpanel_7"_esp32s3/update/recovery.ino.bin`

Then regenerate and commit the repository indexes:

```sh
python3 tools/generate_repository_indexes.py /path/to/fOS-repository
```

The main fOS updater installs this recovery image into `app1`. Do not upload the
recovery sketch with the normal Arduino Upload button: that would place it in
the normal application slot instead of the dedicated recovery partition.

## Recovering a panel already stopped in the old recovery

The old recovery has already erased or partially replaced `app0`, so a normal
restart cannot restore the main application. Restore the updated main fOS
sketch once over USB. After the corrected recovery binary has been published,
one subsequent OTA installation writes it to `app1`; later OTA updates then use
the corrected direct-partition recovery path.

As an alternative emergency route, power the panel off, replace
`/system/update/update.bin` on the SD card with a previously working, smaller
fOS application binary, insert the card again and start the panel. This is only
for escaping the already-installed old recovery; install the corrected
recovery afterward.
