# ESP8266 Upload Troubleshooting

Notes on getting `FastLED_Test.ino` (and other sketches) to upload to the ESP8266 from Arduino IDE on Windows. Captured from a real debugging session — keep this nearby if uploads start failing again.

## Hardware setup

- Board: ESP8266 dev board (silkscreen on the RF can says `ESP8266MOD`, NodeMCU-style "Dx" pin labels).
- USB-to-serial chip on the board: WCH CH340.
- LED matrix: 16x16 WS2812B, wired with `5V → 5V`, `GND → GND`, `D4 (GPIO2) → DIN`.
- IDE: Arduino IDE on Windows 11.

## TL;DR — fast path to fix uploads

1. Disconnect the LED matrix at the connector (don't leave the loose data wire touching anything conductive).
2. In Device Manager, confirm the CH340 driver version is **3.5.2019.1**, not 3.8.x or 3.9.x. If newer, downgrade (see below).
3. Manually enter bootloader mode: unplug USB, jumper `D3` (GPIO0) to `GND`, plug in USB, wait 2 seconds, remove the jumper, click Upload.
4. Match Arduino IDE → Tools → Port to whatever COM number Device Manager shows.

## Symptom 1: `PermissionError(13, 'A device attached to the system is not functioning.')`

Full error tail:
```
A fatal esptool.py error occurred: Cannot configure port, something went wrong.
Original message: PermissionError(13, 'A device attached to the system is not functioning.', None, 31)
```

### What it means

esptool can't open / configure the COM port. The compile already finished — this is purely a Windows-side problem. It happens *before* esptool tries to talk to the ESP8266.

### Root cause

Newer WCH CH340 drivers (3.8.x, 3.9.x — released 2024) reject some `SetCommState` flag combinations that older `esptool.py v3.0` (which ships with older ESP8266 board packages) still sends. Windows reports system error 31 ("device not functioning"). The error wording wobbles between attempts because the underlying call is getting different rejection paths.

### How to confirm it's this specific issue

- Arduino IDE Serial Monitor *can* open the port (you see boot garbage / dots at the wrong baud) — meaning the driver and port basically work.
- esptool fails at "Cannot configure port" specifically.
- Device Manager → CH340 → Driver tab shows version 3.8.x or 3.9.x with a 2024 date.

### Fix: roll back the CH340 driver to 3.5.2019.1

1. Device Manager → Ports (COM & LPT) → right-click `USB-SERIAL CH340 (COMx)` → **Uninstall device** → tick **Delete the driver software for this device** → OK.
2. Unplug the ESP8266.
3. Download the older driver from SparkFun's tutorial (trusted Arduino-community mirror): https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all
4. Run the installer.
5. Plug the ESP8266 back in. Verify Device Manager → CH340 → Driver tab now shows version `3.5.2019.1`.

### Things that don't fix this (already tried, save the time)

- Rebooting Windows.
- Trying different USB ports.
- Trying different USB cables.
- Closing other terminals / Serial Monitor.
- Disabling USB selective suspend.
- Reinstalling the ESP8266 board package in Boards Manager (the package was already up to date, and bundled esptool 3.0 still didn't talk to the new driver).
- Uninstalling greyed-out / hidden duplicate CH340 entries.

These are all standard troubleshooting steps and worth doing for sanity, but the actual fix is the driver downgrade.

## Symptom 2: `Failed to connect to ESP8266: Timed out waiting for packet header`

Full error tail:
```
Connecting......_____.....
A fatal esptool.py error occurred: Failed to connect to ESP8266: Timed out waiting for packet header
```

### What it means

esptool successfully opened the port and toggled DTR/RTS to put the ESP8266 into download mode, but the chip never responded to the sync packet. The chip is not in bootloader mode.

This is *good news* if you're transitioning from Symptom 1 — it means the COM-port problem is solved. We're now past Windows and talking to the chip itself.

### Root cause

The auto-reset circuit (DTR/RTS → RST/GPIO0) isn't successfully landing the chip in bootloader mode. Possible reasons on this board:

- Anything connected to GPIO0 / GPIO2 / GPIO15 strap pins is fighting the bootloader. On this build, the LED data line is on `D4` (GPIO2). Even with the matrix unplugged, the *dangling solder-tail wire* on D4 can pick up noise or short to something.
- Auto-reset capacitor timing is off (sometimes happens after extra wires/load are added to the board).

### Fix: manually enter bootloader mode

The reliable cold-boot procedure (no timing window required):

1. **Unplug the USB cable** from the ESP8266.
2. Connect a jumper wire from `D3` (GPIO0) to a `GND` pin. Make sure it's a solid mechanical connection.
3. **With the jumper still connected**, plug USB back in.
4. Wait ~2 seconds. The chip is now in bootloader mode and will stay there until the next reset.
5. Remove the jumper from `D3`.
6. Click **Upload** in Arduino IDE.

### How to know you're in bootloader mode

- Any previous sketch's behavior (LED chase on the matrix, blinking onboard LED, etc.) does **not** run.
- Serial Monitor at **74880 baud** shows nothing on reset (a normal boot prints a banner like `ets Jan 8 2013, rst cause:1, boot mode:(3,7)`).

### Pin reference (NodeMCU-style boards)

| Silkscreen | GPIO   | Notes                                    |
|------------|--------|------------------------------------------|
| D0         | GPIO16 |                                          |
| D1         | GPIO5  | Safe for LED data                        |
| D2         | GPIO4  | Safe for LED data                        |
| D3         | GPIO0  | **FLASH / bootloader strap pin**         |
| D4         | GPIO2  | Strap pin (must be HIGH at boot). Currently used for LED DIN — works but causes upload pain |
| D5         | GPIO14 |                                          |
| D6         | GPIO12 |                                          |
| D7         | GPIO13 |                                          |
| D8         | GPIO15 | Strap pin (must be LOW at boot)          |

### Things to also check

- The dangling end of the D4 wire (the side that normally plugs into the matrix's DIN) must not be touching anything conductive. Tape it off if needed.
- If you have a multimeter: GPIO0 sits at ~3.3 V at idle and should drop to ~0 V when the jumper to GND is properly making contact. Use this to verify the jumper is doing its job.

## Other things to remember

### Power the matrix separately

A 16x16 WS2812B matrix can pull up to ~15 A at full white. USB ports give 0.5–0.9 A. Currently the matrix's `5V` is wired to the ESP8266's `5V` pin, which means it's pulling matrix current through the USB rail. This caused early symptoms in the debugging session (browning out the CH340 chip, which produced flaky `PermissionError(13)` errors that looked like the driver issue — they compounded).

For real use:
- Cut or disconnect the matrix's `5V` wire from the ESP8266's `5V` pin.
- Power the matrix from its own 5 V supply (10 A+ for full brightness; a 5 V / 2 A supply is fine for testing low-brightness patterns).
- Keep `GND` shared between the supply and the ESP8266.
- Standard WS2812B practice: ~470 Ω resistor inline on the data line near the matrix, plus a 1000 µF electrolytic cap across the matrix's 5 V/GND near the input.

### Consider moving the LED data line off D4 (GPIO2)

D4 is a boot strap pin. As long as the LED data line lives there, every upload risks bootloader-entry trouble. `D2` (GPIO4) or `D1` (GPIO5) are the safest alternatives — neither is a strap pin.

If you move the wire, update the sketch:
```cpp
#define LED_PIN 4   // D2 = GPIO4 (recommended)
// or
#define LED_PIN 5   // D1 = GPIO5
```

### Confirming the COM port

Each USB port assigns its own COM number, and unplug/replug + driver changes can shuffle them. From PowerShell:
```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```
Whatever this prints with the board plugged in is what Arduino IDE → Tools → Port should be set to.

### Build warnings that are *not* problems

This warning during compile is normal and can be ignored:
```
WARNING: The SPI pins you chose have not been marked as hardware accelerated within
the code base. All SPI access will default to bitbanged output.
```
WS2812B uses bit-banged output by design on ESP8266. Nothing to fix.

## Quick diagnostic order if uploads break again

1. Compile errors? → fix those first; they're sketch issues, not upload issues.
2. `PermissionError(13)` / "Cannot configure port"? → CH340 driver version. Roll back to 3.5.2019.1.
3. `Failed to connect: Timed out waiting for packet header`? → Manual bootloader entry (D3-to-GND cold boot).
4. Connects but garbage uploaded / chip won't run? → Check power (matrix on USB rail?), check strap pins (anything on D3/D4/D8?).
5. Port disappears / "Invalid serial port"? → Check Device Manager for the current COM number; the port may have been reassigned.
