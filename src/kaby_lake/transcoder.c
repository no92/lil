#include <lil/intel.h>

#include "src/kaby_lake/inc/kbl.h"
#include "src/regs.h"

static uint32_t trans(LilTranscoder id) {
	switch(id) {
		case TRANSCODER_A: return TRANSCODER_A_BASE;
		case TRANSCODER_B: return TRANSCODER_B_BASE;
		case TRANSCODER_C: return TRANSCODER_C_BASE;
		case TRANSCODER_EDP: return TRANSCODER_EDP_BASE;
		default: lil_panic("invalid transcoder");
	}
}

void kbl_transcoder_enable(LilGpu *gpu, LilCrtc *crtc) {
	REG(trans(crtc->transcoder) + TRANS_CONF) |= (1 << 31);
}

void kbl_transcoder_disable(LilGpu *gpu, LilCrtc *crtc) {
	// if(crtc->transcoder == TRANSCODER_EDP)
		// return;

	REG(trans(crtc->transcoder) + TRANS_CONF) &= ~(1 << 31);

	wait_for_bit_unset(REG_PTR(trans(crtc->transcoder) + TRANS_CONF), (1 << 30), 21000, 1000);
}

void kbl_transcoder_ddi_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) &= 0xFFFFFFF;
}

void kbl_transcoder_clock_disable(LilGpu *gpu, LilCrtc *crtc) {
	if(crtc->transcoder != TRANSCODER_EDP && crtc->connector->type != EDP) {
		REG(TRANS_CLK_SEL(crtc->transcoder)) &= 0x1FFFFFFF;
	}
}

void kbl_transcoder_configure_clock(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t val = 0;

	if(crtc->transcoder == TRANSCODER_EDP)
		return;

	switch(crtc->connector->ddi_id) {
		case DDI_A: val = 0x00000000; break;
		case DDI_B: val = 0x40000000; break;
		case DDI_C: val = 0x60000000; break;
		case DDI_D: val = 0x80000000; break;
		case DDI_E: val = 0xA0000000; break;
	}

	REG(TRANS_CLK_SEL(crtc->transcoder)) = (REG(TRANS_CLK_SEL(crtc->transcoder)) & 0x1FFFFFFF) | val;
}

void kbl_transcoder_timings_configure(LilGpu *gpu, LilCrtc *crtc) {
	LilModeInfo *mode = &crtc->current_mode;

	REG(trans(crtc->transcoder) + TRANS_HTOTAL) = ((mode->htotal - 1) << 16) | (mode->hactive - 1);
	REG(trans(crtc->transcoder) + TRANS_HBLANK) = ((mode->htotal - 1) << 16) | (mode->hactive - 1);
	REG(trans(crtc->transcoder) + TRANS_HSYNC) = ((mode->hsyncEnd - 1) << 16) | (mode->hsyncStart - 1);
	REG(trans(crtc->transcoder) + TRANS_VTOTAL) = ((mode->vtotal - 1) << 16) | (mode->vactive - 1);
	REG(trans(crtc->transcoder) + TRANS_VBLANK) = ((mode->vtotal - 1) << 16) | (mode->vactive - 1);
	REG(trans(crtc->transcoder) + TRANS_VSYNC) = ((mode->vsyncEnd - 1) << 16) | (mode->vsyncStart - 1);

	REG(trans(crtc->transcoder) + TRANS_CONF) &= 0xFF9FFFFF;

	/* TODO: handle interlaced modes here? */
}

void kbl_transcoder_bpp_set(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	uint32_t val = 0;

	switch(bpp) {
		case 18:
			val = 0x200000;
			break;
		case 24:
			val = 0;
			break;
		case 30:
			val = 0x100000;
			break;
		case 36:
			val = 0x300000;
			break;
		default:
			lil_panic("unsupported bpp");
	}

	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0xFF8FFFFF) | val;
}

void kbl_transcoder_set_dp_msa_misc(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	if(crtc->connector->type != EDP && crtc->connector->type != DISPLAYPORT) {
		return;
	}

	uint32_t val = 0;

	switch(bpp) {
		case 18:
			val = 0;
			break;
		case 24:
			val = 0x20;
			break;
		case 30:
			val = 0x40;
			break;
		case 36:
			val = 0x60;
			break;
		default:
			lil_panic("unsupported bpp");
	}

	REG(trans(crtc->transcoder) + TRANS_MSA_MISC) = val | 1;
}

void kbl_transcoder_ddi_polarity_setup(LilGpu *gpu, LilCrtc *crtc) {
	if(crtc->current_mode.hsyncPolarity && crtc->current_mode.vsyncPolarity) {
		uint32_t val = REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL);

		if(crtc->current_mode.hsyncPolarity == 2)
			val |= 0x10000;
		else
			val &= ~0x10000;

		REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = val;
		val = REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL);

		if(crtc->current_mode.vsyncPolarity == 2)
			val |= 0x20000;
		else
			val &= ~0x20000;

		REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = val;
	}
}

void kbl_transcoder_ddi_setup(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes) {
	uint32_t val = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A:
			val = 0;
			break;
		case DDI_B:
			val = 0x10000000;
			break;
		case DDI_C:
			val = 0x20000000;
			break;
		case DDI_D:
			val = 0x30000000;
			break;
		case DDI_E:
			val = 0x40000000;
			break;
	}

	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0x8FFFFFFF) | val;

	if(crtc->transcoder == TRANSCODER_EDP) {
		switch(crtc->pipe_id) {
			case 0:
				val = 0;
				break;
			case 1:
				val = 0x5000;
				break;
			case 2:
				val = 0x6000;
				break;
			default:
				lil_panic("unhandled pipe");
		}

		REG(TRANSCODER_EDP_BASE + TRANS_DDI_FUNC_CTL) = val;
	}

	switch(crtc->connector->type) {
		case HDMI:
			REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0xF8FFFFFF);
			break;
		case DISPLAYPORT:
		case EDP:
			REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0xF8FFFFFF) | 0x2000000;
			break;
		default:
			lil_panic("unimplemented connector type");
	}

	switch(lanes) {
		case 0:
			goto trans_ddi_enable;
		case 1:
			val = 0;
			break;
		case 2:
			val = 2;
			break;
		case 4:
			val = 6;
			break;
		default:
			lil_panic("invalid lanes");
	}

	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0xFFFFFFF1) | val;
trans_ddi_enable:
	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) |= (1 << 31);
}

void kbl_transcoder_configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t max_lanes, uint32_t bpp, bool downspread) {
	uint64_t strm_clk = 10 * pixel_clock;
	uint32_t ls_clk = 10 * link_rate;

	if(downspread) {
		strm_clk = 10025 * strm_clk / 10000;
	}

	uint32_t ls_clk_lanes = ls_clk * max_lanes;
	uint64_t linkm = (strm_clk << 19) / ls_clk;
	uint64_t datam = (((strm_clk * bpp) << 23) / ls_clk_lanes) >> 3;

	REG(trans(crtc->transcoder) + TRANS_DATAM) = datam | 0x7E000000;
	REG(trans(crtc->transcoder) + TRANS_DATAN) = 0x800000;
	REG(trans(crtc->transcoder) + TRANS_LINKM) = linkm;
	REG(trans(crtc->transcoder) + TRANS_LINKN) = 0x80000;
}
