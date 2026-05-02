Sample Apps for fOS (SD Card)

Includes three apps:
- hello_fos (plain text)
- button_demo (interactive button)
- ebook_demo (long, scrollable text)

Option A: Use the script
1) Insert the SD card
2) Run the script, e.g.:
   ./create_example_app.sh /Volumes/YOUR_SD

Option B: Copy directly
- Copy the “apps” folder from this directory to the SD card.
- The result on the SD card should be:
  /apps/hello_fos/...
  /apps/button_demo/...
  /apps/ebook_demo/...

app.cfg keys:
- name=Display name in the launcher
- type=text | button
- content=Filename for text apps (e.g., content.txt or book.txt)
- button_text=Button label (only for type=button)
- button_message=Text displayed after clicking (only for type=button)

Then open the AppLauncher in fOS.
