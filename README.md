# VOCAL DMG 1.3.0

Vocal glitch multi-effects VST3 by polS Software for 64-bit Windows 10 and 11.

## Install

Extract the archive and copy the entire `VOCAL DMG.vst3` folder to
`C:\Program Files\Common Files\VST3`. Rescan VST3 plugins in your DAW,
then add VOCAL DMG to a vocal track as an insert effect.

## Use

- Start with a factory preset or turn on individual effects in the five-module row.
- Use the arrows to step through factory presets. The top power icon controls global bypass.
- Resize from the bottom-right corner. The star's long tip shows each knob's position.
- CRUSHER uses one On/Off button and one Mix knob. Factory presets supply the underlying crushing settings.
- Drag a module header to change the left-to-right signal order. The order is
  saved in your DAW project and user presets. Factory presets keep the current order.
  Older projects with separated Bitcrush and Sample Rate stages put the combined module
  at the earlier of the two positions.
- Low bit depths use level-bounded quantization to keep quiet vocals from jumping
  in level. Use the Bitcrusher Mix to blend in more of the original vocal.
- Random in REPEATER and REVERSER checks the selected note grid for a 25% trigger chance per step. Each module's Mix sets the blend of its triggered audio. REVERSER uses one-shot playback while Random is on.
- Beat Repeat captures the most recent full fragment. Click Trigger again to release it.
- Reverser in Continuous mode buffers and reverses consecutive fragments. In Trigger
  mode, click Trigger to capture the next fragment and play it backward once.
- Pitch Shifter and Reverser add latency when enabled. Reverse latency follows the
  current tempo and fragment length. Shorter fragments reduce monitoring delay.
- Input drives the wet effects chain. Output adjusts the blended signal. The HP and LP
  sliders engage when moved off their default positions. Double-click either slider to reset it.
- The preset dropdown contains Factory, User, and Reset to Default. User presets saved in
  Documents/VOCAL DMG Presets appear in User. Use Load from File to open a preset elsewhere.
- Double-click a knob or filter slider to reset it. Right-click to enter a value.

## Build from source

Open a terminal in the Source directory with CMake and Visual Studio 2022 C++ tools
installed. The first configure downloads JUCE 8.0.12. For an offline build, add
`-DJUCE_DIR=C:/path/to/JUCE` to the first command.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target VocalDMG_VST3
```

The bundle is under `build/VocalDMG_artefacts/Release/VST3/`.

