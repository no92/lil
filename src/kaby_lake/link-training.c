#include "../coffee_lake/dp.h"
#include "../regs.h"

#include "kbl.h"
#include "lil/imports.h"
#include "lil/intel.h"

static bool kbl_dp_training_pattern_1_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out);
static bool kbl_dp_training_pattern_2_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out);
static bool kbl_dp_training_pattern_3_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out);

bool kbl_edp_link_training(LilGpu *gpu, LilCrtc *crtc, uint32_t max_link_rate, uint8_t lane_count) {
	LilConnector *con = crtc->connector;

	uint8_t dpcd_rev = dp_aux_native_read(gpu, con, DPCD_REV);
	uint8_t edp_rev = 0;
	uint8_t aux_training_interval = 0;

	if(crtc->connector->type == EDP) {
		edp_rev = dp_aux_native_read(gpu, con, EDP_DPCD_REV);
	}

	if(dpcd_rev >= 0x12) {
		aux_training_interval = dp_aux_native_read(gpu, con, TRAINING_AUX_RD_INTERVAL);
		aux_training_interval &= ~0x80;
	}

	if(edp_rev < 3) {
		dp_aux_native_write(gpu, con, LINK_BW_SET, max_link_rate);
	} else {
		dp_aux_native_write(gpu, con, LINK_RATE_SET, max_link_rate);
	}

	uint32_t lanes = lane_count & 0x1F;
	uint32_t lane_count_ddi = ((REG(DDI_BUF_CTL(con->ddi_id)) >> 1) & 7) + 1;
	uint8_t lane_count_set = lane_count_ddi;
	if(lane_count_ddi <= lanes)
		lane_count_set = (lane_count & 0x80) | (lane_count_ddi & 0xFF);

	dp_aux_native_write(gpu, con, LANE_COUNT_SET, lane_count_set);

	uint16_t adjust_req = 0;

	if(!kbl_dp_training_pattern_1_set(gpu, crtc, lane_count & 0x1F, aux_training_interval, &adjust_req)) {
		return false;
	}

	bool tps3_supported = !!(lane_count & 0x40);
	bool done = false;

	while(1) {
		// if pci_revision < 2 && gen == SKL then tps3_supported = 0

		bool success = (tps3_supported)
			? kbl_dp_training_pattern_3_set(gpu, crtc, lane_count & 0x1F, aux_training_interval, &adjust_req)
			: kbl_dp_training_pattern_2_set(gpu, crtc, lane_count & 0x1F, aux_training_interval, &adjust_req);

		if(!success)
			return false;

		uint8_t training_pattern_set = dp_aux_native_read(gpu, con, TRAINING_PATTERN_SET);
		training_pattern_set &= 0xDC;
		dp_aux_native_write(gpu, con, TRAINING_PATTERN_SET, training_pattern_set);

		REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFFF8FF) | 0x200;
		lil_usleep(1000);

		if(crtc->connector->type != DISPLAYPORT || dpcd_rev != 0x12 || max_link_rate != 20 || done) {
			return true; // TODO: unsure?
		}

		done = true;
		lil_usleep(220000);

		uint8_t lane_align_status = dp_aux_native_read(gpu, con, LANE_ALIGN_STATUS_UPDATED);

		if((lane_align_status & 1) == 0)
			return false;
		else
		 	return true;

		if(!kbl_dp_training_pattern_1_set(gpu, crtc, lane_count & 0x1F, aux_training_interval, &adjust_req))
			return false;
	}

	return false;
}

uint8_t vswing_lookup[4] = { 3, 2, 0, 0 };
uint8_t preemph_lookup[4] = { 3, 3, 0, 0 };

uint32_t vswing_emp_sel_table[32] = {
	0x0, 0x1000000, 0x2000000, 0x3000000,
	0x4000000, 0x5000000, 0x6000000, 0xFF,
	0x7000000, 0x8000000, 0xFF, 0xFF,
	0x9000000, 0xFF, 0xFF, 0xFF,

	0x0, 0x1000000, 0x2000000, 0x3000000,
	0x4000000, 0x5000000, 0x6000000, 0xFF,
	0x7000000, 0x8000000, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

static void dp_vswing_emp_select(LilGpu *gpu, LilCrtc *crtc, uint8_t vswing, uint8_t preemph) {
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	uint8_t vswing_preemph = 1;

	if(crtc->connector->type == EDP) {
		vswing_preemph = enc->edp.edp_vswing_preemph;
	}

	uint32_t vswing_emp_sel = vswing_emp_sel_table[16 * vswing_preemph + 4 * vswing + preemph];

	// TODO: detect onboard redriver
	if(crtc->connector->type == DISPLAYPORT && false) {
		vswing_emp_sel = vswing_emp_sel_table[4 * vswing_preemph
			+ 4 * ((enc->dp.onboard_redriver_emph_vswing >> 3) & 7)
			+ (enc->dp.onboard_redriver_emph_vswing & 7)];
	}

	if(vswing_emp_sel != 0xFF)
		REG(DDI_BUF_CTL(con->ddi_id)) = (REG(DDI_BUF_CTL(con->ddi_id)) & 0xF0FFFFFF) | vswing_emp_sel;
}

static void dp_training_lanes_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint8_t vswing, uint8_t preemph, bool max_vswing, bool max_preemph) {
	uint8_t data[4];

	data[0] = vswing | (preemph << 3);

	if(max_vswing) {
		data[0] |= (1 << 2);
	}

	if(max_preemph) {
		data[0] |= (1 << 5);
	}

	for(size_t i = 1; i < lanes; i++) {
		data[i] = data[0];
	}

	dp_aux_native_writen(gpu, crtc->connector, TRAINING_LANE0_SET, lanes, data);
}

void dp_parse_adjust_request(uint8_t vswing_preemph, uint16_t adjust_req, uint8_t *vswing, uint8_t *preemph, uint32_t lanes) {
	uint8_t vswing0 = (adjust_req >> 0) & 3;
	uint8_t vswing1 = (adjust_req >> 4) & 3;
	uint8_t vswing2 = 0;
	uint8_t vswing3 = 0;
	uint8_t preemph0 = (adjust_req >> 2) & 3;
	uint8_t preemph1 = (adjust_req >> 6) & 3;
	uint8_t preemph2 = 0;
	uint8_t preemph3 = 0;

	uint8_t adjust_req_lookup[8] = { 0x03, 0x02, 0x01, 0x00, 0x02, 0x02, 0x01, 0x00 };

	if(lanes == 4) {
		vswing2 = (adjust_req >> 8) & 3;
		preemph2 = (adjust_req >> 10) & 3;
		vswing3 = (adjust_req >> 12) & 3;
		preemph3 = (adjust_req >> 14) & 3;
	}

	*vswing = vswing0;
	if(vswing1 > vswing0)
		*vswing = vswing1;

	if(vswing2 > *vswing)
		*vswing = vswing2;

	if(vswing3 > *vswing)
		*vswing = vswing3;

	*preemph = preemph0;

	if(preemph1 > preemph0)
		*preemph = preemph1;

	if(preemph2 > *preemph)
		*preemph = preemph2;

	if(preemph3 > *preemph)
		*preemph = preemph3;

	uint8_t vswing_val = adjust_req_lookup[4 * vswing_preemph + *preemph];

	if(*vswing <= vswing_val)
		vswing_val = *vswing;

	*vswing = vswing_val;
}

static bool kbl_dp_training_pattern_1_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out) {
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	uint8_t vswing_preemph = 1;

	if(crtc->connector->type == EDP) {
		vswing_preemph = enc->edp.edp_vswing_preemph;
	}

	size_t timeout = 0;

	switch(aux_training_interval) {
		case 0:
			timeout = 100;
			break;
		case 1:
			timeout = 4000;
			break;
		case 2:
			timeout = 8000;
			break;
		case 3:
			timeout = 12000;
			break;
		case 4:
			timeout = 16000;
			break;
		default:
			lil_panic("invalid aux_training_interval");
	}

	REG(DP_TP_CTL(con->ddi_id)) &= 0xFFFFF8FF;

	uint8_t training_pattern_set = dp_aux_native_read(gpu, con, TRAINING_PATTERN_SET);
	training_pattern_set = (training_pattern_set & 0xDC) | 0x21;
	dp_aux_native_write(gpu, con, TRAINING_PATTERN_SET, training_pattern_set);

	uint8_t vswing = 0;
	uint8_t preemph = 0;
	uint16_t adjust_req = 0;

	if(crtc->connector->type == EDP && enc->edp.edp_full_link_params_provided)
		lil_panic("full link params unimpemented");

	dp_vswing_emp_select(gpu, crtc, vswing, preemph);
	dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, 0, 0);

	size_t retries = 0;
	bool max_vswing = false;
	bool max_preemph = false;

	while(1) {
		uint16_t prev_adjust_req = adjust_req;
		lil_usleep(timeout);

		uint16_t lane_status = 0;
		dp_aux_native_readn(gpu, con, DP_LANE0_1_STATUS, 2, &lane_status);

		if( (lanes != 1 || (lane_status & 0x0001) != 0)
		 && (lanes != 2 || (lane_status & 0x0011) == 0x0011)
		 && (lanes != 4 || (lane_status & 0x1111) == 0x1111)) {
			break;
		}

		dp_aux_native_readn(gpu, con, ADJUST_REQUEST_LANE0_1, 2, &adjust_req);

		if(adjust_req == prev_adjust_req) {
			if(++retries > 4) {
				return false;
			}
		} else {
			retries = 0;
		}

		if(max_vswing)
			return false;

		dp_parse_adjust_request(vswing_preemph, adjust_req, &vswing, &preemph, lanes);

		if(vswing == vswing_lookup[vswing_preemph])
			max_vswing = true;

		if(preemph == preemph_lookup[vswing_preemph])
			max_preemph = true;

		dp_vswing_emp_select(gpu, crtc, vswing, preemph);
		dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, max_vswing, max_preemph);
	}

	*adjust_req_out = adjust_req;

	return true;
}


static bool kbl_dp_training_pattern_2_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out) {
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	uint8_t vswing_preemph = 1;

	if(crtc->connector->type == EDP) {
		vswing_preemph = enc->edp.edp_vswing_preemph;
	}

	size_t timeout = 0;

	switch(aux_training_interval) {
		case 0:
			timeout = 100;
			break;
		case 1:
			timeout = 4000;
			break;
		case 2:
			timeout = 8000;
			break;
		case 3:
			timeout = 12000;
			break;
		case 4:
			timeout = 16000;
			break;
	}

	REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFFF8FF) | 0x100;

	uint8_t training_pattern_set = dp_aux_native_read(gpu, con, TRAINING_PATTERN_SET);
	training_pattern_set = (training_pattern_set & 0xDC) | 0x22;
	dp_aux_native_write(gpu, con, TRAINING_PATTERN_SET, training_pattern_set);

	uint8_t vswing = 0;
	uint8_t preemph = 0;

	dp_parse_adjust_request(vswing_preemph, *adjust_req_out, &vswing, &preemph, lanes);

	bool max_vswing = (vswing == vswing_lookup[vswing_preemph]);
	bool max_preemph = (preemph == preemph_lookup[vswing_preemph]);

	dp_vswing_emp_select(gpu, crtc, vswing, preemph);
	dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, max_vswing, max_preemph);

	size_t tries = 0;

	while(1) {
		lil_usleep(timeout);

		uint16_t lane_status = 0;
		dp_aux_native_readn(gpu, con, DP_LANE0_1_STATUS, 2, &lane_status);

		uint8_t align_status_updated = dp_aux_native_read(gpu, con, LANE_ALIGN_STATUS_UPDATED);

		if( (lanes == 1 && (lane_status & 0x0001) == 0x0000)
		 && (lanes == 2 && (lane_status & 0x0011) != 0x0011)
		 && (lanes == 4 && (lane_status & 0x1111) != 0x1111)) {
			return false;
		}

		if( (lanes != 1 || (lane_status & 0x0007) == 0x0007)
		 && (lanes != 2 || (lane_status & 0x0077) == 0x0077)
		 && (lanes != 4 || (lane_status & 0x7777) == 0x7777)
		 && (align_status_updated & 1)) {
			break;
		}

		tries++;

		uint16_t adjust_req = 0;
		dp_aux_native_readn(gpu, con, ADJUST_REQUEST_LANE0_1, 2, &adjust_req);

		if(tries > 5)
			return false;

		dp_parse_adjust_request(vswing_preemph, adjust_req, &vswing, &preemph, lanes);

		if(vswing == vswing_lookup[vswing_preemph])
			max_vswing = true;

		if(preemph == preemph_lookup[vswing_preemph])
			max_preemph = true;

		dp_vswing_emp_select(gpu, crtc, vswing, preemph);
		dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, max_vswing, max_preemph);
	}

	return true;
}

static bool kbl_dp_training_pattern_3_set(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t aux_training_interval, uint16_t *adjust_req_out) {
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	uint8_t vswing_preemph = 1;

	if(crtc->connector->type == EDP) {
		vswing_preemph = enc->edp.edp_vswing_preemph;
	}

	size_t timeout = 0;

	switch(aux_training_interval) {
		case 0:
			timeout = 100;
			break;
		case 1:
			timeout = 4000;
			break;
		case 2:
			timeout = 8000;
			break;
		case 3:
			timeout = 12000;
			break;
		case 4:
			timeout = 16000;
			break;
	}

	REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & 0xFFFFF8FF) | 0x400;

	uint8_t training_pattern_set = dp_aux_native_read(gpu, con, TRAINING_PATTERN_SET);
	training_pattern_set = (training_pattern_set & 0xDC) | 0x23;
	dp_aux_native_write(gpu, con, TRAINING_PATTERN_SET, training_pattern_set);

	uint8_t vswing = 0;
	uint8_t preemph = 0;

	dp_parse_adjust_request(vswing_preemph, *adjust_req_out, &vswing, &preemph, lanes);

	bool max_vswing = (vswing == vswing_lookup[vswing_preemph]);
	bool max_preemph = (preemph == preemph_lookup[vswing_preemph]);

	dp_vswing_emp_select(gpu, crtc, vswing, preemph);
	dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, max_vswing, max_preemph);

	size_t tries = 0;

	while(1) {
		lil_usleep(timeout);

		uint16_t lane_status = 0;
		dp_aux_native_readn(gpu, con, DP_LANE0_1_STATUS, 2, &lane_status);

		uint8_t align_status_updated = dp_aux_native_read(gpu, con, LANE_ALIGN_STATUS_UPDATED);

		if( (lanes == 1 && (lane_status & 0x0001) == 0x0000)
		 && (lanes == 2 && (lane_status & 0x0011) != 0x0011)
		 && (lanes == 4 && (lane_status & 0x1111) != 0x1111)) {
			return false;
		}

		if( (lanes != 1 || (lane_status & 0x0007) == 0x0007)
		 && (lanes != 2 || (lane_status & 0x0077) == 0x0077)
		 && (lanes != 4 || (lane_status & 0x7777) == 0x7777)
		 && (align_status_updated & 1)) {
			break;
		}

		tries++;

		uint16_t adjust_req = 0;
		dp_aux_native_readn(gpu, con, ADJUST_REQUEST_LANE0_1, 2, &adjust_req);

		if(tries > 5)
			return false;

		dp_parse_adjust_request(vswing_preemph, adjust_req, &vswing, &preemph, lanes);

		if(vswing == vswing_lookup[vswing_preemph])
			max_vswing = true;

		if(preemph == preemph_lookup[vswing_preemph])
			max_preemph = true;

		dp_vswing_emp_select(gpu, crtc, vswing, preemph);
		dp_training_lanes_set(gpu, crtc, lanes, vswing, preemph, max_vswing, max_preemph);
	}

	return true;
}
