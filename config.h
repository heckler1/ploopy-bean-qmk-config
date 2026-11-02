/* Copyright 2023 Colin Lam (Ploopy Corporation)
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

#pragma once

#define UNUSABLE_PINS \
    { GP1, GP2, GP3, GP4, \
      GP7, GP8, GP9, GP10, GP11, GP12, GP13, GP14, GP15, GP16, GP17, GP18, GP19, \
      GP21, GP22, \
      GP25, GP26, GP27, GP28, GP29 \
    }

/* TMAG5273 Pointing Device Settings */
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP0
#define I2C1_SCL_PIN GP5

#define PLOOPY_DRAGSCROLL_MOMENTARY
// #define PLOOPY_DRAGSCROLL_DIVISOR_H 1.0
// #define PLOOPY_DRAGSCROLL_DIVISOR_V 1.0
// #define PLOOPY_DRAGSCROLL_INVERT
// Windows starts beeping if we send scroll events too quickly.
#define PLOOPY_DRAGSCROLL_RATE_LIMIT 20

#define SCROLL_DIRECTION_LOCK_ENABLE
#define POINTING_DEVICE_TASK_THROTTLE_MS 1
//#define PLOOPY_DPI_DEFAULT 1
//#define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#define WHEEL_EXTENDED_REPORT

/* --- Sensor Configuration (Ploopy Adept) ---
These DPI values are pretty high, to provide smooth low jitter 
movement and scrolling. Instead, speed is controlled through 
the acceleration factors below.
*/
//#define PLOOPY_TRACKBALL_CPI 2000
#define PLOOPY_DRAGSCROLL_DPI 1200 
#define PLOOPY_DRAGSCROLL_INVERT_Y // Invert vertical scroll direction (Default is "Natural" Scrolling)
// #define POINTING_DEVICE_HIRES_SCROLL_ENABLE // Enable fluid high-resolution scrolling - broken on MacOS

/* --- Mouse Acceleration (Quadratic) --- */
#define MOUSE_ACCEL_MIN_SPEED 17 // Threshold to start acceleration
#define MOUSE_ACCEL_SLOPE 0.6  // Aggressiveness of acceleration (Curve Steepness)
#define MOUSE_ACCEL_LIMIT 1.5 // Maximum speed multiplier cap
// Multiplier applied at speed == 0, ramping to 1.0x at MOUSE_ACCEL_MIN_SPEED.
// Lower = slower/more precise fine movement, without affecting speeds >= MOUSE_ACCEL_MIN_SPEED.
#define MOUSE_ACCEL_LOW_SPEED_SENSITIVITY 0.01f
// Shape of that ramp: 1.0 is linear, higher values hold the multiplier down for longer
// (finer slow/mid-speed control) and then rise more steeply to 1.0x at MOUSE_ACCEL_MIN_SPEED.
#define MOUSE_ACCEL_LOW_SPEED_RAMP_POWER 2.0f
#define MOUSE_ACCEL_POWER 2 // Exponential scaling of the acceleration
// Zero the fractional-count accumulators after this many ms with no sensor motion,
// so noise can't integrate into slow drift between movements.
#define ACCUM_IDLE_RESET_MS 30

// Negate both sensor axes. The magnet's pole orientation sets the sign of the
// field the sensor reads, so a magnet installed the other way up reverses movement.
#define SENSOR_INVERT_XY

/* --- Scroll Configuration (Quadratic & High-Res) --- */
#define SCROLL_ACCEL_MIN_SPEED 15 // Threshold to start acceleration
#define SCROLL_ACCEL_SLOPE 1.5  // Aggressiveness of acceleration (Curve Steepness)
#define SCROLL_ACCEL_LIMIT 4.5 // Maximum speed multiplier cap
#define SCROLL_ACCEL_POWER 4 // Exponential scaling of the acceleration
/*
Multiplier for fine-grained scroll speed

This behaves slightly differently than the mouse
sensitivity, for more consistent behavior while 
scrolling through a page at a moderate speed.
*/
#define SCROLL_SENSITIVITY 0.002f
/*
High refresh rate for smooth scroll

The default of 10,s for most trackballs causes 
choppy scrolling, especially at low trackball speeds
*/
//#define POINTING_DEVICE_TASK_THROTTLE_MS 1 
#define TAPPING_TERM 200

// tap_code16() defaults to register/unregister back-to-back with no gap,
// which some hosts drop as too fast to register as a real click.
#define TAP_CODE_DELAY 10
