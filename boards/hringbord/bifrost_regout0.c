/*
 * Copyright (c) 2026 hringdrifi
 * SPDX-License-Identifier: MIT
 */

/*
 * Both Bifrost main boards supply the MDBT50Q from VDDH (+VSW), which puts the
 * nRF52840 in high-voltage mode. GPIO levels then follow REG0's output, set by
 * UICR.REGOUT0, and an erased UICR selects 1.8 V. The thumb WS2812 LEDs run
 * from +VSW (about 3.7-4.7 V) and need roughly 0.7 * VDD on DIN, so 1.8 V data
 * is never recognised. Program REGOUT0 to 3.3 V once, then reset to apply it.
 */

#include <zephyr/init.h>
#include <nrfx.h>

#define BIFROST_REGOUT0_VOUT UICR_REGOUT0_VOUT_3V3

static int bifrost_regout0_init(void) {
    if ((NRF_POWER->MAINREGSTATUS & POWER_MAINREGSTATUS_MAINREGSTATUS_Msk) !=
        (POWER_MAINREGSTATUS_MAINREGSTATUS_High << POWER_MAINREGSTATUS_MAINREGSTATUS_Pos)) {
        return 0;
    }

    uint32_t vout = (NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos;
    if (vout != UICR_REGOUT0_VOUT_DEFAULT) {
        /* Already programmed; UICR bits cannot be raised without an erase. */
        return 0;
    }

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
    }
    NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~UICR_REGOUT0_VOUT_Msk) |
                        (BIFROST_REGOUT0_VOUT << UICR_REGOUT0_VOUT_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
    }
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
    }

    /* Reset only if the write took effect, so a failed write cannot loop. */
    if (((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos) ==
        BIFROST_REGOUT0_VOUT) {
        NVIC_SystemReset();
    }

    return 0;
}

SYS_INIT(bifrost_regout0_init, PRE_KERNEL_1, 0);
