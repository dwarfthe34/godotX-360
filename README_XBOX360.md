## Build

Requires SCons, the usual Godot 3.6.3 build dependencies, and an Xbox 360 xsdk installation available separately.

```bash
scons platform=xbox360xsdk target=release tools=no -j4
```

The resulting ELF is written to:

```text
bin/godot_xbox360xsdk.xbox360xsdk.opt.ppc
```

## Package as an XEX

Convert the ELF with the xsdk `elf2xex.py` tool:

```bash
python3 <xsdk>/tools/elf2xex.py \
  bin/godot_xbox360xsdk.xbox360xsdk.opt.ppc \
  game.xex \
  --title-id 52584401 \
  --title-name "Godot" \
  --version 1.0.0.0
```

Then append an exported PCK and its footer:

```python
import struct
from pathlib import Path

pck = Path("game.pck").read_bytes()
with open("game.xex", "ab") as f:
    f.write(pck)
    f.write(struct.pack("<Q", len(pck)))
    f.write(struct.pack("<I", 0x43504447))  # "GDPC"
```

## Packaging helper

A copy of the test game project and its `build_x360.sh` is included at the repository root. The script performs the export, XEX conversion, and PCK append above. It takes the input PPC, input PCK, and output XEX as arguments:

```bash
./build_x360.sh [PPC] [PCK] [XEX]
./build_x360.sh --ppc PATH --pck PATH --xex PATH
```

- `--ppc` / positional 1: ELF/PPC to convert.
- `--pck` / positional 2: an existing PCK to append. Supplying it skips the Godot export and never deletes the input PCK.
- `--xex` / positional 3: XEX to create.
- If `--pck` is omitted, the PCK is exported from the game project.
- With `--xex` but no `--pck`, the exported PCK is placed next to the XEX.
- With `--pck` but no `--xex`, the XEX is written next to the PCK.
- `xenia.log` and optional captures are written next to the XEX.

Environment flags:

```bash
BUILD_ENGINE=0   # skip the engine rebuild (game-only changes)
CAPTURE=1        # save a 1280x720 capture next to the XEX
LAUNCH=0         # package only; do not touch Xenia
```

Examples:

```bash
BUILD_ENGINE=0 LAUNCH=0 ./build_x360.sh \
  --pck /path/to/game.pck \
  --xex /path/to/output.xex
```

```bash
BUILD_ENGINE=0 CAPTURE=1 ./build_x360.sh \
  /path/to/godot_xbox360xsdk.xbox360xsdk.opt.ppc \
  /path/to/game.pck \
  /path/to/output.xex
```

## Notes

- The Xbox 360 port uses the xsdk 2D renderer under `drivers/xsdk/`.
- The xenia test title used title ID `52584401`.
- The included game project is a minimal demo; separate game projects can keep their own copy of `build_x360.sh`.
