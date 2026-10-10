/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_pmw3610.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(bifrost_trackball, LOG_LEVEL_INF);

#define TRACKBALL_NODE DT_NODELABEL(trackball_0)

static const struct device *const trackball = DEVICE_DT_GET(TRACKBALL_NODE);
static const struct gpio_dt_spec motion = GPIO_DT_SPEC_GET(TRACKBALL_NODE, motion_gpios);
static atomic_t input_events;
static const struct spi_dt_spec trackball_spi = SPI_DT_SPEC_GET(
    TRACKBALL_NODE, SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA, 15);
static unsigned int poll_count;
static unsigned int poll_motion_count;
static bool awake_requested;
static int awake_result;

/* Read the same burst as the driver, including optical quality and shutter.
 * Only used while no input callbacks have been observed. Burst reads consume
 * motion data, so this diagnostic fallback stops once normal input is working.
 */
static int read_motion_burst(uint8_t *burst) {
    uint8_t address = 0x12;
    const struct spi_buf tx_buf = {.buf = &address, .len = 1};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf[] = {{.buf = NULL, .len = 1}, {.buf = burst, .len = 7}};
    const struct spi_buf_set rx = {.buffers = rx_buf, .count = ARRAY_SIZE(rx_buf)};

    return spi_transceive_dt(&trackball_spi, &tx, &rx);
}

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

    bool ready = device_is_ready(trackball);
    atomic_val_t events = atomic_get(&input_events);

    if (ready && !awake_requested) {
        awake_result = pmw3610_force_awake(trackball, true);

        LOG_INF("force_awake ret=%d", awake_result);
        awake_requested = true;
        /* Let the sensor acquire frames before inspecting optical quality. */
        k_work_reschedule(&status_work, K_MSEC(100));
        return;
    }

    if (ready && events == 0) {
        uint8_t burst[7] = {0};
        int ret = read_motion_burst(burst);

        if (ret == 0 && (burst[0] & BIT(7))) {
            int32_t x = sign_extend(((burst[3] << 4) & 0xf00) | burst[1], 11);
            int32_t y = sign_extend(((burst[3] << 8) & 0xf00) | burst[2], 11);
            poll_motion_count++;
            LOG_INF("polled motion x=%d y=%d motion_active=%d", x, y, motion_active);
        }
        if ((poll_count++ % 20) == 0) {
            LOG_INF("sensor_ready=1 awake_ret=%d motion_active=%d input_events=0 poll_ret=%d "
                    "motion=0x%02x squal=%u shutter=%u polled_motion=%u",
                    awake_result, motion_active, ret, burst[0], burst[4], sys_get_be16(&burst[5]),
                    poll_motion_count);
        }
        k_work_reschedule(&status_work, K_MSEC(100));
    } else {
        LOG_INF("sensor_ready=%u motion_active=%d input_events=%ld", ready, motion_active,
                (long)events);
        k_work_reschedule(&status_work, K_SECONDS(2));
    }
}

static int trackball_diagnostics_init(void) {
    k_work_reschedule(&status_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(trackball_diagnostics_init, APPLICATION, 99);
