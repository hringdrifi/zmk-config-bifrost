/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <drivers/ext_power.h>
#include <zmk/rgb_underglow.h>
#include <zmk/workqueue.h>
#include <string.h>

static const struct gpio_dt_spec data_line =
    GPIO_DT_SPEC_GET(DT_CHOSEN(zmk_underglow), gpios);
static const struct device *const led_power =
    DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
static bool static_mode;
static bool requested_high;
static int static_result;

static void set_static_line(struct k_work *work) {
    ARG_UNUSED(work);
    /* Run after the queued RGB-off write on the same work queue. */
    static_result = ext_power_enable(led_power);
    if (static_result < 0) {
        return;
    }
    /* Keep the input connected so GPIO readback can also be reported. */
    static_result = gpio_pin_configure_dt(
        &data_line, GPIO_INPUT | (requested_high ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE));
    static_mode = static_result == 0;
}

K_WORK_DEFINE(static_line_work, set_static_line);

static int cmd_rgbdiag(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    if (!gpio_is_ready_dt(&data_line) || !device_is_ready(led_power)) {
        shell_error(sh, "RGB GPIO or LED power device is not ready");
        return -ENODEV;
    }

    int err = 0;
    if (!strcmp(argv[1], "high") || !strcmp(argv[1], "low")) {
        err = zmk_rgb_underglow_off();
        if (err < 0) {
            return err;
        }
        requested_high = !strcmp(argv[1], "high");
        struct k_work_sync sync;
        err = k_work_submit_to_queue(zmk_workqueue_lowprio_work_q(), &static_line_work);
        if (err < 0) {
            return err;
        }
        k_work_flush(&static_line_work, &sync);
        err = static_result;
    } else if (!strcmp(argv[1], "resume")) {
        err = zmk_rgb_underglow_on();
        if (!err) {
            static_mode = false;
        }
    } else if (strcmp(argv[1], "status")) {
        shell_error(sh, "Use: rgbdiag status|low|high|resume");
        return -EINVAL;
    }

    if (err < 0) {
        shell_error(sh, "RGB diagnostic command failed: %d", err);
        return err;
    }
    bool rgb_on;
    err = zmk_rgb_underglow_get_state(&rgb_on);
    if (err < 0) {
        return err;
    }
    struct zmk_led_hsb color = zmk_rgb_underglow_calc_brt(0);
    shell_print(sh, "mode=%s rgb_on=%d power_on=%d h=%u s=%u brightness=%u",
                static_mode ? "static" : "normal", rgb_on, ext_power_get(led_power),
                color.h, color.s, color.b);
    if (static_mode) {
        shell_print(sh, "data=P0.27 requested=%d readback=%d", requested_high,
                    gpio_pin_get_dt(&data_line));
    }
    return 0;
}

SHELL_CMD_ARG_REGISTER(rgbdiag, NULL, "RGB multimeter test: status|low|high|resume",
                       cmd_rgbdiag, 2, 0);
