# Copyright (c) 2026 hringdrifi
# SPDX-License-Identifier: MIT

zephyr_library()
zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library_sources(../bifrost_regout0.c bifrost_status_led.c)
