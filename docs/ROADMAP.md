# Xebec roadmap

Where Xebec is going, in order. Everything is built from scratch on the
chip's own SDK.

Status: ✅ done · 🔨 in progress · ⬜ to do

## Milestones

### 1. Bare boot ✅

- ✅ Own start-up code, memory layout and C runtime
- ✅ Own display driver (ST7789V on SPI1)
- ✅ The screen shows one colour, checked pixel for pixel in fm1-emu on every build

*Proven in fm1-emu. The first check on a real FM-1 comes with milestone 2.*

### 2. Update path ⬜

- ⬜ Own update package for the FM-1 (`.fwsc`)
- ⬜ Xebec answers the FM-1 update protocol over USB-MIDI, so it can overwrite itself
- ⬜ Way back to the official firmware, tested
- ⬜ Display bring-up confirmed on a real FM-1

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
