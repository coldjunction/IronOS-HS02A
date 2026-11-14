#!/usr/bin/env bash
# Builds the browser demo: the firmware's own GUI sources plus demo_host.cpp, compiled to
# WebAssembly with zig's C++ compiler, then packed into one HTML file (docs/demo/index.html).
#
# Needs: Python 3 with the "ziglang" package (pip install ziglang), and one firmware build
# beforehand (cd source && make model=HS02 firmware-EN), which generates Core/Gen/Translation.EN.cpp.
# Usage, from the repository root:  tools/hs02-card-demo/build.sh
# PYTHON selects the interpreter (default: python3).
set -u
PY="${PYTHON:-python3}"
D="$(cd "$(dirname "$0")" && pwd)"; R="$(cd "$D/../.." && pwd)"; src="$R/source"
mkdir -p "$D/out"
DEFS="-DMODEL_HS02 -DHS02_THEME_CARD -DCANT_DIRECT_READ_SETTINGS"
INC="-I$D/shim -I$src/Core/BSP/Fnirsi -I$src/Core/BSP -I$src/Core/Inc -I$src/Core/Drivers -I$src/Core/Threads -I$src/Core/Threads/UI -I$src/Core/Threads/UI/logic -I$src/Core/Threads/UI/logic/utils -I$src/Core/Threads/UI/drawing -I$src/Core/Threads/UI/drawing/color_160x80 -I$src/Core/Drivers/usb-pd/include -I$src/Core/brieflz"
FLAGS="-target wasm32-wasi -std=gnu++17 -O2 -fshort-wchar -fshort-enums -w -fpermissive -fno-exceptions -fno-rtti"
FILES=( "$src/Core/Drivers/Display.cpp" "$src/Core/Drivers/LCD.cpp" "$src/Core/Drivers/Buttons.cpp" "$src/Core/Drivers/BootLogo.cpp"
        "$src/Core/Src/settingsGUI.cpp" "$src/Core/Src/ScrollMessage.cpp" "$src/Core/Src/Settings.cpp" "$src/Core/Src/Translation.cpp"
        "$src/Core/LangSupport/lang_single.cpp" "$src/Core/Threads/GUIThread.cpp" "$src/Core/Gen/Translation.EN.cpp" "$D/demo_host.cpp" )
for f in "$src"/Core/Threads/UI/logic/*.cpp "$src"/Core/Threads/UI/logic/utils/*.cpp "$src"/Core/Threads/UI/drawing/color_160x80/*.cpp; do
  case "$f" in *USBPDDebug*) ;; *) FILES+=("$f");; esac
done
fail=0; objs=()
for f in "${FILES[@]}"; do
  o="$D/out/$(basename "${f%.*}").o"; objs+=("$o")
  $PY -m ziglang c++ $FLAGS $DEFS $INC -c "$f" -o "$o" 2> "$D/out/err.txt" || { echo "FAILED: $(basename "$f")"; grep -m4 "error" "$D/out/err.txt" | cut -c1-300; fail=1; }
done
echo "compiled ${#FILES[@]} files"
[ $fail = 0 ] || exit 1
EXP=""; for e in demo_init demo_frame demo_publish demo_inputs demo_panel demo_state demo_flash demo_flash_size __stack_pointer; do EXP="$EXP -Wl,--export=$e"; done
$PY -m ziglang c++ -target wasm32-wasi -O2 -mexec-model=reactor -fno-exceptions "${objs[@]}" -Wl,--strip-all $EXP -o "$D/out/demo.wasm" 2> "$D/out/link.txt" || { echo "link failed"; grep -o "undefined symbol: .*" "$D/out/link.txt" | sort -u | head -40; exit 1; }
mkdir -p "$R/docs/demo"
$PY "$D/pack.py" "$D/page.html" "$D/out/demo.wasm" "$R/docs/demo/index.html"
