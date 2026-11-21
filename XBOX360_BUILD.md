# Xbox 360 Build, Pack & Run — Guide

Files involved:

| File | What it is |
|---|---|
| `bin/godot_xbox360.xbox360.opt.ppc` | Release export template (engine only, no editor) |
| `*.pck` | Your game data (exported from the X11 editor) |
| `game.xex` | Final executable: template + PCK, ready to run |

The tools binary (`*.xbox360.tools.ppc`) is **not** meant for releasing games — it contains the editor. Use the release template above.

## 1. Build the release template

```bash
scons platform=xbox360 target=release tools=no -j$(nproc)
```

## 2. Pack the ELF into an XEX

```bash
XexTool pack bin/godot_xbox360.xbox360.opt.ppc -o game.xex
```

**Do not pass `--base`.** XexTool rejects a `--base` that differs from the ELF's linked base. The linker script (`platform/xbox360/app.lds`) now emits a skipped `.note.headroom` section so the ELF's load base is `0x80000000` and `.text` sits at RVA `0x1000` (a fresh PE image reserves the first 4 KiB for headers; before the fix, `.text` was at RVA `0x0` and XexTool aborted with *"section .text at RVA 0x0 overlaps the PE headers"*).

`.data` is also aligned to a 64 KiB page so writable data never shares a page with code (XEX page descriptors allow one protection flag per page).

## 3. Embed your game (PCK) in the XEX

XexTool only packs allocatable ELF sections, so an appended PCK would be lost — and a bare `cat game.pck >> game.xex` would still not be detected, because Godot looks for a 12-byte footer at EOF.

Godot's self-contained detection expects the file to end with:

```
[PCK bytes][uint64 pck_size][uint32 0x43504447 ('GDPC')]
```

Append it with:

```bash
python3 - <<'EOF'
import struct
with open("game.pck", "rb") as f:
    pck = f.read()
footer = struct.pack("<Q", len(pck)) + struct.pack("<I", 0x43504447)
with open("game.xex", "ab") as f:
    f.write(pck)
    f.write(footer)
EOF
```

On boot, Godot opens its own executable (`exec_path` from `argv[0]`), sees the footer at EOF, and loads the PCK directly from memory/storage.

**Fallback:** if the loader passes a null/relative `argv[0]`, the embedded check fails, but Godot also looks for `<exec_basename>.pck` next to the executable (`game.pck` next to `game.xex`) — the export output is already named that way, so copying it alongside works as a backup.

## 4. Running it

**Recommended storage:** FAT32 USB/HDD with readable files (libxenon's `fopen` goes through the FAT/USB drivers — the Unix `FileAccess`/`DirAccess` drivers are now registered for the 360 platform; they were silently missing before).

- Copy `game.xex` (plus `game.pck` as fallback) to the drive.
- Launch through a JTAG/RGH dashboard (Aurora / FSD) or a homebrew XEX loader.
- Expected output goes to the console/UART via `printf`.

**Status of the port:** it boots, polls the controller (see `XBOX360_CONTROLLER_INPUT.md`), loads PCKs — but `can_draw()` returns `false` and no GPU rasterizer is wired up, so the game loop runs without rendering anything to the screen yet. Getting pixels on screen requires implementing the video/raster path (Xenon Xenos / XDK-style GPU interface or similar).

**Xenia:** libxenon apps use privileged PowerPC instructions (`mtspr`/`mfspr`) and homebrew APIs Xenia doesn't implement — expect failures there regardless of the XEX wrapper. Real hardware (or a loader that supports libxenon ELFs) is the realistic target for now.

## 5. Housekeeping

- `game.elf` (template + PCK concat) is obsolete — XexTool ignores trailing bytes; don't use it.
- `game.ppc` in the game folder was the old **tools** (editor) build; games should ship with the release template.
- Backups kept: `game.elf.pre-fix-backup`, `game.pck.bak` — safe to delete once you trust the new flow.

## 6. The xsdk build (preferred going forward)

The libxenon build above works as a file system exercise but can't render. The xsdk-based platform (`platform/xbox360xsdk/`) builds the same Godot engine against xsdk (GCC 15, `powerpc-xsdk-elf`, `-mcpu=cell`) with real kernel/XAM syscalls, XAM input, file IO through xsdk's devices, and the xsdk runtime.

```bash
# Build (XSDK_TOOLCHAIN=~/xsdk-gcc and XSDK_ROOT=~/Downloads/xsdk-main are the defaults; override via env)
scons platform=xbox360xsdk target=release tools=no -j$(nproc)

# Pack into a XEX (title id/name/version are embedded)
python3 ~/Downloads/xsdk-main/tools/elf2xex.py \
    bin/godot_xbox360xsdk.xbox360xsdk.opt.ppc game.xex \
    --title-id 52584401 --title-name "Godot" --version 1.0.0.0

# Embed the game PCK (Godot self-contained footer)
python3 - <<'EOF'
import struct
with open("game.pck", "rb") as f:
    pck = f.read()
with open("game.xex", "ab") as f:
    f.write(pck)
    f.write(struct.pack("<Q", len(pck)) + struct.pack("<I", 0x43504447))
EOF
```

Notes:
- Rebuild needs `scons platform=xbox360xsdk ...` — not the libxenon platform.
- The xsdk log is written next to the `.xex` as `<title-name>.log` (e.g. `game-xsdk.log`) — that's where boot progress appears on the console.
- `dbgprint.txt` next to the `.xex` turns on DbgPrint-per-line logging (slow on console; useful on Xenia).
- Status: engine boots (dummy rasterizer, `can_draw() == false`) — no pixels yet. Controller works via `XamInputGetState`. Menu/UI rendering is the 2D rasterizer milestone (not built yet).
- Current artifact: `~/new-game-project/game^2/game-xsdk.xex` (engine + PCK, ~51 MB). `game.xex` in the same folder is the older libxenon variant.
