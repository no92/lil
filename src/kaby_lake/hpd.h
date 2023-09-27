#pragma once

#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

void kbl_hpd_enable(LilGpu* gpu, LilCrtc* crtc);
void kbl_program_pulse_count(LilGpu* gpu, LilConnector* connector);

bool kbl_detect_ddi_hotplug_state(LilGpu* gpu, LilConnector* connector);

#ifdef __cplusplus
}
#endif
