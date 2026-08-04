# fOS v3.0.0 Release Notes

Release date: 2026-08-04

## Highlights

fOS 3.0.0 is the first major release of the 3.x series and introduces customizable themes, significant Radio application improvements, expanded file management, timer alarm sounds and multiple performance optimizations.

---

## Added

### Theme System

* Blue theme
* Orange theme
* Green theme
* Theme support in native applications
* Theme support in SD applications
* Theme-aware UI values:
  * `bg=theme`
  * `bg=surface`
  * `fg=contrast`

### File System

* Create custom folders.
* Save text files inside subfolders.
* Load text files from subfolders.

### Radio

* Folder navigation in the File Player.
* Open folders using the ">" button.
* Volume slider.
* Playback progress bar.
* Audio seeking.

### Clock

* Timer alarm sound support using:

```
/system/timer.mp3
```

---

## Changed

* Updated UI Demo application.
* Calendar now starts on Monday instead of Sunday.
* Faster system boot.
* Reduced RAM usage.
* Improved overall responsiveness.

---

## Compatibility

* Existing SD applications remain fully compatible.
* Theme-aware UI commands are optional.
* Existing UI layouts continue to work without modification.
* `/system/timer.mp3` is optional.

---

## Upgrade Notes

Existing installations can be upgraded without modifying SD applications.

Developers can optionally update their own UI layouts to use:

```text
bg=theme
bg=surface
fg=contrast
```

to automatically match the currently selected system theme.