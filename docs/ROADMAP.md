# Xebec roadmap

Where Xebec is going, in order. Everything is built from scratch on the
chip's own SDK.

## Milestones

1. **Bare boot** — the display comes up and shows one colour. *Runs in fm1-emu; not yet on hardware.*
2. **Update path** — Xebec packages itself as an FM-1 update, installs, overwrites itself and returns to the official firmware safely. *First milestone that goes onto a real FM-1.*
3. **Panel** — keys, buttons, encoders, MASTER pot and LEDs read and driven.
4. **Audio out** — a single sine on the audio output.
5. **USB and MIDI** — USB-MIDI, USB audio and the TRS MIDI input.
6. **Dual core** — the second core renders voices; Xebec falls back to one core on units where the second core is unreliable.
7. **Sound engine and interface** — what Xebec actually plays, on top.

## Later

- **Bluetooth MIDI** — *unlikely.* The radio can only be driven through JieLi's closed library, which does not fit Xebec's GPL-3.0-only licence.
