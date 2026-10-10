# Copyright (c) 2026 hringdrifi
# SPDX-License-Identifier: MIT

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BIFROST_TRACKBALL_DIAGNOSTICS src/trackball_diagnostics.c)
