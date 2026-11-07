/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

/*
 * Turn the thumb underglow green on both halves while the MOUSE layer is
 * active, then restore the previous colour. The change is sent through the
 * global &rgb_ug behavior, so the right-hand peripheral follows the central.
 * RGB_COLOR_HSB only updates the in-memory colour and does not schedule a
 * settings save, so layer changes cause no flash writes.
 */

#include <zephyr/kernel.h>

#include <dt-bindings/zmk/rgb.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

#define MOUSE_LAYER 3
#define MOUSE_HUE 120 /* green */
#define MOUSE_SAT 100

static struct zmk_led_hsb saved_color;
static bool showing_mouse;

static void set_color_all(struct zmk_led_hsb color) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = "rgb_ug",
        .param1 = RGB_COLOR_HSB_CMD,
        .param2 = RGB_COLOR_HSB_VAL(color.h, color.s, color.b),
    };
    struct zmk_behavior_binding_event event = {
        .layer = MOUSE_LAYER,
        .position = 0,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    /* &rgb_ug has global locality: applied locally and sent to peripherals. */
    zmk_behavior_invoke_binding(&binding, event, true);
    zmk_behavior_invoke_binding(&binding, event, false);
}

/* Apply changes from the system work queue, outside the event's context. */
static void update_work_handler(struct k_work *work) {
    bool mouse = zmk_keymap_layer_active(MOUSE_LAYER);
    if (mouse == showing_mouse) {
        return;
    }

    if (mouse) {
        /* A zero step returns the current colour unchanged. */
        saved_color = zmk_rgb_underglow_calc_hue(0);
        set_color_all((struct zmk_led_hsb){
            .h = MOUSE_HUE, .s = MOUSE_SAT, .b = saved_color.b});
    } else {
        set_color_all(saved_color);
    }
    showing_mouse = mouse;
}

static K_WORK_DEFINE(update_work, update_work_handler);

static int mouse_layer_rgb_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev != NULL && ev->layer == MOUSE_LAYER) {
        k_work_submit(&update_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bifrost_mouse_layer_rgb, mouse_layer_rgb_listener);
ZMK_SUBSCRIPTION(bifrost_mouse_layer_rgb, zmk_layer_state_changed);
