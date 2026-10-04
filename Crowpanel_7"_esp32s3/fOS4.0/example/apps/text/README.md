# Text fScript App

This is the fScript implementation of the fOS text editor. It uses `file_list()`
and the TextArea methods `load_file(path)` and `save_file(path)`.

Files are stored inside the app folder under `files/`. Text files up to 4000
characters are loaded completely; larger files are truncated with a marker.
