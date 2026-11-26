#!/usr/bin/env bash
# Export this Godot project, append its PCK to the Xbox 360 XEX, and launch Xenia.
#
# Usage:
#   ./build_x360.sh [PPC] [PCK] [XEX]
#   ./build_x360.sh --ppc PATH --pck PATH --xex PATH
#
# Arguments:
#   --ppc PATH  input ELF/PPC to convert (positional 1)
#   --pck PATH  existing PCK to append (positional 2); skips the Godot export
#   --xex PATH  output XEX to create (positional 3)
#
# If --pck is omitted, the PCK is exported from this project. With --xex but no
# --pck, the exported PCK is placed next to the XEX. With --pck but no --xex,
# the XEX is written next to the PCK.
#
# Environment:
#   BUILD_ENGINE=0   skip the engine rebuild (game-only changes)
#   CAPTURE=1        also save a 1280x720 capture next to the XEX
#   LAUNCH=0         build/package only, do not touch Xenia
#
# Defaults:
#   PPC: <engine>/bin/godot_xbox360xsdk.xbox360xsdk.opt.ppc
#   PCK: ./xex/game.pck
#   XEX: ./xex/game.xex

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GODOT_SRC="/home/dwarfthe3/Downloads/godotX-360-main"
TOOLS_BIN="$GODOT_SRC/bin/godot.x11.tools.64"
DEFAULT_PPC="$GODOT_SRC/bin/godot_xbox360xsdk.xbox360xsdk.opt.ppc"
XSDK_ROOT="/home/dwarfthe3/Downloads/xsdk-main"
ELF2XEX="$XSDK_ROOT/tools/elf2xex.py"
XENIA="/home/dwarfthe3/xenia/xenia.AppImage"
DEFAULT_OUT_DIR="$PROJECT_DIR/xex"

TITLE_ID="52584401"
TITLE_NAME="Godot"
VERSION="1.0.0.0"

BUILD_ENGINE="${BUILD_ENGINE:-1}"
CAPTURE="${CAPTURE:-0}"
LAUNCH="${LAUNCH:-1}"

die() { echo "ERROR: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "missing command: $1"; }
with_ext() {
	local path="$1" ext="$2"
	if [[ "$path" == *.* ]]; then
		printf '%s.%s' "${path%.*}" "$ext"
	else
		printf '%s.%s' "$path" "$ext"
	fi
}

usage() {
	sed -n '2,24p' "${BASH_SOURCE[0]}"
}

PPC_ARG=""
PCK_ARG=""
XEX_ARG=""
POSITIONAL=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--ppc)
			[[ $# -ge 2 ]] || die "--ppc needs a path"
			PPC_ARG="$2"; shift 2 ;;
		--pck)
			[[ $# -ge 2 ]] || die "--pck needs a path"
			PCK_ARG="$2"; shift 2 ;;
		--xex)
			[[ $# -ge 2 ]] || die "--xex needs a path"
			XEX_ARG="$2"; shift 2 ;;
		--ppc=*) PPC_ARG="${1#*=}"; shift ;;
		--pck=*) PCK_ARG="${1#*=}"; shift ;;
		--xex=*) XEX_ARG="${1#*=}"; shift ;;
		-h|--help) usage; exit 0 ;;
		--) shift; POSITIONAL+=("$@"); break ;;
		-*) die "unknown option: $1 (try --help)" ;;
		*) POSITIONAL+=("$1"); shift ;;
	esac
done

[[ ${#POSITIONAL[@]} -le 3 ]] || die "too many positional arguments (PPC PCK XEX)"
[[ -n "$PPC_ARG" ]] || PPC_ARG="${POSITIONAL[0]:-}"
[[ -n "$PCK_ARG" ]] || PCK_ARG="${POSITIONAL[1]:-}"
[[ -n "$XEX_ARG" ]] || XEX_ARG="${POSITIONAL[2]:-}"

PPC="${PPC_ARG:-$DEFAULT_PPC}"
PCK_EXPLICIT=0
XEX_EXPLICIT=0
PCK=""
XEX=""

if [[ -n "$PCK_ARG" ]]; then
	PCK="$PCK_ARG"
	PCK_EXPLICIT=1
fi
if [[ -n "$XEX_ARG" ]]; then
	XEX="$XEX_ARG"
	XEX_EXPLICIT=1
fi

if [[ "$XEX_EXPLICIT" == "1" && "$PCK_EXPLICIT" == "0" ]]; then
	PCK="$(with_ext "$XEX" pck)"
elif [[ "$PCK_EXPLICIT" == "1" && "$XEX_EXPLICIT" == "0" ]]; then
	XEX="$(with_ext "$PCK" xex)"
elif [[ "$PCK_EXPLICIT" == "0" && "$XEX_EXPLICIT" == "0" ]]; then
	PCK="$DEFAULT_OUT_DIR/game.pck"
	XEX="$DEFAULT_OUT_DIR/game.xex"
fi

OUT_DIR="$(dirname "$XEX")"
LOG="$OUT_DIR/xenia.log"
CAPTURE_XWD="$OUT_DIR/capture.xwd"
CAPTURE_PNG="$OUT_DIR/capture.png"

need python3
[[ -f "$ELF2XEX" ]] || die "missing elf2xex.py: $ELF2XEX"

if [[ "$PCK_EXPLICIT" != "1" ]]; then
	[[ -f "$TOOLS_BIN" ]] || die "missing Godot tools binary: $TOOLS_BIN"
fi

if [[ "$BUILD_ENGINE" != "1" || "$PPC" != "$DEFAULT_PPC" ]]; then
	[[ -f "$PPC" ]] || die "missing Xbox 360 ELF: $PPC (run the engine build once)"
fi

mkdir -p "$OUT_DIR"
rm -f "$XEX" "$CAPTURE_XWD" "$CAPTURE_PNG"
if [[ "$PCK_EXPLICIT" != "1" ]]; then
	rm -f "$PCK"
fi

if [[ "$BUILD_ENGINE" == "1" ]]; then
	need scons
	echo "[1/5] Building Xbox 360 engine..."
	(cd "$GODOT_SRC" && scons platform=xbox360xsdk target=release tools=no -j4)
else
	echo "[1/5] Skipping engine build (BUILD_ENGINE=0)"
fi
[[ -f "$PPC" ]] || die "missing Xbox 360 ELF after build: $PPC"

if [[ "$PCK_EXPLICIT" == "1" ]]; then
	echo "[2/5] Using PCK: $PCK (skipping export)"
else
	echo "[2/5] Exporting PCK from $PROJECT_DIR to $PCK ..."
	"$TOOLS_BIN" --headless --path "$PROJECT_DIR" --export-pack "Linux/X11" "$PCK"
fi
[[ -s "$PCK" ]] || die "PCK is missing or empty: $PCK"

echo "[3/5] Converting $PPC -> $XEX and appending PCK..."
python3 "$ELF2XEX" "$PPC" "$XEX" \
	--title-id "$TITLE_ID" \
	--title-name "$TITLE_NAME" \
	--version "$VERSION"

python3 - "$PCK" "$XEX" <<'PY'
import struct
import sys
from pathlib import Path

pck_path, xex_path = sys.argv[1], sys.argv[2]
pck = Path(pck_path).read_bytes()
with open(xex_path, "ab") as f:
	f.write(pck)
	f.write(struct.pack("<Q", len(pck)))
	f.write(struct.pack("<I", 0x43504447))
print("  PCK: %d bytes" % len(pck))
print("  XEX: %d bytes" % Path(xex_path).stat().st_size)
PY

WID=""
if [[ "$LAUNCH" == "1" ]]; then
	need xwininfo
	[[ -x "$XENIA" ]] || die "missing Xenia: $XENIA"

	echo "[4/5] Stopping existing Xenia..."
	ps -eo pid,args | awk -v p="$XENIA" '$2 == p {print $1}' | xargs -r kill -9 || true
	sleep 1

	echo "[5/5] Launching Xenia..."
	setsid nohup "$XENIA" "$XEX" > "$LOG" 2>&1 < /dev/null &
	echo "  Xenia PID: $!"

	for _ in $(seq 1 30); do
		WID="$(xwininfo -root -tree 2>/dev/null | awk '/Xenia-canary/ && /Godot/ && /xenia.AppImage/ {print $1; exit}')"
		[[ -n "$WID" ]] && break
		sleep 1
	done

	if [[ -n "$WID" && -x /tmp/movewin ]]; then
		/tmp/movewin "$WID" 100 50
	fi

	if [[ "$CAPTURE" == "1" ]]; then
		[[ -n "$WID" ]] || die "could not find Xenia window for capture"
		need xwd
		need convert
		for _ in $(seq 1 15); do
			xwd -id "$WID" -silent -out "$CAPTURE_XWD"
			convert "$CAPTURE_XWD" -crop 1280x720+0+27 +repage "$CAPTURE_PNG"
			if python3 - "$CAPTURE_PNG" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
sys.exit(0 if im.getpixel((640, 360)) != (0, 0, 0) else 1)
PY
			then
				break
			fi
			sleep 1
		done
		# Give the canvas a few extra frames so all rects/styles are present.
		sleep 3
		xwd -id "$WID" -silent -out "$CAPTURE_XWD"
		convert "$CAPTURE_XWD" -crop 1280x720+0+27 +repage "$CAPTURE_PNG"
		echo "  Capture: $CAPTURE_PNG"
	fi
else
	echo "[4/5] Skipping Xenia shutdown (LAUNCH=0)"
	echo "[5/5] Skipping Xenia launch (LAUNCH=0)"
fi

echo
echo "Done."
echo "  PPC: $PPC"
echo "  PCK: $PCK"
echo "  XEX: $XEX"
echo "  LOG: $LOG"
[[ -n "$WID" ]] && echo "  Xenia window: $WID"
