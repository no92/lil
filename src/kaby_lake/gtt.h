#pragma once

#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

void lil_kbl_vmem_clear(LilGpu* gpu);
void lil_kbl_vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr);

#ifdef __cplusplus
}
#endif
