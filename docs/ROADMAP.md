# Xebec roadmap

Where Xebec is going, in order. Everything is built from scratch on the
chip's own SDK.

Status: ✅ done · 🔨 in progress · ⬜ to do

## Milestones

### 1. Bare boot ✅

- ✅ Own start-up code, memory layout and C runtime
- ✅ Own display driver (ST7789V on SPI1)
- ✅ The screen shows one colour, checked pixel for pixel in fm1-emu on every build
- 🔨 Boot test screen: name, version and build code, then the boot checks line by line (display, RAM, core 0, flash), in Xebec's own font

*Proven in fm1-emu. The first check on a real FM-1 comes with milestone 2.*

### 2. Update path 🔨

- ✅ Hand-over to the OTA loader worked out: an 80-byte update record at RAM `0x01C7FD88` (CRC, type `0x5A0D` for the USB loader, status `0x5A01`, magic `0x5441`, name `ota-FM-1_015`, the staged loader's flash location), also kept in flash at the end of the staging area, then a core reset
- ✅ Where the loader goes: the official app stages it in free flash after its own image, on a 128 KiB boundary, written in 4 KiB blocks, clear of the user area
- ✅ Flash driver that runs from RAM (erase and write while the code itself lives in flash)
  - Writes only inside its window: from the first 128 KiB boundary past Xebec's own image up to flash `0xE0000`, clear of the boot loader below and the official firmware's areas above
  - Known flash chips only (Puya `85 60 14`, UC25HQ80 `B3 60 14`), write enable checked, every erase and write read back before it counts
  - Every build checks that the RAM code never reaches back into flash, and runs a driver test image in fm1-emu (refusals, erase, write, read-back, the commands that reach the chip, and a trace that each driver call stays in RAM)
  - The firmware itself only reads the chip's ID for now (the FLASH line on the boot screen); erase and write are first used by the update path
- ✅ Xebec carries M-VAVE's OTA loader (Apache-2.0) and stages it the same way
  - `vendor/m-vave/ota.bin`, unchanged; the build checks its SHA-256 and CRCs
  - Staged at the start of the write window (`0x20000` now): erase 20 KiB, write the loader, read it back against its CRC, then its file head last; on any failure the area is erased again
  - Every build stages it in fm1-emu and reads it back byte for byte
- ✅ Key held at power-on: stage the loader, write the record, restart into the loader
  - OCT- and OCT+ held while powering on: straight into update mode, no confirmation (an accidental start leaves the FM-1 in M-VAVE's loader until a firmware is installed with the web updater)
  - The screen shows UPDATE KEYS and LOADER; on a failure it says UPDATE FAILED and nothing is left behind
  - A normal boot clears a staging or record left by an earlier update
  - Every build powers the firmware on in fm1-emu with both keys, with one key, and over a leftover staging, and checks every flash write and the record byte for byte (fm1-emu stops at the reset itself)
- ⬜ Own update package for the FM-1 (`.fwsc`), with M-VAVE's boot loader, OTA loader and config byte for byte unchanged; the flash driver's write window checked against the package's layout
- ⬜ First install on a real FM-1, display bring-up confirmed
- ⬜ Way back to the official firmware, tested
- ⬜ Later: answer the update protocol over USB-MIDI, so installers can update Xebec without a key held

*The first milestone that goes onto a real FM-1.*

### 3. Panel ⬜

- ⬜ Key and button matrix (27 keys, 14 buttons)
- ⬜ The seven encoders
- ⬜ MASTER pot and battery level
- ⬜ Key and button LEDs

### 4. Audio out ⬜

- ⬜ Audio output to the FM-1's DAC
- ⬜ A single sine, clean and steady

### 5. USB and MIDI ⬜

- ⬜ USB-MIDI in and out
- ⬜ USB audio
- ⬜ TRS MIDI input

### 6. Dual core ⬜

- ⬜ Start the second core
- ⬜ Second core renders voices, first core runs everything else
- ⬜ Fallback to one core on units where the second core is unreliable

### 7. Sound engine and interface ⬜

- ⬜ What Xebec actually plays, and how it is played

## Later

- **Bluetooth MIDI** — *unlikely.* The radio can only be driven through JieLi's closed library, which does not fit Xebec's GPL-3.0-only licence.
