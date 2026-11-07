/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

/*
 * Flash the left-hand status RGB LED (D1, common anode) briefly when the
 * output endpoint changes: USB, or one of the Bluetooth profiles.
 * Cathodes: red P0.20, green P0.24, blue P0.22 (active low).
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/events/ble_active_profile_changed.h>
#endif

#define FLASH_MS 1000
/* Ignore endpoint and profile restoration while the keyboard starts. */
#define STARTUP_QUIET_MS 3000

#define RED BIT(0)
#define GREEN BIT(1)
#define BLUE BIT(2)

static const struct device *const gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const gpio_pin_t pins[] = {20, 24, 22}; /* red, green, blue */

/* Bluetooth profiles 0-4. */
static const uint8_t profile_colors[] = {
    BLUE, GREEN, RED, GREEN | BLUE, RED | BLUE,
};
#define USB_COLOR (RED | GREEN | BLUE)

static void set_color(uint8_t color) {
    for (int i = 0; i < ARRAY_SIZE(pins); i++) {
        /* Low turns a cathode on. */
        gpio_pin_set_raw(gpio0, pins[i], (color & BIT(i)) ? 0 : 1);
    }
}

static void off_work_handler(struct k_work *work) { set_color(0); }

static K_WORK_DELAYABLE_DEFINE(off_work, off_work_handler);

static void flash(uint8_t color) {
    if (k_uptime_get() < STARTUP_QUIET_MS) {
        return;
    }
    set_color(color);
    k_work_reschedule(&off_work, K_MSEC(FLASH_MS));
}

static uint8_t profile_color(int index) {
    return (index >= 0 && index < ARRAY_SIZE(profile_colors)) ? profile_colors[index]
                                                              : (RED | GREEN);
}

static int status_led_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *ep = as_zmk_endpoint_changed(eh);
    if (ep != NULL) {
        if (ep->endpoint.transport == ZMK_TRANSPORT_USB) {
            flash(USB_COLOR);
        } else if (ep->endpoint.transport == ZMK_TRANSPORT_BLE) {
            flash(profile_color(ep->endpoint.ble.profile_index));
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

#if IS_ENABLED(CONFIG_ZMK_BLE)
    /* Also covers BT_SEL while USB remains the active endpoint. */
    const struct zmk_ble_active_profile_changed *prof = as_zmk_ble_active_profile_changed(eh);
    if (prof != NULL) {
        flash(profile_color(prof->index));
    }
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bifrost_status_led, status_led_listener);
ZMK_SUBSCRIPTION(bifrost_status_led, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(bifrost_status_led, zmk_ble_active_profile_changed);
#endif

static int status_led_init(void) {
    if (!device_is_ready(gpio0)) {
        return -ENODEV;
    }
    for (int i = 0; i < ARRAY_SIZE(pins); i++) {
        /* Output high keeps every colour off. */
        gpio_pin_configure(gpio0, pins[i], GPIO_OUTPUT_HIGH);
    }
    return 0;
}

SYS_INIT(status_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
