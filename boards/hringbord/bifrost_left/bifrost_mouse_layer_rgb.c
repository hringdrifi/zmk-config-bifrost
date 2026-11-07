/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

/*
 * Turn the thumb underglow green on both halves while the MOUSE layer is
 * active, then restore the previous colour. The change is sent through the
 * global &rgb_ug behavior, so the right-hand peripheral follows the central.
 * RGB_COLOR_HSB only updates the in-memory colour and does not schedule a
 * settings save. With an animated effect, the layer also switches to the
 * solid effect, which schedules one debounced save (60 s after the last
 * change); with the solid effect, layer changes cause no flash writes.
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

#define EFFECT_SOLID 0

static struct zmk_led_hsb saved_color;
static int saved_effect;
static bool showing_mouse;
/* Whether the colour/effect was changed and must be restored. */
static bool applied;

static void invoke_rgb_ug_all(uint32_t cmd, uint32_t param) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = "rgb_ug",
        .param1 = cmd,
        .param2 = param,
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

static void set_color_all(struct zmk_led_hsb color) {
    invoke_rgb_ug_all(RGB_COLOR_HSB_CMD, RGB_COLOR_HSB_VAL(color.h, color.s, color.b));
}

/* Unlike colour changes, selecting an effect schedules a settings save. */
static void set_effect_all(int effect) { invoke_rgb_ug_all(RGB_EFS_CMD, effect); }

/* Apply changes from the system work queue, outside the event's context. */
static void update_work_handler(struct k_work *work) {
    bool mouse = zmk_keymap_layer_active(MOUSE_LAYER);
    if (mouse == showing_mouse) {
        return;
    }

    showing_mouse = mouse;

    if (mouse) {
        bool on = false;
        /* With the lights off, change nothing so no settings save is scheduled. */
        if (zmk_rgb_underglow_get_state(&on) != 0 || !on) {
            return;
        }

        /* A zero step returns the current colour unchanged. */
        saved_color = zmk_rgb_underglow_calc_hue(0);
        saved_effect = zmk_rgb_underglow_calc_effect(0);
        /* Animated effects ignore the colour, so show solid green instead. */
        if (saved_effect != EFFECT_SOLID) {
            set_effect_all(EFFECT_SOLID);
        }
        set_color_all((struct zmk_led_hsb){
            .h = MOUSE_HUE, .s = MOUSE_SAT, .b = saved_color.b});
        applied = true;
    } else if (applied) {
        set_color_all(saved_color);
        if (saved_effect != EFFECT_SOLID) {
            set_effect_all(saved_effect);
        }
        applied = false;
    }
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
