# Xebec roadmap

Where Xebec is going, in order. Everything is built from scratch on the
chip's own SDK.

Status: ✅ done · 🔨 in progress · ⬜ to do

## Milestones

### 1. Bare boot ✅

- ✅ Own start-up code, memory layout and C runtime
- ✅ Own display driver (ST7789V on SPI1)
- ✅ The screen shows one colour, checked pixel for pixel in fm1-emu on every build
- ✅ Boot test screen: name, version and build code, then the boot checks line by line (display, RAM, CPU clock, flash), in Xebec's own font (confirmed on an FM-1 with 0.0.5)

*Proven in fm1-emu. The first check on a real FM-1 comes with milestone 2.*

### 2. Update path 🔨

- ✅ Hand-over to the OTA loader worked out: an 80-byte update record at RAM `0x01C7FD88` (CRC, type `0x5A0D` for the USB loader, status `0x5A01`, magic `0x5441`, name `ota-FM-1_015`, the staged loader's flash location), also kept in flash at the end of the staging area, then a core reset
- ✅ Where the loader goes: the official app stages it in free flash after its own image, on a 128 KiB boundary, written in 4 KiB blocks, clear of the user area
- ✅ Flash driver that runs from RAM (erase and write while the code itself lives in flash)
  - Writes only inside its window, flash `0xE0000`–`0xE5000`: the update staging area and nothing else, clear of the package below and the official firmware's areas above
  - Known flash chips only (Puya `85 60 14`, UC25HQ80 `B3 60 14`), write enable checked, every erase and write read back before it counts
  - Every build checks that the RAM code never reaches back into flash, and runs a driver test image in fm1-emu (refusals, erase, write, read-back, the commands that reach the chip, and a trace that each driver call stays in RAM)
  - The firmware itself only reads the chip's ID for now (the FLASH line on the boot screen); erase and write are first used by the update path
- ✅ Xebec carries M-VAVE's OTA loader (Apache-2.0) and stages it the same way
  - `vendor/m-vave/ota.bin`, unchanged; the build checks its SHA-256 and CRCs
  - Staged at flash `0xE0000`, where Felucca stages the same loader: erase 20 KiB, write the loader, read it back against its CRC, then its file head last; on any failure the area is erased again
  - Every build stages it in fm1-emu and reads it back byte for byte
- ✅ Key held at power-on: stage the loader, write the record, restart into the loader
  - OCT- and OCT+ held while powering on: straight into update mode, no confirmation (an accidental start leaves the FM-1 in M-VAVE's loader until a firmware is installed with the web updater)
  - The screen shows UPDATE KEYS and LOADER; on a failure it says UPDATE FAILED and nothing is left behind
  - A normal boot clears a staging or record left by an earlier update
  - Every build powers the firmware on in fm1-emu with both keys, with one key, and over a leftover staging, and checks every flash write and the record byte for byte (fm1-emu stops at the reset itself)
- ✅ Own update package for the FM-1 (`.fwsc`), with M-VAVE's boot loader, OTA loader and config byte for byte unchanged; the flash driver's write window checked against the package's layout
  - M-VAVE's stock package (FM-1_015) with Xebec in the app slot: the same head, layout and files, only the slot and its two CRCs differ; no factory patches, so an install leaves the FM-1's patches alone
  - Identity `FM-1_47932320`, in the range of Xebec's sister firmwares
  - Every build checks it against M-VAVE's package fetched from M-VAVE's repository, checks the write window (`0xE0000`–`0xE5000`, inside VM, past the package, clear of BTIF, the patches and key_mac), and boots the package in fm1-emu to the boot screen
- ✅ Installer page (GitHub Pages, `dev/`): Web MIDI, both update steps, only packages that keep M-VAVE's boot loader and loader; also puts M-VAVE's stock firmware back from update mode
  - 0.0.27: off the USB bus (controller off, D+ pull-up off, 30 ms) before the restart into the loader, as Felucca does; before, the loader started on a controller still in Xebec's state and its session stalled
  - Tested against Manta's own update code: it accepts Xebec's package and stages the loader byte for byte as Xebec's update mode does, and refuses a package whose head does not match the FM-1's without writing anything
- ✅ First install on a real FM-1, display bring-up confirmed
  - 0.0.4 installed from Manta and booted: backlight on, but no picture (SPI1 was not routed to the display pins nor set as master); update mode, the flash driver and the hand-over to M-VAVE's loader all worked on hardware
  - 0.0.5: SPI1 routing and master mode as M-VAVE's SDK and Felucca set them; the boot screen shows on an FM-1, every check OK
  - 0.0.6: the CORE 0 line becomes CPU, core 0's clock measured against TIMER4 (24 MHz crystal) with 1,000,000 dependent adds from RAM; an FM-1 shows 360 MHz, M-VAVE's boot configuration's `SYS_CLK` (fm1-emu runs at 240)
- ✅ Way back to the official firmware, tested: OCT- + OCT+ at power-on, then M-VAVE's FM-1_015 from the installer page (done on an FM-1 with 0.0.4)
- ✅ Install detection: Xebec answers the identity question over USB-MIDI (`FM-1_47932320`), and the installer page confirms Xebec runs after an install
- 🔨 Update over USB-MIDI, no key held: for now Xebec stages its loader right away on the installer's UPGRADE (the page checks the package). The checks over USB (`update_usb.c`: UFW head, board size, loader, flash head, the installer's go-ahead; native test in CI) wait for the babble fix below
- ⬜ USB babble: on an FM-1 about half of Xebec's EP1 IN replies end in `urb status -75` and a port reset, with register-for-register Felucca's USB code and M-VAVE's (checked in `libfm1_engine.a`). Not the cause: screen drawing, flash access, poll rate, SOF flag, TXMAXP, a FIFO leftover, the RAM layout, LOWP_MD. 0.0.25 holds core 1 at start-up: still babble. 0.0.26 writes POWER 0x40 (soft connect only, as M-VAVE's SDK) instead of 0x60 (with the high-speed enable bit). Next, if that is not it: bisect against Felucca's runtime (timer-interrupt poll, interrupt set-up)

*The first milestone that goes onto a real FM-1.*

### 3. Panel ⬜

- ⬜ Key and button matrix (27 keys, 14 buttons)
- ⬜ The seven encoders
- ⬜ MASTER pot and battery level
- ⬜ Key and button LEDs

### 4. Audio out ⬜

- ⬜ Audio output to the FM-1's DAC
- ⬜ A single sine, clean and steady

### 5. USB and MIDI 🔨

- 🔨 USB-MIDI device, polled: enumerates as "FM-1 Xebec" (pid.codes test ids `1209:0001`), SysEx in and out; notes and other MIDI to follow
  - Rule: no loop may spin on a peripheral register while USB is in use. Spinning on the SIE or TIMER4 starved the USB DMA: the host saw babble (`urb status -75`) on Xebec's replies and reset the port. Between polls the CPU waits on itself (`cpu_wait`, a nop loop from the cache)
  - The boot screen's USB line turns OK once a computer has configured the FM-1; every build checks enumeration and the identity reply against fm1-emu's USB host
  - Confirmed on an FM-1 with 0.0.7: the USB line shows OK
- ⬜ USB audio
- ⬜ TRS MIDI input

### 6. Dual core ⬜

- ⬜ Start the second core
- ⬜ Second core renders voices, first core runs everything else
- ⬜ Fallback to one core on units where the second core is unreliable

### 7. Voice budget ⬜

Measured before any feature is promised: everything below is sized by it.

- ⬜ How many FM voices Xebec renders at once on an FM-1 (one core and two), with effects running
- ⬜ From that: the number of layers, voices per layer and effects per layer

### 8. Synth workstation ⬜

Xebec is an instrument first, then a sequencer, then a mixer. No generative
music at its centre: a composer is there only as a tool, when the player
calls it.

**Start-up**

- ⬜ Boots straight into a playable instrument; optionally into the last session instead
- ⬜ No fresh groove at power-on

**Sound**

- ⬜ Multitimbral FM engine: several layers, each with its own patch
- ⬜ Patch editor at the heart of the firmware: shape a sound with the knobs, try it on the keys, save it as your own
- ⬜ Own patch library, saved in flash
- ⬜ Real synth effects per layer, plus master effects
- ⬜ No samples, no synthesized cymbals; flash goes to the player's own patches (a tiny crisp drum kit is the only possible exception)

**Sequencer**

- ⬜ Step entry and live recording with quantise
- ⬜ Real triplets and quintuplets (different notes per step) and time signatures beyond 4/4
- ⬜ Patterns replay everything: all effects are kept and saved with a pattern
- ⬜ Song chaining: song sections are patterns, linked into a full song
- ⬜ Knob moves recorded into patterns (motion recording); the session format ready for it from the start
- ⬜ Arpeggiator with several modes, per layer
- ⬜ Chord changes and modulation to another key
- ⬜ One global quantise setting for changes: instant or on the next bar

**Mixer**

- ⬜ Each layer's home page has its tone knobs right there (filter, resonance, drive)
- ⬜ Pump (ducking) and space pages: reverb, delay, width
- ⬜ Mute and fade in/out per layer
- ⬜ Stereo meter and load (CPU) meter always on the main screen

**Composer (tool only)**

- ⬜ Works only on what is selected: fill a pattern, write a bass line under the chords, vary a part, harmonise a melody
- ⬜ Every composer action undone with one press
- ⬜ Never touches anything the player played

**Playing**

- ⬜ Keys played as pressed; SNAP to the scale as an option, off by default
- ⬜ Held notes on brass and wind patches get a slight, human vibrato

**Interface**

- ⬜ UI plan at real FM-1 size, approved before any code
- ⬜ The SELECT knob alone can manage the whole system
- ⬜ Long press on HOME always goes back to the first home screen; PLAY and REC work on every screen
- ⬜ Keys are only for playing and entering steps, never for settings
- ⬜ Settings page: hardware, licence, CPU, memory, credits

**MIDI and sync**

- ⬜ MIDI out per layer, with a page to see what goes out
- ⬜ MIDI learn, following the shared MIDI implementation chart of its sister firmwares
- ⬜ Clock in and out to industry sync standards, on a menu page of its own

**Saving**

- ⬜ Sessions, single layers and single patches saved; save slots plus a LAST slot

## Later

- **Bluetooth MIDI** — *unlikely.* The radio can only be driven through JieLi's closed library, which does not fit Xebec's GPL-3.0-only licence.
