#pragma once

#include <lil/intel.h>

#include "src/gtt.h"

void lil_kbl_vmem_clear(LilGpu* gpu);
void lil_kbl_vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr);
