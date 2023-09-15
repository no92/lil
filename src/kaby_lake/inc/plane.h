#pragma once

#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

bool lil_kbl_update_primary_surface(struct LilGpu* gpu, struct LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride);

#ifdef __cplusplus
}
#endif
