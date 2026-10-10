/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(bifrost_trackball, LOG_LEVEL_INF);

#define TRACKBALL_NODE DT_NODELABEL(trackball_0)

static const struct device *const trackball = DEVICE_DT_GET(TRACKBALL_NODE);
static const struct gpio_dt_spec motion = GPIO_DT_SPEC_GET(TRACKBALL_NODE, motion_gpios);
static atomic_t input_events;

static void trackball_input(struct input_event *event, void *user_data) {
    ARG_UNUSED(user_data);
    atomic_inc(&input_events);
    LOG_INF("input type=%u code=%u value=%d sync=%u", event->type, event->code,
            event->value, event->sync);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(TRACKBALL_NODE), trackball_input, NULL);

static void trackball_status(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(status_work, trackball_status);

static void trackball_status(struct k_work *work) {
    ARG_UNUSED(work);
    int motion_active = gpio_is_ready_dt(&motion) ? gpio_pin_get_dt(&motion) : -ENODEV;

    LOG_INF("sensor_ready=%u motion_active=%d input_events=%ld", device_is_ready(trackball),
            motion_active, (long)atomic_get(&input_events));
    k_work_reschedule(&status_work, K_SECONDS(2));
}

static int trackball_diagnostics_init(void) {
    k_work_reschedule(&status_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(trackball_diagnostics_init, APPLICATION, 99);
