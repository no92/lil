#include <lil/intel.h>
#include <lil/imports.h>

#include "src/kaby_lake/inc/kbl.h"
#include "src/regs.h"

struct vco_lookup {
	unsigned int decimal;
	unsigned int integer;
};

static struct vco_lookup vco8100_lookup[] = {
	{ 0x2A1, 0x152 },
	{ 0x382, 0x1C2 },
	{ 0x436, 0x21C },
	{ 0x544, 0x2A3 },
};

static struct vco_lookup vco8640_lookup[] = {
	{ 0x267, 0x135 },
	{ 0x35E, 0x1B0 },
	{ 0x436, 0x21C },
	{ 0x4D0, 0x26A },
};

uint32_t kbl_cdclk_dec_to_int(uint32_t cdclk_freq_decimal) {
	for(size_t i = 0; i < 4; i++) {
		if(vco8100_lookup[i].decimal == cdclk_freq_decimal)
			return vco8100_lookup[i].integer;
		if(vco8640_lookup[i].decimal == cdclk_freq_decimal)
			return vco8640_lookup[i].integer;
	}

	return 0;
}

void kbl_cdclk_set_freq(LilGpu *gpu, uint32_t cdclk_freq_int) {
	uint32_t cdfreq_decimal = 0;
	uint32_t cdfreq_select = 0;

	switch(cdclk_freq_int) {
		case 309: {
			cdfreq_decimal = 615;
			cdfreq_select = 0x8000000;
			break;
		}
		case 338: {
			cdfreq_decimal = 673;
			cdfreq_select = 0x8000000;
			break;
		}
		case 432: {
			cdfreq_decimal = 862;
			cdfreq_select = 0;
			break;
		}
		case 450: {
			cdfreq_decimal = 898;
			cdfreq_select = 0;
			break;
		}
		case 540: {
			cdfreq_decimal = 1078;
			cdfreq_select = 0x4000000;
			break;
		}
		case 618: {
			cdfreq_decimal = 1232;
			cdfreq_select = 0xC000000;
			break;
		}
		case 675: {
			cdfreq_decimal = 1348;
			cdfreq_select = 0xC000000;
			break;
		}
		default: {
			lil_log(ERROR, "cdclk_int=%u\n", cdclk_freq_int);
			lil_panic("unhandled cdclk");
		}
	}

	uint32_t big_timeout = 3000;
	uint32_t data0 = 0;
	uint32_t data1 = 0;

	while(1) {
		uint32_t pcode_max_timeout = (big_timeout < 150) ? big_timeout : 150;
		uint32_t pcode_real_timeout = pcode_max_timeout;

		data0 = 3;

		if(!kbl_pcode_rw(gpu, &data0, &data1, 7, &pcode_real_timeout))
			lil_panic("cdclk pcode_rw failed");

		if(data0 & 1) {
			break;
		}

		lil_usleep(10);

		big_timeout = (big_timeout + pcode_real_timeout - pcode_max_timeout) - 10;

		if(!big_timeout)
			lil_panic("timeout in cdclk_set_freq");
	}

	REG(CDCLK_CTL) = cdfreq_decimal | cdfreq_select | (REG(CDCLK_CTL) & 0xF3FFF800);

	lil_usleep(10);

	switch(cdfreq_select) {
		case 0: {
			data0 = 1;
			break;
		}
		case 0x4000000: {
			data0 = 2;
			break;
		}
		case 0x8000000: {
			data0 = 0;
			break;
		}
		case 0xC000000: {
			data0 = 3;
			break;
		}
		default:
			lil_panic("invalid cdfreq_select");
	}

	data1 = 0;

	uint32_t timeout = 100;

	if(!kbl_pcode_rw(gpu, &data0, &data1, 7, &timeout))
		lil_panic("timeout in cdclk set");

	gpu->cdclk_freq = cdclk_freq_int;
}

void kbl_cdclk_set_for_pixel_clock(LilGpu *gpu, uint32_t *pixel_clock) {
	struct vco_lookup *table = (gpu->vco_8640) ? vco8640_lookup : vco8100_lookup;
	size_t offset = 0;
	uint32_t clock = *pixel_clock;

	while(clock >= 990 * table[offset].integer) {
		if(++offset >= 4) {
			lil_panic("VCO table lookup failed");
		}
	}

	uint32_t vco_val = table[offset].integer;

	if(vco_val != gpu->cdclk_freq) {
		kbl_cdclk_set_freq(gpu, vco_val);
	}
}

void kbl_dpll_clock_set(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t clock_select = 0;
	uint32_t clock_ungate = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 2;
					break;
				case WRPLL1:
					clock_select = 4;
					break;
				case WRPLL2:
					clock_select = 6;
					break;
			}
			REG(DPLL_CTRL2) &= 0xFFFF7FF8;
			REG(DPLL_CTRL2) |= clock_select | 0x8001;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFF7FFF;
			break;
		}
		case DDI_B: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x10;
					break;
				case WRPLL1:
					clock_select = 0x20;
					break;
				case WRPLL2:
					clock_select = 0x30;
					break;
			}
			REG(DPLL_CTRL2) &= 0xFFFEFFC7;
			REG(DPLL_CTRL2) |= clock_select | 0x10008;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFEFFFF;
			break;
		}
		case DDI_C: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x80;
					break;
				case WRPLL1:
					clock_select = 0x100;
					break;
				case WRPLL2:
					clock_select = 0x180;
					break;
			}
			REG(DPLL_CTRL2) &= 0xFFFDFE3F;
			REG(DPLL_CTRL2) |= clock_select | 0x20040;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFDFFFF;
			break;
		}
		case DDI_D: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x400;
					break;
				case WRPLL1:
					clock_select = 0x800;
					break;
				case WRPLL2:
					clock_select = 0xC00;
					break;
			}
			REG(DPLL_CTRL2) &= 0xFFFBF1FF;
			REG(DPLL_CTRL2) |= clock_select | 0x40200;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFBFFFF;
			break;
		}
		case DDI_E: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x2000;
					break;
				case WRPLL1:
					clock_select = 0x4000;
					break;
				case WRPLL2:
					clock_select = 0x6000;
					break;
			}
			REG(DPLL_CTRL2) &= 0xFFF78FFF;
			REG(DPLL_CTRL2) |= clock_select | 0x81000;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFF7FFFF;
			break;
		}
	}

	REG(DPLL_CTRL2) = clock_ungate;
}

static bool kbl_dpll_is_locked(LilGpu *gpu, enum LilPllId pll_id) {
	uint32_t mask = 0;

	switch(pll_id) {
		case LCPLL1:
			mask = 1;
			break;
		case LCPLL2:
			mask = 0x100;
			break;
		case WRPLL1:
			mask = 0x10000;
			break;
		case WRPLL2:
			mask = 0x1000000;
			break;
	}

	return (REG(DPLL_STATUS) & mask) != 0;
}

void kbl_dpll_ctrl_enable(LilGpu *gpu, LilCrtc *crtc, uint32_t link_rate) {
	uint32_t dpll_link_rate = 0;

	switch(link_rate) {
		case 162000u:
			dpll_link_rate = 2;
			break;
		case 216000u:
			dpll_link_rate = 4;
			break;
		case 270000u:
			dpll_link_rate = 1;
			break;
		case 324000u:
			dpll_link_rate = 3;
			break;
		case 432000u:
			dpll_link_rate = 5;
			break;
	}

	uint32_t set_dpll_ctrl = 0;
	uint32_t dpll_ctrl_val = 0;

	LilEncoder *enc = crtc->connector->encoder;

	switch(crtc->pll_id) {
		case LCPLL1: {
			lil_assert(crtc->connector->type == EDP || crtc->connector->type == DISPLAYPORT);
			set_dpll_ctrl = (2 * dpll_link_rate) | 1;
			REG(LCPLL1_CTL) &= ~(1 << 31);
			dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFFFFF0;
			break;
		}
		case LCPLL2: {
			uint32_t dpll1_flags = 0x40;
			lil_log(VERBOSE, "configuring LCPLL2\n");
			//if(crtc->connector->type != EDP ) {
			//	lil_assert(crtc->connector->type == HDMI);
			if(crtc->connector->type == HDMI) {
				lil_log(VERBOSE, "\tfor HDMI\n");
				set_dpll_ctrl = dpll1_flags | 0x800;
				dpll_ctrl_val = (REG(DPLL_CTRL1) & 0xFFFFF3BF);
				break;
			}

			if(enc->edp.edp_downspread) {
				dpll1_flags = 0x440;
			}

			set_dpll_ctrl = (dpll_link_rate << 7) | dpll1_flags;
			dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFFF03F;
			break;
		}
		case WRPLL1: {
			uint32_t dpll1_flags = 0x1000;
			if(crtc->connector->type != EDP)
				lil_panic("non-eDP is unimplemented");
			if(enc->edp.edp_downspread)
				dpll1_flags = 0x11000;
			set_dpll_ctrl = (dpll_link_rate << 13) | dpll1_flags;
        	dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFC0FFF;
			break;
		}
		case WRPLL2: {
			uint32_t dpll1_flags = 0x40000;
			if(crtc->connector->type != EDP)
				lil_panic("non-eDP is unimplemented");
			if(enc->edp.edp_downspread)
				dpll1_flags = 0x440000;
			set_dpll_ctrl = (dpll_link_rate << 19) | dpll1_flags;
        	dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFF03FFFF;
			break;
		}
	}

	lil_log(VERBOSE, "setting DPLL_CTRL1 = %#010x\n", set_dpll_ctrl | dpll_ctrl_val);
	REG(DPLL_CTRL1) = set_dpll_ctrl | dpll_ctrl_val;

	if(crtc->connector->type == HDMI) {
		hdmi_pll_enable_sequence(gpu, crtc);
	}

	if(crtc->connector->type == EDP) {
		uint32_t data0 = 0;
		uint32_t data1 = 0;
		bool pcode_write = true;

		if(link_rate == 216000 || link_rate == 432000) {
			if(enc->edp.edp_downspread) {
				switch(crtc->pll_id) {
					case LCPLL1:
						data0 = 0;
						break;
					case LCPLL2:
						data0 = 1;
						break;
					case WRPLL1:
						data0 = 2;
						break;
					case WRPLL2:
						data0 = 3;
						break;
				}

			} else {
				pcode_write = false;
			}
		} else {
			data0 = 5;
		}

		if(pcode_write) {
			uint32_t timeout = 150;
			kbl_pcode_rw(gpu, &data0, &data1, 0x23, &timeout);
		}
	}

	uint32_t pll_ctl = 0;

	switch(crtc->pll_id) {
		case LCPLL1:
			pll_ctl = LCPLL1_CTL;
			break;
		case LCPLL2:
			pll_ctl = LCPLL2_CTL;
			break;
		case WRPLL1:
			pll_ctl = WRPLL_CTL1;
			break;
		case WRPLL2:
			pll_ctl = WRPLL_CTL2;
			break;
	}

	REG(pll_ctl) |= (1 << 31);
	lil_usleep(3000);

	for(size_t pll_enable_timeout = 3000; pll_enable_timeout; pll_enable_timeout -= 100) {
		lil_usleep(100);

		if(kbl_dpll_is_locked(gpu, crtc->pll_id)) {
			break;
		}
	}
}
