#include <lil/imports.h>
#include <lil/intel.h>

#include "src/coffee_lake/dp.h"
#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

static bool ddi_in_use_by_hdport(LilGpu *gpu, enum LilDdiId ddi_id) {
	uint32_t val = REG(HDPORT_STATE);

	if((val & HDPORT_STATE_ENABLED) == 0) {
		return false;
	}

	uint32_t mask = 0;

	switch(ddi_id) {
		case DDI_A:
		case DDI_E:
			return false;
		case DDI_B:
			mask = (1 << 3);
			break;
		case DDI_C:
			mask = (1 << 5);
			break;
		case DDI_D:
			mask = (1 << 7);
			break;
	}

	return (val & mask);
}

static void hpd_enable(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t shotplug_val = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A:
			break;
		case DDI_B: {
			shotplug_val = REG(SHOTPLUG_CTL) | 0x10;
			REG(SHOTPLUG_CTL) = shotplug_val;
			break;
		}
		case DDI_C: {
			shotplug_val = REG(SHOTPLUG_CTL) | 0x1000;
			REG(SHOTPLUG_CTL) = shotplug_val;
			break;
		}
		case DDI_D: {
			shotplug_val = REG(SHOTPLUG_CTL) | 0x100000;
			REG(SHOTPLUG_CTL) = shotplug_val;
			break;
		}
		case DDI_E: {
			REG(SHOTPLUG_CTL2) |= 0x10;
			break;
		}
	}

	if(!crtc->connector->encoder->dp.vbios_hotplug_support) {
		return;
	}

	uint32_t sdeier_val = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_B: {
			REG(SDEIMR) &= ~0x200000;
			sdeier_val = REG(SDEIER) | 0x200000;
			break;
		}
		default: {
			lil_panic("unhandled DDI in hpd_enable");
		}
	}

	REG(SDEIER) = sdeier_val;
}

static bool hdmi_id_present_on_ddc(LilGpu *gpu, LilCrtc *crtc) {
	if(!crtc->connector->encoder->dp.aux_ch)
		return false;

	LilConnector *con = crtc->connector;

	uint8_t val = 0;
	if(!dp_dual_mode_read(gpu, con, 0x10, &val, 1))
		return false;

	lil_log(VERBOSE, "HDMI ID read = %#x\n", val);

	return (val & 0xF8) == 0xA8;
}

static bool unknown_init(LilGpu *gpu, LilCrtc *crtc) {
	LilEncoder *enc = crtc->connector->encoder;

	uint8_t data = 0;

	if(!dp_dual_mode_read(gpu, crtc->connector, 0x41, &data, 1))
		return false;

	if((data & 1))
		return true;

	if(!dp_dual_mode_read(gpu, crtc->connector, 0x40, &data, 1))
		return false;

	/* TODO: rest of this function (sub_5DE8) */
	lil_panic("TODO: unimplemented");

	return false;
}

bool kbl_dp_pre_enable(LilGpu *gpu, LilConnector *con) {
	LilEncoder *enc = con->encoder;

	if((gpu->variant == ULT || gpu->variant == ULX) && (con->ddi_id == DDI_D || con->ddi_id == DDI_E)) {
		lil_panic("unsupported DP configuration");
	}

	if(ddi_in_use_by_hdport(gpu, con->ddi_id)) {
		lil_log(ERROR, "kbl_dp_pre_enable: failing because ddi_in_use_by_hdport\n");
		return false;
	}

	if(!kbl_ddi_buf_enabled(gpu, con->crtc)) {
		hpd_enable(gpu, con->crtc);

		uint32_t sfuse_strap_mask = 0;
		switch(con->ddi_id) {
			case DDI_B: {
				sfuse_strap_mask = 4;
				break;
			}
			case DDI_C: {
				sfuse_strap_mask = 2;
				break;
			}
			case DDI_D: {
				sfuse_strap_mask = 1;
				break;
			}
			case DDI_A:
			case DDI_E:
				break;
		}

		if(gpu->pch_gen != NO_PCH && sfuse_strap_mask && (REG(SFUSE_STRAP) & sfuse_strap_mask) == 0) {
			lil_log(ERROR, "kbl_dp_pre_enable: failing because sfuse_strap_mask && (REG(SFUSE_STRAP) & sfuse_strap_mask) == 0\n");
			return false;
		}

		// bool init = unknown_init(gpu, con->crtc);

		//if(!hdmi_id_present_on_ddc(gpu, con->crtc) || init) {
		if(true) {
			dp_aux_native_write(gpu, con, SET_POWER, 1);
			uint8_t rev = dp_aux_native_read(gpu, con, DPCD_REV);

			lil_log(INFO, "DPCD rev %x\n", rev);
		}
	}

	// Read various display port parameters
	enc->dp.dp_max_link_rate = dp_aux_native_read(gpu, con, MAX_LINK_RATE);
	uint8_t raw_max_lane_count = dp_aux_native_read(gpu, con, MAX_LANE_COUNT);
	enc->dp.dp_lane_count = raw_max_lane_count & 0x1F;
	enc->dp.support_post_lt_adjust = raw_max_lane_count & (1 << 5);
	enc->dp.support_tps3_pattern = raw_max_lane_count & (1 << 6);
	enc->dp.support_enhanced_frame_caps = raw_max_lane_count & (1 << 7);
	lil_log(VERBOSE, "DPCD Info:\n");
	lil_log(VERBOSE, "\tmax_link_rate: %i\n", enc->dp.dp_max_link_rate);
	lil_log(VERBOSE, "\tlane_count: %i\n", enc->dp.dp_lane_count);
	lil_log(VERBOSE, "\tsupport_post_lt_adjust: %s\n", enc->dp.support_post_lt_adjust ? "yes" : "no");
	lil_log(VERBOSE, "\tsupport_tps3_pattern: %s\n", enc->dp.support_tps3_pattern ? "yes" : "no");
	lil_log(VERBOSE, "\tsupport_enhanced_frame_caps: %s\n", enc->dp.support_enhanced_frame_caps ? "yes" : "no");
	
	return true;
}
