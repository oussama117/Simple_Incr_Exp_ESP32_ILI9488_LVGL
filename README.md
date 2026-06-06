# ESP32 LVGL 9 Counter App (ILI9488 + TFT_eSPI)

## Overview

This project is a hardware-accelerated Counter Application built for the **ESP32**, utilizing an **ILI9488 TFT Display (320x480)** and the **XPT2046 Touch Controller**. It leverages **LVGL v9.2** for the graphical user interface and **TFT_eSPI** for display and touch drivers.

A key feature of this codebase is its robust handling of **hardware orientation mismatches** between the display driver and the touch digitizer. It provides configurable coordinate mapping to ensure precise touch hits regardless of how the screen components were manufactured.

---

## 🛠 Hardware Configuration

The project uses standard hardware SPI. The pins are explicitly mapped in the `platformio.ini` file, bypassing the need to edit `TFT_eSPI`'s internal `User_Setup.h`.

| Pin Function | ESP32 GPIO |
| :----------- | :--------- |
| **MISO**     | 19         |
| **MOSI**     | 23         |
| **SCLK**     | 18         |
| **TFT CS**   | 15         |
| **TFT DC**   | 2          |
| **TFT RST**  | 4          |
| **TOUCH CS** | 21         |

---

## 📂 File Breakdown

### 1. `platformio.ini` (Environment & Dependencies)

This file defines the build environment, library dependencies, and passes dynamic build flags directly to the compiler.

**Key Configurations:**

- **`USER_SETUP_LOADED=1`**: Tells `TFT_eSPI` to ignore its internal configuration files and strictly use the build flags provided in this file.
- **ILI9488 & RGB565 Swap**:
  - `-D ILI9488_DRIVER=1`: Specifies the exact display driver.
  - `-D LV_COLOR_16_SWAP=1`: Crucial for ILI9488 displays. It swaps the byte order of RGB565 colors so reds and blues are not reversed in LVGL.
- **SPI Frequencies**: Optimized speeds for display rendering (`27MHz`), reading (`16MHz`), and touch polling (`2.5MHz`).
- **LVGL Heap (`LV_MEM_SIZE`)**: Allocates 24KB (`24576U`) for LVGL's internal memory management, which is ample for this lightweight interface.
- **Custom Tick**: `-D LV_TICK_CUSTOM=1` maps LVGL's internal timer directly to the ESP32's native `millis()` function.

### 2. `main.cpp` (Application Logic)

The core application handles hardware bootup, touch coordinate translation, LVGL rendering, and UI event callbacks.

**Core Components:**

- **Display Flushing (`flush_cb`)**:
  LVGL renders the UI into a small partial buffer in RAM (`draw_buf`, 10 rows high). Once a chunk is rendered, `flush_cb` pushes the pixel map via SPI to the TFT panel using `tft.pushPixels()`.
- **Touch Mapping & Orientation Fix (`touch_read_cb`)**:
  This is the most critical part of the integration. Often, the XPT2046 digitizer is physically mounted in landscape, but the display is driven in portrait. This function reads `tft.getTouch()` and manually corrects the axes (`raw_x` and `raw_y`) using swapping and mirroring logic. This guarantees the software coordinates match the physical UI layout. _(The code includes alternate commented-out mappings (A, B, C) if your specific panel requires a different 90-degree inversion)._
- **Touch Calibration Wizard**:
  Set `#define RUN_CALIBRATION 1` to boot into a temporary calibration wizard. Tap the corners as prompted, copy the 5 values from the Serial Monitor into the `TOUCH_CAL_DATA` array, and set the macro back to `0` to permanently save your panel's unique calibration.
- **The GUI (`build_ui`)**:
  Constructs a modern, dark-themed UI using LVGL widgets:
  - A styled container card (`lv_obj_create`).
  - Increment, Decrement, and Reset buttons (`lv_button_create`) styled with custom shadows and colors.
  - A central label updating dynamically via the `update_label()` callback when button events (`LV_EVENT_CLICKED`) are triggered.

---

## 🚀 How to Run

1. **Install PlatformIO**: Ensure you have PlatformIO installed in VS Code.
2. **Clone & Open**: Open the project folder containing the `platformio.ini` file.
3. **Calibrate (Optional but Recommended)**:
   - Open `main.cpp`.
   - Change `#define RUN_CALIBRATION 0` to `1`.
   - Upload the code and monitor the serial output. Tap the arrows on the screen corners.
   - Update the `TOUCH_CAL_DATA` array with your new values.
   - Change `RUN_CALIBRATION` back to `0`.
4. **Upload**: Click the `Upload` arrow in PlatformIO to compile and flash to your ESP32.

---

## 🐛 Troubleshooting Touch Alignment

If your touch gestures are mirrored or responding 90 degrees away from the UI:

1. Ensure your `TOUCH_CAL_DATA` is accurate for the current screen rotation (`tft.setRotation(2)`).
2. Navigate to the `touch_read_cb()` function in `main.cpp`.
3. Comment out the active mapping block and uncomment one of the `Alternate` mappings (A, B, or C) provided in the comments to flip or mirror the axes until the button hitboxes align with the graphics.
