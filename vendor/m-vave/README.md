# M-VAVE's files

Xebec carries a few of M-VAVE's files for the FM-1, byte for byte as M-VAVE publishes them, under the Apache License 2.0 ([LICENSE-Apache-2.0.txt](LICENSE-Apache-2.0.txt)). Xebec does not change them.

## The OTA loader

`ota.bin` is M-VAVE's official update loader for the FM-1 (`usb_hid_ota.bin`).

| | |
| --- | --- |
| Source | [m-vave/fm1_open](https://github.com/m-vave/fm1_open), `sdk/tools/pack/ota.bin`, commit `4c7496daac04a95ff10d1afdcc8f9ead2dbabc41` |
| Copyright | © 2026 珠海笙科智能科技有限公司 (M-VAVE) |
| Licence | Apache License 2.0 ([LICENSE-Apache-2.0.txt](LICENSE-Apache-2.0.txt)), as stated in that repository's `THIRD_PARTY.md` |
| SHA-256 | `ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541` |
| Size | 19969 bytes: a 32-byte file head, then the loader (19937 bytes, CRC-16 `0x5881`) |

### How Xebec uses it

Xebec carries the file inside its own image. To update, Xebec writes the loader into free flash and restarts into it. M-VAVE's boot loader then runs it from RAM, and the loader takes the new firmware over USB, as with the official firmware.

The build (`tools/gen_loader.py`) refuses the file unless its SHA-256 and both CRCs match the values above.

This loader is a separate program that Xebec stores and starts. It is not linked into Xebec, so Xebec stays under GPL-3.0-only, and the loader keeps its Apache-2.0 licence.

## The stock package parts

`stock/` holds the parts of M-VAVE's stock FM-1 update package that Xebec's own package reuses unchanged:

| File | What it is |
| --- | --- |
| `head.bin` | The first 16 KiB of the flash image: M-VAVE's boot loader (`uboot.boot`), its configuration (`isd_config.ini`, with the chip key) and the flash directory |
| `area_head.bin` | The app area's directory: `app.bin`, `cfg_tool.bin` and the flash regions (VM, PRCT, BTIF, USR) |
| `area_tail.bin` | What follows the app slot: `cfg_tool.bin` and the `cfg` directory (EQ table) |

| | |
| --- | --- |
| Source | [m-vave/fm1_open](https://github.com/m-vave/fm1_open), `sdk/firmware/FM-1_015.fwsc`, commit `4c7496daac04a95ff10d1afdcc8f9ead2dbabc41` |
| Copyright | © 2026 珠海笙科智能科技有限公司 (M-VAVE) |
| Licence | Apache License 2.0, as stated in that repository's `THIRD_PARTY.md` |
| SHA-256 of the source | `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a` |

`tools/package.py --extract` cuts these parts out of that file and checks that they rebuild its flash image byte for byte. Every build does this again with the file fetched from M-VAVE's repository. Xebec's package is that same flash image with Xebec in the app slot, and the two CRCs that cover the slot updated. The stock package's factory patch bank and its other extras are left out, so installing Xebec never touches the patches on an FM-1.
