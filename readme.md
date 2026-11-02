# Ploopy Bean — `heckler1` keymap

This repo is a copy of the `keyboards/ploopyco/bean` directory from Ploopy's QMK
fork ([ploopyco/qmk_firmware](https://github.com/ploopyco/qmk_firmware)), with a
custom `heckler1` keymap built around a hand-tuned cursor/scroll acceleration
curve in place of QMK's stock pointing-device acceleration.

- `config.h`, `info.json`, `mcuconf.h`, `post_rules.mk`, `rev1_001/` — the stock
  Ploopy Bean keyboard definition, unmodified except for `config.h`, which adds
  the acceleration tuning below on top of the stock sensor/pin config.
- `keymaps/heckler1/` — the custom keymap. `keymap.c` implements the
  acceleration engine, drag-scroll, and button/chord logic; `rules.mk` enables
  the features it needs.

## Motion engine

Cursor movement and drag-scroll both go through the same acceleration model
(`accel_factor()` in `keymaps/heckler1/keymap.c`):

- **At/above a speed threshold**: quadratic acceleration —
  `factor = 1 + slope * (speed - min_speed)^power * 0.001`, capped at a
  configured limit.
- **Below the threshold**: cursor movement ramps linearly from a configurable
  low-speed sensitivity (well under 1x) up to 1.0x right at the threshold, so
  fine, low-speed movement is deliberately slower and more precise without
  touching the medium/high-speed feel above it. Drag-scroll uses a low-speed
  sensitivity of 1.0, i.e. a flat, unscaled 1:1 passthrough below its
  threshold — no low-speed ramp.

Because the low-speed multiplier can be well under 1.0, naively casting a
scaled 1-count movement to an integer would truncate it to zero every time.
`accumulate_axis()` instead carries the fractional remainder forward in a
float accumulator across polling cycles, so slow movement still eventually
produces real output instead of being silently dropped.

### Tuning (`config.h`)

All values live in `config.h`, not here — treat that file as canonical and
read it directly rather than trusting numbers copied into this doc.

| Define | Effect |
| --- | --- |
| `PLOOPY_DRAGSCROLL_DPI` | Sensor resolution while drag-scrolling |
| `PLOOPY_DRAGSCROLL_INVERT_Y` | Inverts vertical scroll direction |
| `MOUSE_ACCEL_MIN_SPEED` | Counts/report before quadratic cursor acceleration kicks in |
| `MOUSE_ACCEL_SLOPE` | Steepness of the cursor acceleration curve |
| `MOUSE_ACCEL_LIMIT` | Max cursor acceleration multiplier |
| `MOUSE_ACCEL_LOW_SPEED_SENSITIVITY` | Cursor multiplier at rest, ramping to 1.0x at `MOUSE_ACCEL_MIN_SPEED` — lower = slower/more precise fine movement |
| `MOUSE_ACCEL_LOW_SPEED_RAMP_POWER` | Shape of the low-speed ramp: 1.0 is linear, higher holds the multiplier down longer for finer slow/mid-speed control |
| `MOUSE_ACCEL_POWER` | Exponent of the cursor acceleration curve |
| `SCROLL_ACCEL_MIN_SPEED` | Counts/report before scroll acceleration kicks in |
| `SCROLL_ACCEL_SLOPE` | Steepness of the scroll acceleration curve |
| `SCROLL_ACCEL_LIMIT` | Max scroll acceleration multiplier |
| `SCROLL_ACCEL_POWER` | Exponent of the scroll acceleration curve |
| `SCROLL_SENSITIVITY` | Final multiplier converting accelerated counts into scroll steps |
| `POINTING_DEVICE_TASK_THROTTLE_MS` | Sensor poll interval in ms — lower means smoother but more CPU |
| `TAPPING_TERM` | Tap-dance hold/tap resolution window, also used to bound the back/forward chord |

`POINTING_DEVICE_HIRES_SCROLL_ENABLE` is left disabled — it's broken on macOS.

## Scrolling

Drag-scroll is omnidirectional: horizontal and vertical deltas are computed
and emitted independently (no axis locking), so diagonal scrolling works.
Both axes run through the same `accel_factor()` + `accumulate_axis()`
pipeline as cursor movement, using the `SCROLL_ACCEL_*` parameters and a
final `SCROLL_SENSITIVITY` multiplier.

## Buttons

| Position | Action |
| --- | --- |
| Bottom Left | Left Click (`MS_BTN1`) |
| Top Left | Mission Control (`KC_MISSION_CONTROL`) |
| Top Right | Drag-scroll tap-dance (see below) |
| Bottom Right | Right Click (`MS_BTN2`) |

### Drag-scroll tap-dance (Top Right)

- **Single tap**: middle click (`MS_BTN3`). Sent once the tap-dance window
  (`TAPPING_TERM`) resolves, so it lands slightly after the release.
- **Hold**: drag-scroll is active while held, and turns off automatically on
  release.
- **Double-tap**: toggles drag-scroll on/off as a lock (tap again to
  release).

### Back/forward chord (Top Left + Top Right)

Chording Top Left (`TOP_LEFT_KEY`, Mission Control) with the tap-dance key sends back/forward
instead of either key's own action — Top Left held then Top Right pressed
sends forward, Top Right held then Top Left pressed sends back. The chord is
detected directly in `process_record_user()`/`matrix_scan_user()` rather than
via QMK's combo feature, so a solo press of either key isn't delayed waiting
out a combo window — see the comments above `CHORD_TERM` in `keymap.c` for
why.

## `keymaps/heckler1/rules.mk`

- `POINTING_DEVICE_ENABLE`, `MOUSEKEY_ENABLE`, `PLOOPY_DRAGSCROLL_ENABLE`,
  `TAP_DANCE_ENABLE` — required for the above.
- `MOUSE_EXTENDED_REPORT = yes` — 16-bit mouse reports, needed since the
  acceleration math scales counts well past the 8-bit range at high speed
  before the accumulator brings them back down to emitted deltas.
- `VIA_ENABLE = no` — disabled in favor of the custom C logic above.
- `LTO_ENABLE = yes` — smaller firmware.

## Setup

QMK expects this to live at `keyboards/ploopyco/bean` inside a full
`qmk_firmware` checkout — it won't build from a standalone clone. Rather than
copying files back and forth, clone the two repos separately and symlink this
one into place:

```bash
# Ploopy's QMK fork, `ploopyco/bean` branch — needed for the tmag5273axial
# pointing-device driver. Neither upstream QMK nor the fork's default branch
# (master) has the driver or the bean keyboard, so the branch matters.
git clone -b ploopyco/bean git@github.com:ploopyco/qmk_firmware.git ~/qmk_firmware
cd ~/qmk_firmware && git submodule update --init --recursive

git clone git@github.com:heckler1/ploopy-bean-qmk-config.git ~/git/ploopy-bean-qmk-config

# The stock bean directory gets replaced entirely by the symlink
rm -rf ~/qmk_firmware/keyboards/ploopyco/bean
ln -s ~/git/ploopy-bean-qmk-config ~/qmk_firmware/keyboards/ploopyco/bean
```

That branch is based on an older QMK master, so it's best kept as its own
checkout rather than reusing a current upstream `qmk_firmware` clone. If you
already have one, add a worktree instead of switching branches:

```bash
git remote add ploopyco https://github.com/ploopyco/qmk_firmware.git
git fetch ploopyco ploopyco/bean
git worktree add -b bean-build ../qmk_firmware_ploopy_bean ploopyco/ploopyco/bean
cd ../qmk_firmware_ploopy_bean
git submodule update --init --recursive --reference ~/qmk_firmware
```

From then on this repo *is* `keyboards/ploopyco/bean` as far as QMK is
concerned — edit and commit here, and `qmk compile` (run from inside
`~/qmk_firmware`, or after `qmk config user.qmk_home="~/qmk_firmware"`) picks
the changes up directly, with no copying step.

## Compiling

```bash
qmk compile -kb ploopyco/bean/rev1_001 -km heckler1
```

The resulting `ploopyco_bean_rev1_001_heckler1.uf2` is written to the root of
the `qmk_firmware` checkout (and `.build/`); see
[Triggering the bootloader](#triggering-the-bootloader) to flash it.

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools)
and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for
more information. Brand new to QMK? Start with the
[Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Triggering the bootloader

[Do you see those two golden holes in the board](https://ploopy.co/wp-content/uploads/2023/11/boot.jpg)?
Those are called **vias**. They act exactly like a switch does. Right now,
that switch is OFF. However, if you take a paperclip or a pair of metal
tweezers and touch those two vias, the two vias will form an electrical
connection. Effectively, that switch turns ON.

Go ahead and connect the two vias, and then (while the vias are connected)
plug the Bean board into your computer.

The computer should recognise that a mass storage device was just plugged
in. Once this is done, you should be able to drag and drop files onto the
Bean board, as if the board was a USB drive. Feel free to remove the
tweezers or paperclip at this point.

If you want to upload a new firmware file (a `.uf2` file), just drag it into
the folder, and it'll automatically install on the Bean board and restart
itself in normal operating mode. You're done!

**TIP**: If your firmware is in some kind of strange state and uploading new
firmware isn't fixing it, try uploading
[a flash nuke](https://learn.adafruit.com/getting-started-with-raspberry-pi-pico-circuitpython/circuitpython#flash-resetting-uf2-3083182)
to the Bean board before flashing the new firmware. It wipes the memory of
the board completely clean, which can help clear a few types of errors.
