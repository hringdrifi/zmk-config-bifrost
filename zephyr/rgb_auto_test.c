/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zmk/rgb_underglow.h>

static const struct gpio_dt_spec data_line =
    GPIO_DT_SPEC_GET(DT_CHOSEN(zmk_underglow), gpios);
static const struct gpio_dt_spec power_line =
    GPIO_DT_SPEC_GET(DT_NODELABEL(thumb_led_power), control_gpios);

/* Retain the results for debugger inspection; USB logging is not required. */
volatile uint32_t bifrost_rgb_test_cycles;
volatile int bifrost_rgb_test_stop_result;
volatile int bifrost_rgb_test_power_result;
volatile int bifrost_rgb_test_data_result;
volatile int bifrost_rgb_test_readback;

static void run_voltage_test(void *a, void *b, void *c) {
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    /* Stop normal writes, but an RGB subsystem error must not cancel this test. */
    bifrost_rgb_test_stop_result = zmk_rgb_underglow_off();
    k_msleep(100);

    bool high = false;
    for (;;) {
        /* Drive the power GPIO directly, independently of ext-power state. */
        bifrost_rgb_test_power_result = gpio_is_ready_dt(&power_line)
            ? gpio_pin_configure_dt(&power_line, GPIO_OUTPUT_ACTIVE) : -ENODEV;

        if (gpio_is_ready_dt(&data_line)) {
            bifrost_rgb_test_data_result = gpio_pin_configure_dt(
                &data_line, GPIO_INPUT | (high ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE));
            bifrost_rgb_test_readback = bifrost_rgb_test_data_result < 0
                ? bifrost_rgb_test_data_result : gpio_pin_get_dt(&data_line);
        } else {
            bifrost_rgb_test_data_result = -ENODEV;
            bifrost_rgb_test_readback = -ENODEV;
        }
        bifrost_rgb_test_cycles++;
        high = !high;
        k_msleep(10000);
    }
}

/* A dedicated thread avoids dependence on either Zephyr or ZMK work queues. */
K_THREAD_DEFINE(rgb_voltage_test_thread, 1024, run_voltage_test,
                NULL, NULL, NULL, 10, 0, 2000);
