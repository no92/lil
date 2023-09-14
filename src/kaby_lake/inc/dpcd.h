#pragma once

#include <lil/intel.h>

#include "src/edid.h"

enum DPCD_ADDRESSES {
	DPCD_REV = 0x0,
	MAX_LINK_RATE = 0x1,
	MAX_LANE_COUNT = 0x2,
	MAX_DOWNSPREAD = 0x3,
	DOWNSTREAMPORT_PRESENT = 0x5,
	EDP_CONFIGURATION_CAP = 0xD,
	TRAINING_AUX_RD_INTERVAL = 0xE,
	DOWNSTREAM_PORT0_CAP = 0x80,
	LINK_BW_SET = 0x100,
	LANE_COUNT_SET = 0x101,
	TRAINING_PATTERN_SET = 0x102,
	TRAINING_LANE0_SET = 0x103,
	LINK_RATE_SET = 0x115,
	DP_LANE0_1_STATUS = 0x202,
	DP_LANE2_3_STATUS = 0x203,
	LANE_ALIGN_STATUS_UPDATED = 0x204,
	ADJUST_REQUEST_LANE0_1 = 0x206,
	SET_POWER = 0x600,
	EDP_DPCD_REV = 0x700,
};

#ifdef __cplusplus
extern "C" {
#endif

uint8_t dp_aux_native_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr);
void dp_aux_native_readn(struct LilGpu* gpu, LilConnector *con, uint16_t addr, size_t n, void *buf);
void dp_aux_native_write(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t v);
void dp_aux_native_writen(struct LilGpu* gpu, LilConnector *con, uint16_t addr, size_t n, void *buf);

LilError dp_aux_i2c_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t len, uint8_t* buf);
LilError dp_aux_i2c_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t len, uint8_t* buf);

void dp_aux_read_edid(struct LilGpu* gpu, LilConnector *con, DisplayData* buf);

bool dp_dual_mode_read(LilGpu *gpu, LilConnector *con, uint8_t offset, void *buffer, size_t size);

#ifdef __cplusplus
}
#endif
