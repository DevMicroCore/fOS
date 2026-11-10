# fOS 4.1.0 – Release Notes

## What's New

Version 4.1.0 introduces full support for the **Waveshare ESP32-P4 7-inch display**, adds new hardware-specific features, and fixes an issue with the on-screen keyboard in fScript applications.

### Full Waveshare ESP32-P4 Support

fOS now fully supports the Waveshare ESP32-P4 7-inch display with a resolution of 1024 × 600 pixels.

The Waveshare version includes the core fOS features and fScript application support already available on the CrowPanel ESP32-S3.

**Additional Waveshare Features:**

- **Real-Time Clock (RTC):** Support for the hardware RTC allows the system to maintain date and time information independently of an active network connection, provided the clock has been initialized and backup power is available.
- **USB Mass Storage:** The Waveshare display can be connected to a computer via USB and used as a USB storage device. This allows direct access to the SD card for transferring files, managing applications, and updating content without removing the card.

### fScript Keyboard Bug Fix

Fixed an issue that prevented the on-screen keyboard from appearing in fScript applications.

Text input fields and keyboard-dependent functions can now display the keyboard as intended.

## Supported Hardware

| Device | Display Resolution | Support |
|---|---|---|
| CrowPanel ESP32-S3 7" | 800 × 480 | Full fOS support |
| Waveshare ESP32-P4 7" | 1024 × 600 | Full fOS support + RTC and USB Mass Storage |

## Notes

- RTC and USB Mass Storage functionality are currently exclusive to the Waveshare ESP32-P4 version.
- Existing fScript applications can be used on both supported display platforms, subject to their hardware requirements.

---

**fOS 4.1.0** – Expanding hardware support and improving the fScript experience.