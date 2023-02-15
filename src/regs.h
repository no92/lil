#pragma once

#include <lil/imports.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REG_PTR(r) ((volatile uint32_t *) (gpu->mmio_start + (r)))
#define REG(r) (*REG_PTR(r))

#define GDT_CHICKEN_BITS 0x09840

#define FUSE_STATUS 0x42000
#define FUSE_STATUS_PG0 (1 << 27)
#define FUSE_STATUS_PG1 (1 << 26)
#define FUSE_STATUS_PG2 (1 << 25)

#define DBUF_CTL 0x45008
#define DBUF_CTL_POWER_ENABLE (1 << 31)
#define DBUF_CTL_POWER_STATE (1 << 30)

#define HSW_PWR_WELL_CTL1 0x45400
#define HSW_PWR_WELL_CTL1_POWER_WELL_2_REQUEST (1 << 31)
#define HSW_PWR_WELL_CTL1_POWER_WELL_2_STATE (1 << 30)
#define HSW_PWR_WELL_CTL1_POWER_WELL_1_REQUEST (1 << 29)
#define HSW_PWR_WELL_CTL1_POWER_WELL_1_STATE (1 << 28)

#define CDCLK_CTL 0x46000
#define CDCLK_CTL_DECIMAL_MASK (0x7FF)
#define CDCLK_CTL_DECIMAL(v) (v & CDCLK_CTL_DECIMAL_MASK)
#define CDCLK_CTL_FREQ_SELECT_MASK (3 << 26)
#define CDCLK_CTL_FREQ_SELECT(v) (((v) << 26) & CDCLK_CTL_FREQ_SELECT_MASK)

#define LCPLL1_CTL 0x46010
#define LCPLL1_CTL_DPLL0_ENABLE (1 << 31)

#define NDE_RSTWRN_OPT 0x46408

#define BLC_PWM_DATA 0x48254

#define DDI_BUF_CTL(i) (0x64000 + (i * 0x100))
#define DDI_BUF_CTL_ENABLED (1 << 31)

#define DDI_BUF_TRANS(i) (0x64E00 + (0x60 * i))

#define DPLL_CTRL1 0x6C058
#define DPLL_CTRL1_LINK_RATE_MASK(i) (7 << ((i) * 6 + 1))
#define DPLL_CTRL1_LINK_RATE(i, v) ((v) << ((i) * 6 + 1))
#define DPLL_CTRL1_PROGRAM_ENABLE(i) (1 << ((i) * 6))
#define DPLL_CTRL1_SSC_ENABLE(i) (1 << ((i) * 6 + 4))
#define DPLL_CTRL1_HDMI_MODE(i) (1 << ((i) * 6 + 5))

#define DPLL_STATUS 0x6C060
#define DPLL_STATUS_DPLL0_LOCK (1 << 0)

#define PLANE_BUF_CFG_1_A 0x7027C
#define PLANE_BUF_CFG_1_B 0x7127C

#define SOUTH_CHICKEN1 0xC2000

#define SOUTH_DSPCLK_GATE_D 0xC2020
#define SOUTH_DSPCLK_GATE_D_PCH_LP_PARTITION_LEVEL_DISABLE 0x1000

#define SDEISR 0xC4000
#define SDEIMR 0xC4004
#define SDEIER 0xC400C

#define SHOTPLUG_CTL 0xC4030

#define PP_CONTROL 0xC7204
#define PP_OFF_DELAYS 0xC720C
#define PP_DIVISOR 0xC7210

#define SBLC_PWM_CTL1 0xC8250
#define SBLC_PWM_CTL2 0xC8254

#define GEN6_PCODE_MAILBOX 0x138124
#define GEN6_PCODE_MAILBOX_READY (1 << 31)

#define GEN6_PCODE_DATA 0x138128
#define GEN6_PCODE_DATA1 0x13812C

static inline bool wait_for_bit_set(volatile uint32_t *reg, uint32_t mask, size_t timeout_us, size_t step_us) {
	if(timeout_us % step_us != 0) {
		lil_panic("timeout is not a multiple of step!");
	}

	while((*reg & mask) != mask) {
		lil_usleep(step_us);

		timeout_us -= step_us;

		if(!timeout_us)
			return false;
	}

	return true;
}

static inline bool wait_for_bit_unset(volatile uint32_t *reg, uint32_t mask, size_t timeout_us, size_t step_us) {
	if(timeout_us % step_us != 0) {
		lil_panic("timeout is not a multiple of step!");
	}

	while((*reg & mask) != 0) {
		lil_usleep(step_us);

		timeout_us -= step_us;

		if(!timeout_us)
			return false;
	}

	return true;
}
