#pragma once

#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

bool lil_kbl_dp_is_connected (struct LilGpu* gpu, struct LilConnector* connector);
LilConnectorInfo lil_kbl_dp_get_connector_info(struct LilGpu* gpu, struct LilConnector* connector);

#ifdef __cplusplus
}
#endif
