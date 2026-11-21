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

## Notes

- The Xbox 360 port uses the xsdk 2D renderer under `drivers/xsdk/`.
- The xenia test title used title ID `52584401`.
- The game project and its `build_x360.sh` packaging helper are kept separately from this engine repository.
