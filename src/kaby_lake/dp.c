#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/inc/dpcd.h"
#include "src/kaby_lake/inc/kbl.h"
#include "src/kaby_lake/inc/hpd.h"
#include "src/edid.h"
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
		kbl_hpd_enable(gpu, con->crtc);

		// Gemini Lake and Broxton do not have SFUSE_STRAP.
		if(gpu->subgen != SUBGEN_GEMINI_LAKE) {
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

			if(sfuse_strap_mask && (REG(SFUSE_STRAP) & sfuse_strap_mask) == 0) {
				lil_log(ERROR, "kbl_dp_pre_enable: failing because sfuse_strap_mask && (REG(SFUSE_STRAP) & sfuse_strap_mask) == 0\n");
				return false;
			}
		}
		
		bool init = false; // TODO: implement unknown_init(gpu, con->crtc);
		
		bool hdmi_id_present = hdmi_id_present_on_ddc(gpu, con->crtc);
		lil_log(DEBUG, "kbl_dp_pre_enable: init=%s, hdmi_id_present=%s\n", init ? "true" : "false", hdmi_id_present ? "true" : "false");

		if(!hdmi_id_present || init) {
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

bool lil_kbl_dp_is_connected (struct LilGpu* gpu, struct LilConnector* connector) {
	return false;

	// TODO(): not reliable, only valid on chip startup, also wrong on some generations for some reason 
	if(connector->ddi_id == DDI_A) {
		return REG(DDI_BUF_CTL(DDI_A)) & DDI_BUF_CTL_DISPLAY_DETECTED;
	}

	uint32_t sfuse_mask = 0;
	uint32_t sde_isr_mask = 0;

	switch(connector->ddi_id) {
		case DDI_B: {
			sde_isr_mask = (1 << 4);
			sfuse_mask = (1 << 2);
			break;
		}
		case DDI_C: {
			sde_isr_mask = (1 << 5);
			sfuse_mask = (1 << 1);
			break;
		}
		case DDI_D: {
			sfuse_mask = (1 << 0);
			break;
		}
		default: {
			lil_panic("unhandled DDI id");
		}
	}

	if((REG(SFUSE_STRAP) & sfuse_mask) == 0) {
		return false;
	}

	return REG(SDEISR) & sde_isr_mask;
}

// TODO(SEPERATE;MAYBE) move this to dpcd.cpp? would mean not having to include edid in dpcd.h
LilConnectorInfo lil_kbl_dp_get_connector_info(struct LilGpu* gpu, struct LilConnector* connector) {
    (void)connector;
    LilConnectorInfo ret = {};
    LilModeInfo* info = (LilModeInfo *) lil_malloc(sizeof(LilModeInfo) * 4);

    DisplayData edid = {};
    dp_aux_read_edid(gpu, connector, &edid);

    uint32_t edp_max_pixel_clock = 990 * gpu->boot_cdclk_freq;

    int j = 0;
    for(int i = 0; i < 4; i++) { // Maybe 4 Detailed Timings
        if(edid.detailTimings[i].pixelClock == 0)
            continue; // Not a timing descriptor

        if(connector->type == EDP && edp_max_pixel_clock && (edid.detailTimings[i].pixelClock * 10) > edp_max_pixel_clock) {
            lil_log(WARNING, "EDID: skipping detail timings %u: pixel clock (%u KHz) > max (%u KHz)\n", i, (edid.detailTimings[i].pixelClock * 10), edp_max_pixel_clock);
            continue;
        }

        edid_timing_to_mode(&edid, edid.detailTimings[i], &info[j++]);
    }

    ret.modes = info;
    ret.num_modes = j;
    return ret;
}
