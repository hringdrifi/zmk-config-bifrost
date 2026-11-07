/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <drivers/ext_power.h>
#include <zmk/rgb_underglow.h>
#include <zmk/workqueue.h>

static const struct gpio_dt_spec data_line =
    GPIO_DT_SPEC_GET(DT_CHOSEN(zmk_underglow), gpios);
static const struct device *const led_power =
    DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
static bool next_high;

static void toggle_data_line(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(toggle_work, toggle_data_line);

static void toggle_data_line(struct k_work *work) {
    ARG_UNUSED(work);
    if (ext_power_get(led_power) != 1 && ext_power_enable(led_power) < 0) {
        return;
    }
    if (gpio_pin_configure_dt(&data_line,
                             next_high ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE) < 0) {
        return;
    }
    next_high = !next_high;
    k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &toggle_work, K_SECONDS(10));
}

static void start_test(struct k_work *work) {
    ARG_UNUSED(work);
    if (!gpio_is_ready_dt(&data_line) || !device_is_ready(led_power)) {
        return;
    }
    if (zmk_rgb_underglow_off() < 0) {
        return;
    }
    /* Queue behind the RGB-off write, so it cannot overwrite our static level. */
    k_work_reschedule_for_queue(zmk_workqueue_lowprio_work_q(), &toggle_work, K_NO_WAIT);
}

K_WORK_DELAYABLE_DEFINE(start_work, start_test);

static int rgb_auto_test_init(void) {
    /* Let RGB initialization and saved settings load before taking over P0.27. */
    k_work_schedule(&start_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(rgb_auto_test_init, APPLICATION, 99);
