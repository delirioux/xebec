# Xebec roadmap

Where Xebec is going, in order. Everything is built from scratch on the
chip's own SDK.

Status: ✅ done · 🔨 in progress · ⬜ to do

## Milestones

### 1. Bare boot ✅

- ✅ Own start-up code, memory layout and C runtime
- ✅ Own display driver (ST7789V on SPI1)
- ✅ The screen shows one colour, checked pixel for pixel in fm1-emu on every build
- 🔨 Boot test screen: name, version and build code, then the boot checks line by line (display, RAM, core 0), in Xebec's own font

*Proven in fm1-emu. The first check on a real FM-1 comes with milestone 2.*

### 2. Update path 🔨

- ✅ Hand-over to the OTA loader worked out: an 80-byte update record at RAM `0x01C7FD88` (CRC, type, status `0x5A00`, magic `0x5440`, name `ota-FM-1_0xx`, loader location and length), then a soft reset
- ✅ Where the loader goes: the official app stages it in free flash after its own image, on a 128 KiB boundary, written in 4 KiB blocks, clear of the user area
- ⬜ Flash driver that runs from RAM (erase and write while the code itself lives in flash)
- ⬜ Xebec carries M-VAVE's OTA loader (Apache-2.0) and stages it the same way
- ⬜ Key held at power-on: stage the loader, write the record, restart into the loader
- ⬜ Own update package for the FM-1 (`.fwsc`), with M-VAVE's boot loader, OTA loader and config byte for byte unchanged
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
