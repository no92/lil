#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/inc/dpcd.h"
#include "src/kaby_lake/inc/kbl.h"
#include "src/regs.h"

static uint8_t dp_link_rate_for_crtc(LilGpu *gpu, struct LilCrtc* crtc) {
	uint8_t last_valid_index = 0;
	uint32_t link_rate = 0;
	uint32_t max_link_rate = 0;
	LilEncoder *enc = crtc->connector->encoder;

	for(uint8_t i = 0; i < enc->edp.supported_link_rates_len; i++) {
		uint16_t entry = enc->edp.supported_link_rates[i];

		if(entry == 8100 || entry == 10800 || (entry != 12150 && (entry == 1350 || entry == 16200 || entry == 21600 || entry == 27000))) {
			link_rate = 8 * enc->edp.edp_lane_count * (1000 * entry / 5) / (10 * enc->edp.edp_color_depth);

			if(link_rate >= crtc->current_mode.clock && link_rate < max_link_rate) {
				last_valid_index = i;
				max_link_rate = link_rate;
			}
		}
	}

	return last_valid_index;
}

static void kbl_unmask_vblank(LilGpu *gpu, LilCrtc *crtc) {
	REG(IMR(crtc->pipe_id)) &= ~1;
}

static bool pll_available(LilGpu *gpu, uint32_t reg) {
	uint32_t hdport_state = REG(HDPORT_STATE);
	bool hdport_in_use = false;

	lil_log(VERBOSE, "pll_available: hdport_state: %u\n", hdport_state);
	lil_log(VERBOSE, "pll_available: reg=%x, REG(reg)=%u\n", reg, REG(reg));

	/* check if HDPORT pre-emption is enabled to begin with */
	if(!(hdport_state & HDPORT_STATE_ENABLED)) {
		/* if not, we can just look for the enable flag */
		lil_log(VERBOSE, "pll_available: hdport pre-emption enabled, returning enabled bit\n");
		return !(REG(reg) & (1 << 31));
	}

	/* check if our DPLL has been preempted for HDPORT */
	switch(reg) {
		case LCPLL1_CTL: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL0_USED;
			break;
		}
		case LCPLL2_CTL: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL1_USED;
			break;
		}
		case WRPLL_CTL1: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL3_USED;
			break;
		}
		case WRPLL_CTL2: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL2_USED;
			break;
		}
		default: {
			return !(REG(reg) & (1 << 31));
		}
	}

	/* used for HDPORT */
	if(hdport_in_use)
		return false;

	return !(REG(reg) & (1 << 31));
}

void pll_find(LilGpu *gpu, LilCrtc *crtc) {
	if(gpu->subgen == SUBGEN_GEMINI_LAKE && crtc->connector->type != EDP) {
		// Gemini Lake has a fixed mapping from pipe to PLL
		switch(crtc->transcoder) {
			case TRANSCODER_A: {
				crtc->pll_id = LCPLL1;
				return;
			}
			case TRANSCODER_B: {
				crtc->pll_id = LCPLL2;
				return;
			}
			case TRANSCODER_C: {
				crtc->pll_id = WRPLL1;
				return;
			}
			default: {
				lil_panic("TODO: add more cases here");
			}
		}
		lil_panic("should not be reached");
	}

	if(crtc->connector->type == EDP && !crtc->connector->encoder->edp.edp_downspread) {
		crtc->pll_id = LCPLL1;
		return;
	}

	if(!pll_available(gpu, LCPLL2_CTL)) {
		if(!pll_available(gpu, WRPLL_CTL2)) {
			if(!pll_available(gpu, WRPLL_CTL1)) {
				lil_panic("no PLL available");
			} else {
				crtc->pll_id = WRPLL1;
				return;
			}
		} else {
			crtc->pll_id = WRPLL2;
			return;
		}
	} else {
		crtc->pll_id = LCPLL2;
		return;
	}
}

void lil_kbl_crtc_dp_shutdown(LilGpu *gpu, LilCrtc *crtc) {
	switch(crtc->connector->type) {
		case DISPLAYPORT:
		case EDP: {
			kbl_plane_disable(gpu, crtc);
			kbl_transcoder_disable(gpu, crtc);
			kbl_transcoder_ddi_disable(gpu, crtc);
			kbl_pipe_scaler_disable(gpu, crtc);
			kbl_transcoder_clock_disable(gpu, crtc);

			REG(DDI_BUF_CTL(crtc->connector->ddi_id)) &= ~0x10000;
			REG(DDI_BUF_CTL(crtc->connector->ddi_id)) &= ~0x80000000;

			kbl_ddi_balance_leg_set(gpu, crtc->connector->ddi_id, 0);

			REG(DP_TP_CTL(crtc->connector->ddi_id)) &= ~0x10000;
			lil_usleep(10);
			kbl_ddi_power_disable(gpu, crtc->connector);
			kbl_ddi_clock_disable(gpu, crtc);

			if(crtc->connector->type == DISPLAYPORT) {
				kbl_pll_disable(gpu, crtc->connector);
			}
			break;
		}
		default: {
			lil_panic("connector type unhandled");
		}
	}
}

void lil_kbl_commit_modeset(struct LilGpu* gpu, struct LilCrtc* crtc) {
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	if(enc->edp.dynamic_cdclk_supported && gpu->connectors[0].type == EDP) {
		kbl_cdclk_set_for_pixel_clock(gpu, &crtc->current_mode.clock);
	} else {
		if(gpu->cdclk_freq != gpu->boot_cdclk_freq) {
			kbl_cdclk_set_freq(gpu, gpu->boot_cdclk_freq);
		}
	}

	if(crtc->current_mode.bpp == -1) {
		if(con->type == EDP) {
			crtc->current_mode.bpp = enc->edp.edp_color_depth;
		} else {
			lil_panic("crtc->current_mode.bpp == -1\n");
		}
	}

	pll_find(gpu, crtc);

	REG(SWF_24) = 8;

	uint32_t stride = (crtc->current_mode.hactive * 4 + 63) >> 6;

	if(crtc->planes[0].enabled)
		kbl_plane_enable(gpu, crtc, false);

	REG(PRI_CTL(crtc->pipe_id)) = (REG(PRI_CTL(crtc->pipe_id)) & 0xF0FFFFFF) | 0x4000000;

	uint8_t link_rate_index = 0;

	if(con->type == EDP && enc->edp.edp_dpcd_rev >= 3) {
		link_rate_index = dp_link_rate_for_crtc(gpu, crtc);
		uint16_t looked_up_link_rate = enc->edp.supported_link_rates[link_rate_index];

		if(((looked_up_link_rate == 10800 || looked_up_link_rate == 21600) && !gpu->vco_8640) ||
			((looked_up_link_rate != 10800 && looked_up_link_rate != 21600) && gpu->vco_8640)) {
			lil_panic("unimplemented");
		}
	}

	uint32_t lanes = 0;
	uint32_t max_lanes;
	if(con->type == EDP) {
		max_lanes = enc->edp.edp_max_lanes;
	} else {
		lanes = enc->dp.dp_lane_count;
		max_lanes = enc->dp.dp_lane_count;
		lil_log(VERBOSE, "lil_kbl_commit_modeset: dp lanes=%u, max_lanes=%u\n", lanes, max_lanes);
	}

	if(con->type == EDP) {
		if(enc->edp.edp_fast_link_training_supported) {
			lanes = enc->edp.edp_fast_link_lanes;
		} else {
			lanes = enc->edp.edp_lane_count & 0x1F;
		}
	}
	if(lanes < max_lanes)
		max_lanes = lanes;

	uint32_t link_rate_mhz = 0;

	if(con->type == EDP) {
		if(enc->edp.edp_dpcd_rev >= 3) {
			lil_log(VERBOSE, "edp_dpcd_rev >= 3: link_rate_index=%u\n", link_rate_index);
			enc->edp.edp_max_link_rate = link_rate_index;
			link_rate_mhz = 200 * enc->edp.supported_link_rates[link_rate_index];
		} else {
			switch(enc->edp.edp_max_link_rate) {
				case 6:
					link_rate_mhz = 1620000;
					break;
				case 10:
					link_rate_mhz = 2700000;
					break;
				case 20:
					link_rate_mhz = 5400000;
					break;
				default:
					lil_log(VERBOSE, "edp_max_link_rate=%u\n", enc->edp.edp_max_link_rate);
					lil_panic("invalid edp_max_link_rate");
			}
		}
	} else {
		switch(enc->dp.dp_max_link_rate) {
			case 6:
				link_rate_mhz = 1620000;
				break;
			case 10:
				link_rate_mhz = 2700000;
				break;
			case 20:
				link_rate_mhz = 5400000;
				break;
			default:
				lil_log(VERBOSE, "invalid enc->dp.dp_max_link_rate=%u\n", enc->dp.dp_max_link_rate);
				lil_panic("invalid dp_max_link_rate");
		}
	}

	if(!link_rate_mhz)
		lil_panic("invalid link_rate_mhz == 0");

	dp_aux_native_write(gpu, con, LANE_COUNT_SET, lanes);
	// dp_aux_native_write(gpu, con, LINK_RATE_SET, enc->dp.dp_max_link_rate);

	while((REG(PP_STATUS) & 0x38000000) != 0);
	REG(PP_CONTROL) |= 1;
	kbl_edp_aux_readable(gpu, con);
	REG(PP_CONTROL) &= ~8;

	uint32_t link_rate = link_rate_mhz / 10;

	uint32_t bpp;
	if(con->type == EDP) {
		bpp = enc->edp.edp_color_depth;
	} else {
		bpp = crtc->current_mode.bpp;
	}

	kbl_edp_validate_clocks_for_bpp(gpu, crtc, max_lanes, link_rate, &bpp);

	if(REG(DP_TP_CTL(con->ddi_id)) & (1 << 31)) {
		uint32_t dp_tp_ctl_val = (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFFF8FF) | 0x200;
		REG(DP_TP_CTL(con->ddi_id)) = dp_tp_ctl_val;
		lil_usleep(17000);
		REG(DP_TP_CTL(con->ddi_id)) = dp_tp_ctl_val & 0x7FFFFFFF;
	}

	kbl_dpll_ctrl_enable(gpu, crtc, link_rate);

	if(max_lanes == 4)
		REG(DDI_BUF_CTL(con->ddi_id)) |= 0x10;

	kbl_dpll_clock_set(gpu, crtc);
	kbl_ddi_power_enable(gpu, crtc);

	uint32_t dp_tp_ctl_flags = 0;

	if(con->type == EDP) {
		if(enc->edp.edp_lane_count & 0x80) {
			dp_tp_ctl_flags = 0x40000;
		}
	}

	REG(DP_TP_CTL(con->ddi_id)) = dp_tp_ctl_flags | (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFBF8FF);
	REG(DP_TP_CTL(con->ddi_id)) |= (1 << 31);
	REG(DDI_BUF_CTL(con->ddi_id)) = (2 * max_lanes - 2) | (REG(DDI_BUF_CTL(con->ddi_id)) & 0xFFFFFFF1);

	if(con->type == EDP) {
		uint32_t dispio_cr_tx_bmu_cr0 = 0;

		if(enc->edp.edp_port_reversal) {
			REG(DDI_BUF_CTL(con->ddi_id)) |= 0x10000;
		}

		if(enc->edp.edp_iboost) {
			kbl_ddi_balance_leg_set(gpu, con->ddi_id, enc->edp.edp_balance_leg_val);

			if(max_lanes == 4)
				dispio_cr_tx_bmu_cr0 = (REG(DISPIO_CR_TX_BMU_CR0) & 0xFF8FFFFF) | (enc->edp.edp_balance_leg_val << 20);
		} else if(enc->edp.edp_vswing_preemph == 1) {
			if(gpu->variant == ULX && gpu->gen == GEN_KBL) {
				kbl_ddi_balance_leg_set(gpu, con->ddi_id, 3);

				if(max_lanes == 4)
					dispio_cr_tx_bmu_cr0 = (REG(DISPIO_CR_TX_BMU_CR0) & 0xFF8FFFFF) | 0x300000;
			} else {
				kbl_ddi_balance_leg_set(gpu, con->ddi_id, 1);

				if(max_lanes == 4)
					dispio_cr_tx_bmu_cr0 = (REG(DISPIO_CR_TX_BMU_CR0) & 0xFF8FFFFF) | 0x100000;
			}
		} else {
			kbl_ddi_balance_leg_set(gpu, con->ddi_id, 0);

			if(max_lanes == 4)
				dispio_cr_tx_bmu_cr0 = (REG(DISPIO_CR_TX_BMU_CR0) & 0xFF8FFFFF);
		}

		if(dispio_cr_tx_bmu_cr0)
			REG(DISPIO_CR_TX_BMU_CR0) = dispio_cr_tx_bmu_cr0;
	}

	REG(DDI_BUF_CTL(con->ddi_id)) |= (1 << 31);
	lil_usleep(518);

	if(con->type == EDP) {
		if(enc->edp.edp_fast_link_training_supported) {
			lil_panic("fast eDP link training unsupported");
		} else {
			if(!kbl_edp_link_training(gpu, crtc, enc->edp.edp_max_link_rate, enc->edp.edp_lane_count))
				lil_panic("eDP link training failed");
		}
	} else {
		if(!kbl_edp_link_training(gpu, crtc, enc->dp.dp_max_link_rate, enc->dp.dp_lane_count))
			lil_panic("DP link training failed");
	}
	REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFFF8FF) | 0x300;
	kbl_transcoder_configure_clock(gpu, crtc);
	kbl_pipe_src_size_set(gpu, crtc);
	kbl_plane_size_set(gpu, crtc);

	kbl_unmask_vblank(gpu, crtc);
	kbl_transcoder_timings_configure(gpu, crtc);
	kbl_transcoder_bpp_set(gpu, crtc, bpp);
	kbl_pipe_dithering_enable(gpu, crtc, bpp);
	kbl_transcoder_configure_m_n(gpu, crtc, crtc->current_mode.clock, link_rate, max_lanes, bpp, enc->edp.edp_downspread);
	kbl_transcoder_ddi_polarity_setup(gpu, crtc);

	/* TODO: while setting this is correct, it garbles the output for unknown reasons */
	// kbl_transcoder_set_dp_msa_misc(gpu, crtc, bpp);

	kbl_transcoder_ddi_setup(gpu, crtc, max_lanes);
	kbl_transcoder_enable(gpu, crtc);
	if(crtc->planes[0].enabled) 
		kbl_plane_enable(gpu, crtc, true);

	if(con->type == EDP) {
		if(enc->edp.t8 > enc->edp.pwm_on_to_backlight_enable) {
			lil_usleep(100 * (enc->edp.t8 - enc->edp.pwm_on_to_backlight_enable));
		}

		if(enc->edp.backlight_control_method_type == 2) {
			uint32_t blc_pwm_pipe = 0;

			if(crtc->pipe_id == 1) {
				blc_pwm_pipe = 0x20000000;
			} else if(crtc->pipe_id == 2) {
				blc_pwm_pipe = 0x40000000;
			}

			REG(BLC_PWM_CTL) = (REG(BLC_PWM_CTL) & 0x9FFFFFFF) | blc_pwm_pipe;
			REG(BLC_PWM_CTL) |= (1 << 31);
			REG(SBLC_PWM_CTL1) |= (1 << 31);

			lil_usleep(1000000 / enc->edp.pwm_inv_freq + 100 * enc->edp.pwm_on_to_backlight_enable);

			REG(PP_CONTROL) |= 4;
		}
	}
}
