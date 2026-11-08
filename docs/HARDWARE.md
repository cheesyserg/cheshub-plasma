# Hardware reference

What this module sends, condensed from the ChesHub web drivers, which are the
source of truth:

- TECKNET TK-GM012: `cheshub/devices/tecknet-tk-gm012/docs/PROTOCOL.md`,
  `REGISTERS.md`, `driver.js`
- iLovbee B75: `cheshub/devices/ilovbee-b75/docs/PROTOCOL.md`, `driver.js`,
  `keymap.js`

The byte layouts live in `src/protocol/` and are checked against the captures
from those documents by `tests/`.

## TECKNET TK-GM012 (gaming mouse)

| | |
|---|---|
| USB / 2.4 GHz | `248A:5D2E` / `249A:5C2F` |
| Channel | interface 2, usage page `0xFF01`, usage `0x0010`, report id 0 |
| Frame | 33 bytes out: `00 cmd 00 dir len payload[27] checksum`; 32 bytes back: `cmd status …` |
| Checksum | sum of bytes 5..31, low byte |
| Ack rule | reply must echo the command with status 0; wait 600 ms, resend once, wait 1500 ms |

### Features

| Feature | Range | Register | Read |
|---|---|---|---|
| Report rate | 125 / 250 / 500 / 1000 Hz, wire `1000/hz` | `0x02` | `0x12` |
| DPI stages | 6 stages, 1–6 in use, active stage, 50–24000 DPI | `0x03` (`index<<4 \| count`, 6 × X/Y codes) | `0x13` |
| DPI codec | ≤10000: `/50`; <13000: `200+(d-10000)/100`; ≥13000: `221+(d-13000)/1000` | | |
| Stage colours | 6 × RGB (the DPI indicator is the only LED) | `0x04` | `0x14` |
| Sleep | 1, 2, 5, 6, 10, 15, 20, 30 min, wire `minutes × 6` | `0x07[5]` | `0x17` |
| Move wake-up | on/off | `0x07[6]` | `0x17` |
| Buttons | 6 buttons × 3-byte records, `key_value` order | `0x09` | `0x11` |
| Factory reset | | `0x0F 01 FF` | |

Not exposed, because the vendor descriptor turns them off
(`menu light="0"`, `show_lod="0"`, `show_sensor="0"`, `show_respondtime="0"`):
RGB lighting, lift-off distance, sensor options, debounce.

Not implemented here yet: the six-event macro editor (`0x08`). A macro bound
by the web studio or the vendor tool shows as "Macro (set by another tool)"
and is written back unchanged.

**Codec quirk:** 12100–12900 DPI encode to codes 221–229, which the same codec
reads as 13000–21000 DPI, so 12500 is read back as 17000. The module skips
that range; the slider jumps from 12000 to 13000.

## iLovbee B75 (75% keyboard)

| | |
|---|---|
| Wired | `258A:01E7`, vendor collection (`0xFF00`/1 or `0xFF02`/1–2) with a 0x208-byte feature report |
| Wired frame | `rid cmd sub 0 packets index len_lo len_hi data[512]`, 25 ms before each packet |
| Receiver | `258A:01BB`, `0xFF02`/2, report `0x13`, 20 bytes: `13 cmd total index sub<<4\|len data[14] checksum` |
| Bluetooth | `046D:B35A`: vendor channel only sends a 1-byte status report, **cannot be configured** |

### Features

| Feature | Range | Where |
|---|---|---|
| Effect | 17 effects (Steady … Blossoming, Custom, Off) | config `0x21` |
| Brightness | 5 levels, wire 0/5/10/15/20, per effect | config `0x23`, `0x53 + 2·id` |
| Speed | 0–4, per effect | config `0x24`, high nibble of `0x54 + 2·id` |
| Colour | palette slot 0–6 or colourful (7), per effect | config `0x22`, low nibble of `0x54 + 2·id` |
| Palette | 7 colours per effect | colour table `0x0A`, 21 bytes per effect id |
| Debounce | Office `0x00` / Gaming `0x04` | config `0x02`, `0x03` |
| Sleep | 5 / 10 / 20 / 30 min / never, 30 s units | config `0x09` |
| Key mapping | base layer, 126 × 4-byte codes | matrix `0x03`, sub = layer 0 |
| Factory reset | config byte 37 = 1, plus factory matrix / palette / per-key colours | |

The config block (`0x84` read, `0x04` write, signature `5A A5` at
`0x7E`) is read back and only the bytes the vendor tool owns are patched.
Key assignments and palettes cannot be read from the keyboard, so they are
remembered in `~/.config/cheshub-plasma/ilovbee-b75.conf`.

Not implemented here yet: macros (`0x05`) and painting per-key colours for the
Custom effect (`0x06`). Use the web studio for those; this module does not
overwrite them except on factory reset.
