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

void kbl_transcoder_disable_by_id(LilGpu *gpu, LilTranscoder transcoder) {
	REG(trans(transcoder) + TRANS_CONF) &= ~(1 << 31);
	wait_for_bit_unset(REG_PTR(trans(transcoder) + TRANS_CONF), (1 << 30), 21000, 1000);
}

void kbl_transcoder_ddi_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(trans(crtc->transcoder) + TRANS_DDI_FUNC_CTL) &= 0xFFFFFFF;
	if(gpu->subgen == SUBGEN_GEMINI_LAKE) {
		// Quirk: delay for 100ms
		lil_sleep(100);
	}
}

void kbl_transcoder_ddi_disable_by_id(LilGpu *gpu, LilTranscoder transcoder) {
	REG(trans(transcoder) + TRANS_DDI_FUNC_CTL) &= 0xFFFFFFF;
	if(gpu->subgen == SUBGEN_GEMINI_LAKE) {
		// Quirk: delay for 100ms
		lil_sleep(100);
	}
}

void kbl_transcoder_clock_disable(LilGpu *gpu, LilCrtc *crtc) {
	if(crtc->transcoder != TRANSCODER_EDP && crtc->connector->type != EDP) {
		REG(TRANS_CLK_SEL(crtc->transcoder)) &= 0x1FFFFFFF;
	}
}

void kbl_transcoder_clock_disable_by_id(LilGpu *gpu, LilTranscoder transcoder) {
	REG(TRANS_CLK_SEL(transcoder)) &= 0x1FFFFFFF;
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

	switch(crtc->pipe_id) {
		case 0:
			REG(PLANE_BUF_CFG_1_A) = 0x009F0000;
			break;
		case 1:
			REG(PLANE_BUF_CFG_1_B) = 0x013F00A0;
			break;
	}

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

static void intel_reduce_m_n_ratio(uint32_t *num, uint32_t *den) {
	while (*num > 0xFFFFFF || *den > 0xFFFFFF) {
		*num >>= 1;
		*den >>= 1;
	}
}

static inline uint64_t div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t *remainder)
{
	union {
		uint64_t v64;
		uint32_t v32[2];
	} d = { dividend };
	uint32_t upper;

	upper = d.v32[1];
	d.v32[1] = 0;
	if (upper >= divisor) {
		d.v32[1] = upper / divisor;
		upper %= divisor;
	}
	asm ("divl %2" : "=a" (d.v32[0]), "=d" (*remainder) :
		"rm" (divisor), "0" (d.v32[0]), "1" (upper));
	return d.v64;
}

static inline uint64_t mul_u32_u32(uint32_t a, uint32_t b)
{
	uint32_t high, low;

	asm ("mull %[b]" : "=a" (low), "=d" (high)
			 : [a] "a" (a), [b] "rm" (b) );

	return low | ((uint64_t)high) << 32;
}

static void compute_m_n(uint32_t *ret_m, uint32_t *ret_n, uint32_t m, uint32_t n, uint32_t constant_n) {
	uint32_t rem;
	*ret_n = constant_n;
	*ret_m = div_u64_rem(mul_u32_u32(m, *ret_n), n, &rem);

	intel_reduce_m_n_ratio(ret_m, ret_n);
}

void kbl_transcoder_configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t max_lanes, uint32_t bpp, bool downspread) {
	uint32_t data_m, data_n;
	uint32_t link_m, link_n;
	uint64_t strm_clk = 10 * pixel_clock;
	uint32_t ls_clk = 10 * link_rate;

	compute_m_n(&data_m, &data_n, (3 * strm_clk * 8), ls_clk * max_lanes * 8, 0x8000000);
	compute_m_n(&link_m, &link_n, strm_clk, ls_clk, 0x80000);

	REG(trans(crtc->transcoder) + TRANS_DATAM) = data_m | (63 << 25);
	REG(trans(crtc->transcoder) + TRANS_DATAN) = data_n;
	REG(trans(crtc->transcoder) + TRANS_LINKM) = link_m;
	REG(trans(crtc->transcoder) + TRANS_LINKN) = link_n;
}
