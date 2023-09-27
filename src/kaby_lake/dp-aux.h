#pragma once

#include <lil/intel.h>

#include "src/edid.h"

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
