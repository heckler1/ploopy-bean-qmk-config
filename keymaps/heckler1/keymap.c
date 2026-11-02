/* Copyright 2026 Colin Lam (Ploopy Corporation)
 * Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H
#include <math.h>

//void keyboard_post_init_user(void) {
//    // Customise these values to desired behaviour
//    //debug_enable=true;
//    //debug_matrix=true;
//    //debug_keyboard=true;
//    //debug_mouse=true;
//}

// --- STATE VARIABLES ---
bool drag_scroll_on = false;

// High-Res Scroll Accumulators
static float scroll_accum_x = 0.0f;
static float scroll_accum_y = 0.0f;

// High-Res Mouse Movement Accumulators (preserve sub-count motion at low speed)
static float mouse_accum_x = 0.0f;
static float mouse_accum_y = 0.0f;

// Emits the whole-count part of `*accum + delta`, carrying the fractional
// remainder forward in `*accum`. Used for both scroll and mouse movement so
// that scaling by a sensitivity/acceleration factor < 1 doesn't just get
// truncated to 0 every report -- slow motion accumulates until it's big
// enough to emit a real count instead of being silently dropped.
static int32_t accumulate_axis(float *accum, float delta) {
    *accum += delta;
    int32_t whole = (int32_t)*accum;
    *accum -= (float)whole;
    return whole;
}

// Acceleration multiplier for a given 2D speed.
//
// At/above min_speed: quadratic growth, capped at limit.
// Below min_speed: ramps linearly from low_speed_sensitivity (at speed == 0)
// up to 1.0 (at speed == min_speed), so it blends into the quadratic curve
// exactly where that curve already starts at 1.0. Passing 1.0 collapses the
// ramp to a flat 1:1 passthrough below min_speed, reproducing the original
// (non-low-speed-aware) curve used for scroll.
static float accel_factor(float speed, float min_speed, float slope, float limit, float low_speed_sensitivity, float power) {
    if (speed <= 0.0f) {
        return low_speed_sensitivity;
    }
    if (speed < min_speed) {
        return low_speed_sensitivity + (1.0f - low_speed_sensitivity) * (speed / min_speed);
    }
    // Quadratic acceleration formula.
    // Use a smaller divisor (0.001) since we are squaring the speed difference.
    float factor = 1.0f + pow(speed - min_speed, power) * 0.001f * slope;
    return (factor > limit) ? limit : factor;
}

enum
{
    TD_DOUBLE_TAP_SCROLL // Custom Key Name
};

// Define a type containing as many tapdance states as you need
typedef enum
{
    TD_NONE,
    TD_UNKNOWN,
    TD_SINGLE_TAP,
    TD_SINGLE_HOLD,
    TD_DOUBLE_TAP
} td_state_t;

typedef struct {
    bool is_press_action;
    td_state_t state;
} td_tap_t;


/*********************************
* Main layout and movement logic *
**********************************/
// Top left sends macOS Mission Control. Middle click lives on the tap-dance
// key's single tap instead (see double_tap_scroll_finished()).
#define TOP_LEFT_KEY KC_MISSION_CONTROL

// Button order in matrix: bottom left, top left, top right, bottom right
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT( MS_BTN1, TOP_LEFT_KEY, TD(TD_DOUBLE_TAP_SCROLL), MS_BTN2 ),
};

// --- POINTING DEVICE LOGIC ---
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report)
{
#ifdef SENSOR_INVERT_XY
    mouse_report.x = -mouse_report.x;
    mouse_report.y = -mouse_report.y;
#endif

    float speed = sqrt(pow(mouse_report.x, 2) + pow(mouse_report.y, 2));

    // Drop any carried-over fractions once the ball has been quiet for a
    // while, so sensor noise/bias can't pile up across separate movements
    // and surface later as slow cursor drift.
    static uint16_t last_motion_time = 0;
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        last_motion_time = timer_read();
    } else if (timer_elapsed(last_motion_time) >= ACCUM_IDLE_RESET_MS) {
        mouse_accum_x  = 0.0f;
        mouse_accum_y  = 0.0f;
        scroll_accum_x = 0.0f;
        scroll_accum_y = 0.0f;
    }

    if (!drag_scroll_on) {
        // Cursor movement: low_speed_sensitivity < 1.0 makes fine movement
        // slower/more precise; accumulate so that scaling doesn't truncate
        // small counts to 0 (medium/high speed, i.e. speed >= min_speed, is
        // unaffected by the ramp).
        float factor = accel_factor(speed, MOUSE_ACCEL_MIN_SPEED, MOUSE_ACCEL_SLOPE, MOUSE_ACCEL_LIMIT, MOUSE_ACCEL_LOW_SPEED_SENSITIVITY, MOUSE_ACCEL_POWER);
        mouse_report.x = (int16_t)accumulate_axis(&mouse_accum_x, (float)mouse_report.x * factor);
        mouse_report.y = (int16_t)accumulate_axis(&mouse_accum_y, (float)mouse_report.y * factor);
    } else {
        // DRAG SCROLL MODE (ACCELERATED, OMNIDIRECTIONAL, NO LOCKING)
        // low_speed_sensitivity of 1.0 keeps the flat 1:1 passthrough below
        // min_speed (no low-speed ramp for scroll).
        float factor = accel_factor(speed, SCROLL_ACCEL_MIN_SPEED, SCROLL_ACCEL_SLOPE, SCROLL_ACCEL_LIMIT, 1.0f, SCROLL_ACCEL_POWER);

        int8_t h_part = (int8_t)accumulate_axis(&scroll_accum_x, (float)mouse_report.x * factor * SCROLL_SENSITIVITY);
        int8_t v_part = (int8_t)accumulate_axis(&scroll_accum_y, (float)mouse_report.y * factor * -1.0f * SCROLL_SENSITIVITY); // Invert Y for Scroll V

        // Apply the movement as the scroll movement
        mouse_report.v = v_part;
        mouse_report.h = h_part;

        // Lock cursor movement
        mouse_report.x = 0;
        mouse_report.y = 0;
    }
    return mouse_report;
}

/*********************************
* Double tap to lock DRAG_SCROLL *
**********************************/
// https://docs.qmk.fm/features/tap_dance#example-6
// Determine the current tap dance state
static td_state_t cur_dance(tap_dance_state_t *state)
{
    if (state->count == 1)
    {
        if (!state->pressed)
            return TD_SINGLE_TAP;
        else
            return TD_SINGLE_HOLD;
    }
    else if (state->count == 2)
        return TD_DOUBLE_TAP;
    else
        return TD_UNKNOWN;
}

// Initialize tap structure associated with example tap dance key
static td_tap_t double_tap_scroll_tap_state = {
    .is_press_action = true,
    .state = TD_NONE};

/*********************************************
* TOP_LEFT_KEY + tap-dance chord -> back/forward  *
**********************************************/
// Chording the tap-dance button (top right) with TOP_LEFT_KEY (top left) sends
// MS_BTN4/MS_BTN5 instead of the tap-dance key's own tap/hold action. This
// used to be implemented with QMK's combo feature, but combo processing
// buffers *every* press of a combo member for COMBO_TERM before letting it
// fall through to its normal handling -- so a solo hold of the tap-dance
// key had to wait out COMBO_TERM before tap dance even started its own
// TAPPING_TERM hold-resolution, stacking both delays in front of
// drag-scroll engagement. Detecting the chord here instead lets a solo
// press flow straight into tap dance with no added latency, and bounds
// chord detection by TAPPING_TERM: one direction rides tap dance's own
// interrupt window, the other is an explicit check bounded by CHORD_TERM
// below. CHORD_TERM is independent of TAPPING_TERM: widening it doesn't add
// latency to drag-scroll (the checks it gates are instantaneous state
// checks, not buffering waits), it just gives the chord itself more slack.
// The chord itself is a discrete tap_code16() -- not register/unregister --
// since back/forward is a one-shot action, not a state that should stay
// "held" for however long the user happens to keep a physical button down.
//
// TOP_LEFT_KEY's own press (*and* release) is deferred rather than registered
// immediately: if it registered right away, a chord would still send a
// real, if brief, TOP_LEFT_KEY press (Mission Control) to the host before
// canceling it, which would pop it open mid-chord. Deferring the release too
// (not just the press) matters for a fast roll from TOP_LEFT_KEY onto the
// tap-dance key: it often physically releases right as/before the second key
// goes down, and if that release immediately committed a press, the chord would lose the
// race against its own cancellation. Holding both edges back until either a
// chord consumes them or CHORD_TERM expires (matrix_scan_user below) avoids
// ever sending that press for a hold *or* a roll.
//
// This does mean a plain press is no longer reported press-to-release:
// held under CHORD_TERM, it's compressed into a single short tap at the
// CHORD_TERM mark; held longer, the host sees the down edge CHORD_TERM late.
// CHORD_TERM is kept tight (200ms) specifically to bound that cost -- the
// chord gestures actually seen in testing landed within 72-127ms, so 200ms
// covers them with margin without making a plain click feel sluggish.
#define CHORD_TERM 200

static bool     btn3_pending    = false; // TOP_LEFT_KEY pressed, chord window still open, no output sent yet.
static bool     btn3_down       = false; // Is TOP_LEFT_KEY currently physically held (independent of what's been sent).
static bool     btn3_committed  = false; // TOP_LEFT_KEY registered for real (chord window passed while still held).
static uint16_t btn3_press_time = 0;
static bool     btn3_suppressed = false; // TOP_LEFT_KEY consumed into a chord; swallow its press and matching release.
static uint16_t td_press_time   = 0; // Last physical press of the tap-dance key, for the late-back-chord check below.

void matrix_scan_user(void)
{
    if (btn3_pending && timer_elapsed(btn3_press_time) >= CHORD_TERM)
    {
        // Chord window passed with no tap-dance press: resolve TOP_LEFT_KEY as a
        // real click. Still held -> a real hold, to be unregistered on its
        // actual release. Already released -> a complete, if delayed, tap.
        btn3_pending = false;
        if (btn3_down)
        {
            btn3_committed = true;
            register_code16(TOP_LEFT_KEY);
        }
        else
        {
            tap_code16(TOP_LEFT_KEY);
        }
    }
}

// Functions that control what our tap dance key does
static void double_tap_scroll_finished(tap_dance_state_t *state, void *user_data)
{
    double_tap_scroll_tap_state.state = cur_dance(state);

    if (state->interrupted && state->interrupting_keycode == TOP_LEFT_KEY)
    {
        // Top-right held, top-left presses before the dance times out: back,
        // not drag-scroll. Resolved via tap dance's own interrupt, so this
        // costs no extra latency beyond the dance's own TAPPING_TERM.
        tap_code16(MS_BTN5);
        btn3_suppressed = true; // TOP_LEFT_KEY must not also register its own click.
        // This wasn't a hold: make sure double_tap_scroll_reset() doesn't
        // mistake it for one and unregister DRAG_SCROLL on release.
        double_tap_scroll_tap_state.state = TD_NONE;
        return;
    }

    switch (double_tap_scroll_tap_state.state)
    {
    case TD_SINGLE_TAP:
        tap_code16(MS_BTN3);
        break;
    case TD_SINGLE_HOLD:
        register_code16(DRAG_SCROLL);
        drag_scroll_on = true;
        break;
    case TD_DOUBLE_TAP:
        // Check to see if the layer is already set
        if (drag_scroll_on)
        {
            unregister_code16(DRAG_SCROLL);
            drag_scroll_on = false;
        }
        else
        {
            // If not already set, then switch the layer on
            register_code16(DRAG_SCROLL);
            drag_scroll_on = true;
        }
        break;
    default:
        break;
    }
}

static void double_tap_scroll_reset(tap_dance_state_t *state, void *user_data)
{
    // If the key was held down and now is released then switch off the layer
    if (double_tap_scroll_tap_state.state == TD_SINGLE_HOLD)
    {
        unregister_code16(DRAG_SCROLL);
        drag_scroll_on = false;
    }
    double_tap_scroll_tap_state.state = TD_NONE;
    btn3_suppressed                   = false; // Safety net: never let this outlive the dance that set it.
}

// Associate our tap dance key with its functionality
tap_dance_action_t tap_dance_actions[] = {
    [TD_DOUBLE_TAP_SCROLL] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, double_tap_scroll_finished, double_tap_scroll_reset)};

bool process_record_user(uint16_t keycode, keyrecord_t *record)
{
    if (keycode == TOP_LEFT_KEY)
    {
        if (record->event.pressed)
        {
            if (btn3_suppressed)
            {
                // Consumed into a chord by the tap-dance interrupt path above.
                return false;
            }
            if (double_tap_scroll_tap_state.state == TD_SINGLE_HOLD && drag_scroll_on && timer_elapsed(td_press_time) < CHORD_TERM)
            {
                // The tap-dance key's own dance already resolved to a hold
                // (TAPPING_TERM has a hard cap on tap dance's interrupt
                // window) before TOP_LEFT_KEY arrived, but we're still within the
                // wider chord window: treat it as a late back-chord instead
                // of leaving drag-scroll on alongside a plain TOP_LEFT_KEY click.
                unregister_code16(DRAG_SCROLL);
                drag_scroll_on                    = false;
                double_tap_scroll_tap_state.state = TD_NONE;
                tap_code16(MS_BTN5);
                return false;
            }
            // Hold the click back -- see matrix_scan_user() above.
            btn3_pending    = true;
            btn3_down       = true;
            btn3_committed  = false;
            btn3_press_time = timer_read();
            return false;
        }
        else
        {
            btn3_down = false;
            if (btn3_committed)
            {
                // Already registered for real; let this release flow through
                // normally to unregister it.
                btn3_committed = false;
                btn3_pending   = false;
                return true;
            }
            // Otherwise TOP_LEFT_KEY is still pending (chord window not yet
            // resolved -- matrix_scan_user will tap it out if nothing
            // consumes it first) or was already consumed by a chord. Either
            // way there's nothing to send here.
            btn3_suppressed = false; // No-op if already false; consumed above otherwise.
            return false;
        }
    }
    else if (keycode == TD(TD_DOUBLE_TAP_SCROLL))
    {
        if (record->event.pressed)
        {
            td_press_time = timer_read();
            if (btn3_pending)
            {
                // TOP_LEFT_KEY's click is still being held back: this is a chord,
                // not a standalone click -- consume it without ever
                // registering TOP_LEFT_KEY at all.
                btn3_pending = false;
                tap_code16(MS_BTN4);
                return false;
            }
        }
        // No special handling needed on release: this key's press either went
        // to tap dance normally (handled by its own finished()/reset()), or
        // was consumed above -- either way there's nothing left to release
        // here, and an orphaned release is a safe no-op.
    }

    return true;
}
