# Blackface Compressor (VST3)

Class-A FET limiting amplifier in the "blackface" 1176 tradition. Built on your IEM-1176 DSP core
(JFET gain cell, class-AB stage, output transformer, 8x oversampling), with true ratios, soft knee,
stereo link, external sidechain, VU/GR meter ballistics and a VSTGUI editor.

## Controls
INPUT (0..+40 dB, drives a fixed -3 dBFS threshold) / OUTPUT (-24..+12 dB) / ATTACK (7 = 20 us ... 1 = 800 us) /
RELEASE (7 = 50 ms ... 1 = 1.1 s) / RATIO 4,8,12,20 + ALL (all-buttons mode) / METER GR, +4, +8, OFF
(0 VU = -18 / -14 dBFS) / MIX / SC HPF / COLOR / DETECTOR Int, Link (stereo), Ext (aux sidechain bus) /
OVERSAMPLE 1x-8x / PRESET (16 factory) / POWER (= host bypass). Drag knobs vertically, Shift = fine,
double-click = default, wheel works.

## Build
1. Download the Steinberg VST3 SDK (3.7.8+), unzip it.
2. `cmake -S . -B build -DVST3_SDK_ROOT=/path/to/VST3_SDK -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build build --config Release` -> `build/VST3/Release/Blackface.vst3`

Linux needs the VSTGUI deps (libxcb, cairo, pango, fontconfig). Windows: Visual Studio 2019+.

## Tests (no SDK needed)
`./tests/run_tests.sh` - 35 engine checks (ratios, attack/release, link, ext key, bypass, meters, CPU)
plus a 192-combination stability sweep.

## Regenerating art
`python3 tools/make_assets.py --art <folder with 13739.jpg, 13740.jpg>` then `python3 tools/preview.py`.
Cropped tomb-painting plates are cached in tools/art.

## Status
DSP, parameters, art and tests were run here. The VST3/VSTGUI glue (processor, controller, views, CMake)
was written against the SDK API but NOT compiled here (no SDK offline) - expect to fix a few compile
errors on first build and tell me what the compiler says.
