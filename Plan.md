## DIY ESP32-S3 Fractal FM Synthesizer

*Documentation reflecting the project state as of Monday, July 27, 2026.*

This document outlines the complete hardware, software, and architectural setup for the dual-core FM synthesizer, establishing the foundation for a future live-sampling granular engine.

## Hardware Inventory

*   **Microcontroller:** ESP32-S3 (8MB PSRAM, 16MB Flash, 2 USB-C ports, SD slot)
*   **Audio Output:** CJMCU-5102 I2S DAC
*   **Inputs:** 25 tactile push buttons, 7 10k potentiometers, 1 rotary encoder
*   **Power:** USB-C breakout board, 3.3V LDO, filtering capacitors
*   **Output:** Active speakers
*   **Future Expansion:** SPI 0.96" OLED display, DIY FX Rack (op-amp scaling to 10Vpp and sync out)

## Pin Mapping

This layout avoids all pins reserved for the 8MB Octal PSRAM and the internal USB interface. Button pins are configured to use internal pull-up resistors (`INPUT_PULLUP`), requiring only a direct connection to ground.

| Component | Pin (GPIO) | Function |
| :--- | :--- | :--- |
| **I2S DAC (BCK)** | 4 | Bit Clock |
| **I2S DAC (WS)** | 5 | Word Select / Left-Right Clock |
| **I2S DAC (DATA)** | 6 | Serial Data |
| **Octave Down** | 1 | Shifts base frequency down |
| **Octave Up** | 2 | Shifts base frequency up |
| **Keyboard** | 7 through 18 | 12-key chromatic scale input |

*Note: MUX pins (for pots), the rotary encoder, and the remaining 11 utility buttons will be mapped when integrating the granular engine and UI.*

## System Architecture

To ensure the audio engine never drops a buffer, tasks are strictly divided between the ESP32-S3's two cores.

| Core | Role | Primary Tasks |
| :--- | :--- | :--- |
| **Core 0 (PRO_CPU)** | UI & Control | Runs the sequencer, polls hardware buttons/MUX, handles LFO automation, and manages future OLED SPI rendering. |
| **Core 1 (APP_CPU)** | Audio Engine | Computes FM oscillators, calculates envelopes, runs Mozzi FX, manages the PSRAM circular buffer, and drives the I2S DAC. |

## Software Configuration

**Environment:** PlatformIO

The `platformio.ini` uses the `pioarduino` fork to guarantee stable I2S drivers under Arduino Core 3.x. The memory is strictly configured for Octal SPI (`qio_opi`) to properly address the 8MB PSRAM without crashing the chip.

```ini
[env:esp32-s3-devkitc-1]
platform = [https://github.com/pioarduino/platform-espressif32.git](https://github.com/pioarduino/platform-espressif32.git)
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

; --- MEMORY CONFIGURATION ---
board_build.arduino.memory_type = qio_opi 
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216 

build_flags = 
    -DBOARD_HAS_PSRAM
    -D CORE_DEBUG_LEVEL=3
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ARDUINO_USB_MODE=1

lib_deps =
    sensorium/Mozzi @ ^2.0.0
    thomasfredericks/Bounce2 @ ^2.71