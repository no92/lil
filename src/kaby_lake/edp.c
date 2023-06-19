#include <lil/imports.h>
#include <lil/intel.h>

#include "src/coffee_lake/dp.h"
#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

bool kbl_edp_aux_readable(LilGpu *gpu, LilConnector *con) {
	LilEncoder *enc = con->encoder;

	if(REG(PP_CONTROL) & 8) {
		lil_usleep(100);

		if(kbl_ddi_hotplug_detected(gpu, con->ddi_id)) {
			return dp_aux_native_read(gpu, DPCD_REV) != 0;
		}
	} else {
		REG(PP_CONTROL) |= 8;
		lil_sleep(10);

		if(enc->edp.t3_optimization) {
			size_t tries = enc->edp.t1_t3 / 100;

			while(tries --> 0) {
				if(kbl_ddi_hotplug_detected(gpu, con->ddi_id)) {
					REG(SHOTPLUG_CTL) = REG(SHOTPLUG_CTL);

					if(dp_aux_native_read(gpu, DPCD_REV) != 0) {
						return true;
					}
				}

				lil_sleep(10);
			}

			REG(PP_CONTROL) &= ~8;
			return false;
		} else {
			lil_usleep(100 * enc->edp.t1_t3);
			return true;
		}
	}

	return false;
}

