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

#define CHICKEN_TRANS(ddi_id) (0x420C0 + (((ddi_id) - 1) * 4))

#define ISR(i) (0x44400 + ((i) * 0x10))
#define IMR(i) (0x44404 + ((i) * 0x10))
#define IIR(i) (0x44408 + ((i) * 0x10))

#define DBUF_CTL 0x45008
#define DBUF_CTL_POWER_ENABLE (1 << 31)
#define DBUF_CTL_POWER_STATE (1 << 30)

#define HDPORT_STATE 0x45050
#define HDPORT_STATE_ENABLED (1 << 0)
#define HDPORT_STATE_DPLL0_USED (1 << 12)
#define HDPORT_STATE_DPLL1_USED (1 << 13)
#define HDPORT_STATE_DPLL2_USED (1 << 15)
#define HDPORT_STATE_DPLL3_USED (1 << 14)

#define WM_LINETIME(i) (0x45270 + ((i) * 4))

#define HSW_PWR_WELL_CTL1 0x45400
#define HSW_PWR_WELL_CTL1_POWER_WELL_2_REQUEST (1 << 31)
#define HSW_PWR_WELL_CTL1_POWER_WELL_2_STATE (1 << 30)
#define HSW_PWR_WELL_CTL1_POWER_WELL_1_REQUEST (1 << 29)
#define HSW_PWR_WELL_CTL1_POWER_WELL_1_STATE (1 << 28)

#define DC_STATE_EN 0x45504

#define CDCLK_CTL 0x46000
#define CDCLK_CTL_DECIMAL_MASK (0x7FF)
#define CDCLK_CTL_DECIMAL(v) (v & CDCLK_CTL_DECIMAL_MASK)
#define CDCLK_CTL_FREQ_SELECT_MASK (3 << 26)
#define CDCLK_CTL_FREQ_SELECT(v) (((v) << 26) & CDCLK_CTL_FREQ_SELECT_MASK)

#define LCPLL1_CTL 0x46010
#define LCPLL1_ENABLE (1u << 31)
#define LCPLL1_LOCK (1 << 30)

#define LCPLL2_CTL 0x46014
#define LCPLL2_ENABLE (1u << 31)

#define WRPLL_CTL1 0x46040
#define WRPLL_CTL2 0x46060

#define TRANS_CLK_SEL(i) (0x46140 + ((i) * 4))

#define NDE_RSTWRN_OPT 0x46408

#define BLC_PWM_CTL 0x48250
#define BLC_PWM_DATA 0x48254

#define SWF_6 0x4F018
#define SWF_24 0x4F060

#define TRANSCODER_A_BASE 0x60000
#define TRANSCODER_B_BASE 0x61000
#define TRANSCODER_C_BASE 0x62000
#define TRANSCODER_EDP_BASE 0x6F000

#define TRANS_HTOTAL 0x0
#define TRANS_HBLANK 0x4
#define TRANS_HSYNC 0x8
#define TRANS_VTOTAL 0xC
#define TRANS_VBLANK 0x10
#define TRANS_VSYNC 0x14
#define TRANS_SPACE 0x24
#define TRANS_VSYNCSHIFT 0x28
#define TRANS_DATAM 0x30
#define TRANS_DATAN 0x34
#define TRANS_LINKM 0x40
#define TRANS_LINKN 0x44
#define TRANS_MULT 0x210
#define TRANS_DDI_FUNC_CTL 0x400
#define TRANS_MSA_MISC 0x410
#define TRANS_CONF 0x10008

#define PIPE_SRCSZ(pipe) (0x6001C + ((pipe) * 0x1000))

// #define VIDEO_DIP_CTL(transcoder) (((transcoder) == TRANSCODER_EDP) ? 0x6F200 : (0xre60200 + ((transcoder) * 0x1000)))
#define VIDEO_DIP_CTL(ddi) (0x60200 + ((ddi) * 0x1000))
#define VIDEO_DIP_AVI_DATA(ddi) (0x60220 + ((ddi) * 0x1000))

#define DDI_BUF_CTL(i) (0x64000 + ((i) * 0x100))
#define DDI_BUF_CTL_ENABLE (1 << 31)
#define DDI_BUF_CTL_IDLE (1 << 7)
#define DDI_BUF_CTL_DISPLAY_DETECTED (1 << 0)

#define DDI_BUF_CTL_DP_PORT_WIDTH(v) (((v) >> 1) & 0x7)

#define DDI_AUX_CTL(c) (0x64010 + ((c) * 0x100))
#define DDI_AUX_CTL_BUSY (1u << 31)
#define DDI_AUX_CTL_DONE (1 << 30)
#define DDI_AUX_CTL_IRQ (1 << 29)
#define DDI_AUX_CTL_TIMEOUT (1 << 28)
#define DDI_AUX_CTL_TIMEOUT_VAL_MAX (0x3 << 26)
#define DDI_AUX_CTL_RX_ERR (1 << 25)
#define DDI_AUX_SET_MSG_SIZE(sz) (((sz) & 0x1F) << 20)
#define DDI_AUX_GET_MSG_SIZE(ctl) (((ctl) >> 20) & 0x1F)

#define DDI_AUX_CTL_SKL_FW_SYNC_PULSE(v) (((v) - 1) << 5)
#define DDI_AUX_CTL_SKL_SYNC_PULSE(v) ((v) - 1)

#define DDI_AUX_DATA(c) (0x64014 + ((c) * 0x100))
#define DDI_AUX_I2C_WRITE 0x0
#define DDI_AUX_I2C_READ 0x1
#define DDI_AUX_I2C_MOT (1 << 2) // Middle of Transfer

#define DDI_AUX_NATIVE_WRITE 0x8
#define DDI_AUX_NATIVE_READ 0x9

#define DP_TP_CTL(i) (0x64040 + ((i) * 0x100))
#define DP_TP_CTL_ENABLE (1u << 31)
#define DP_TP_CTL_TRAIN_MASK (7 << 8)
#define DP_TP_CTL_TRAIN_PATTERN1 (0 << 8)
#define DP_TP_CTL_TRAIN_PATTERN2 (1 << 8)
#define DP_TP_CTL_TRAIN_PATTERN_IDLE (2 << 8)
#define DP_TP_CTL_TRAIN_PATTERN_NORMAL (3 << 8);

#define DDI_BUF_TRANS(i) (0x64E00 + (0x60 * i))

#define PS_WIN_POS_1(pipe) (0x68170 + (0x800 * pipe))
#define PS_WIN_POS_2(pipe) (0x68270 + (0x800 * pipe))

#define PS_CTRL_1(i) (0x68180 + (0x800 * i))
#define PS_CTRL_2(i) (0x68280 + (0x800 * i))

#define PS_WIN_SZ_1(i) (0x68174 + (0x800 * i))
#define PS_WIN_SZ_2(i) (0x68274 + (0x800 * i))

#define DISPIO_CR_TX_BMU_CR0 0x6C00C

#define DPLL_CFGCR1(pll) (0x6C040 + ((pll - LCPLL2) * 8))
#define DPLL_CFGCR2(pll) (0x6C044 + ((pll - LCPLL2) * 8))

#define DPLL_CTRL1 0x6C058
#define DPLL_CTRL1_LINK_RATE_MASK(i) (7 << ((i) * 6 + 1))
#define DPLL_CTRL1_LINK_RATE(i, v) ((v) << ((i) * 6 + 1))
#define DPLL_CTRL1_PROGRAM_ENABLE(i) (1 << ((i) * 6))
#define DPLL_CTRL1_SSC_ENABLE(i) (1 << ((i) * 6 + 4))
#define DPLL_CTRL1_HDMI_MODE(i) (1 << ((i) * 6 + 5))

#define DPLL_CTRL2 0x6C05C
#define DPLL_CTRL2_DDI_CLK_OFF(port) (1 << ((port) + 15))
#define DPLL_CTRL2_DDI_CLK_SEL_MASK(port) (3 << ((port) * 3 + 1))
#define DPLL_CTRL2_DDI_CLK_SEL(clk, port) ((clk) << ((port) * 3 + 1))
#define DPLL_CTRL2_DDI_SEL_OVERRIDE(port) (1 << ((port) << 3))
#define DPLL_CTRL2_DDI_CLOCK_SELECT_SHIFT(i) (3 * (i) + 1)
#define DPLL_CTRL2_DDI_CLOCK_SELECT_MASK(i) (3 << DPLL_CTRL2_DDI_CLOCK_SELECT_SHIFT(i))

#define DPLL_STATUS 0x6C060
#define DPLL_STATUS_LOCK(i) (1 << ((i) * 8))

#define PIPE_MISC(i) (0x70030 + (0x1000 * i))
#define PRI_CTL(i) (0x70180 + (0x1000 * i))
#define DSP_ADDR(i) (0x70184 + (0x1000 * i))
#define PRI_STRIDE(i) (0x70188 + (0x1000 * i))
#define PLANE_SIZE(i) (0x70190 + (0x1000 * i))
#define PRI_SURFACE(i) (0x7019C + (0x1000 * i))
#define PLANE_WM_1(i) (0x70240 + (0x1000 * i))

#define PLANE_BUF_CFG_1_A 0x7027C
#define PLANE_BUF_CFG_1_B 0x7127C

#define SOUTH_CHICKEN1 0xC2000

#define SFUSE_STRAP 0xC2014

#define SOUTH_DSPCLK_GATE_D 0xC2020
#define SOUTH_DSPCLK_GATE_D_PCH_LP_PARTITION_LEVEL_DISABLE 0x1000

#define SDEISR 0xC4000
#define SDEIMR 0xC4004
#define SDEIER 0xC400C

#define SHOTPLUG_CTL 0xC4030
#define SHOTPLUG_CTL2 0xC403C

#define GMBUS0 0xC5100
#define GMBUS1 0xC5104
#define GMBUS2 0xC5108
#define GMBUS3 0xC510C

#define PP_STATUS 0xC7200
#define PP_STATUS_ON_STATUS (1u << 31)
#define PP_STATUS_GET_SEQUENCE_PROGRESS(v) (((v) >> 28) & 0x3)
#define PP_STATUS_SEQUENCE_NONE 0

#define PP_CONTROL 0xC7204
#define PP_CONTROL_ON (1 << 0)
#define PP_CONTROL_RESET (1 << 1)
#define PP_CONTROL_BACKLIGHT (1 << 2)
#define PP_CONTROL_FORCE_VDD (1 << 3)

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
