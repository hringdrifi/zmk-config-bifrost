# ZMK Config for Bifrost

ZMK firmware configuration for the Bifrost BLE split keyboard, generated with
**Smidr**. On this branch, **both halves are split peripherals**. The Liki
Android app is the split central, manages layers and key bindings, and sends
keyboard and pointer input to the paired PC.

## Repository Structure

- `config/bifrost.keymap`: Shared reference keymap for the Android central.
  ZMK does not execute these bindings on a peripheral. Keep the Liki keymap in
  sync when changing it.
- `config/bifrost_left.keymap` and `config/bifrost_right.keymap`: Per-board entry
  points that include the shared keymap.
- `boards/hringbord/bifrost.dtsi`: Matrix transform shared by both halves.
- `boards/hringbord/bifrost_left/`: Left peripheral board definition.
- `boards/hringbord/bifrost_right/`: Right peripheral board definition and
  PMW3610 pointing-device configuration.
- `build.yaml`: GitHub Actions build matrix for both halves.

## Build and Flash

The build matrix produces `bifrost_left` and `bifrost_right` UF2 firmware.
Commit and push the branch, download both artifacts from the **Build ZMK
firmware** GitHub Actions run, and flash the corresponding UF2 on each half.
Both builds target the application partition at `0x26000` and leave the
Adafruit nRF52 UF2 bootloader intact.

Existing bonds to the former left central must be cleared before pairing both
halves with Liki. A split peripheral does not connect directly to a PC and
does not expose a ZMK Studio interface. Liki must implement the keymap and
layer logic; the `.keymap` file here is a reference for matching positions.

## Matrix and Trackball

Both boards use the 49-position transform in `bifrost.dtsi`. The right board
applies `col-offset = <24>`, so the two halves report distinct global key
positions. The right PMW3610 sends raw input events through `zmk,input-split`.
Liki maps movement to pointer output and applies layer-dependent scrolling.

## License

This repository's original content is licensed under the [MIT License](LICENSE).
Copyright (c) 2026 hringdrifi.
