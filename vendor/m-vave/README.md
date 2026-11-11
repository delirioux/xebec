# M-VAVE's OTA loader

`ota.bin` is M-VAVE's official update loader for the FM-1 (`usb_hid_ota.bin`), byte for byte as M-VAVE publishes it. Xebec does not change it.

| | |
| --- | --- |
| Source | [m-vave/fm1_open](https://github.com/m-vave/fm1_open), `sdk/tools/pack/ota.bin`, commit `4c7496daac04a95ff10d1afdcc8f9ead2dbabc41` |
| Copyright | © 2026 珠海笙科智能科技有限公司 (M-VAVE) |
| Licence | Apache License 2.0 ([LICENSE-Apache-2.0.txt](LICENSE-Apache-2.0.txt)), as stated in that repository's `THIRD_PARTY.md` |
| SHA-256 | `ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541` |
| Size | 19969 bytes: a 32-byte file head, then the loader (19937 bytes, CRC-16 `0x5881`) |

## How Xebec uses it

Xebec carries the file inside its own image. To update, Xebec writes the loader into free flash and restarts into it. M-VAVE's boot loader then runs it from RAM, and the loader takes the new firmware over USB, as with the official firmware.

The build (`tools/gen_loader.py`) refuses the file unless its SHA-256 and both CRCs match the values above.

This loader is a separate program that Xebec stores and starts. It is not linked into Xebec, so Xebec stays under GPL-3.0-only, and the loader keeps its Apache-2.0 licence.
