#include <lil/imports.h>
#include <lil/intel.h>

#include "src/coffee_lake/plane.h"
#include "src/coffee_lake/dp.h"
#include "src/coffee_lake/crtc.h"

#include "src/kaby_lake/kbl.h"
#include "src/kaby_lake/gtt.h"

void lil_init_kbl_gpu(LilGpu* ret) {
	ret->vmem_clear = lil_kbl_vmem_clear;
	ret->vmem_map = lil_kbl_vmem_map;

	ret->num_connectors = 1;
	ret->connectors = lil_malloc(sizeof(LilConnector) * ret->num_connectors);

	ret->connectors[0].id = 0;
	ret->connectors[0].type = EDP;
	ret->connectors[0].on_pch = true;
	ret->connectors[0].get_connector_info = lil_cfl_dp_get_connector_info;
	ret->connectors[0].is_connected = lil_cfl_dp_is_connected;
	ret->connectors[0].get_state = lil_cfl_dp_get_state;
	ret->connectors[0].set_state = lil_cfl_dp_set_state;

	ret->connectors[0].crtc = lil_malloc(sizeof(LilCrtc));
	ret->connectors[0].crtc->transcoder = TRANSCODER_EDP;
	ret->connectors[0].crtc->connector = &ret->connectors[0];
	ret->connectors[0].crtc->num_planes = 1;
	ret->connectors[0].crtc->planes = lil_malloc(sizeof(LilPlane));
	for (int i = 0; i < ret->connectors[0].crtc->num_planes; i++) {
		ret->connectors[0].crtc->planes[i].enabled = 0;
		ret->connectors[0].crtc->planes[i].pipe_id = 0;
		ret->connectors[0].crtc->planes[i].update_surface = lil_cfl_update_primary_surface;
	}
	ret->connectors[0].crtc->pipe_id = 0;
	ret->connectors[0].crtc->shutdown = lil_cfl_shutdown;

	lil_kbl_pci_detect(ret);
	lil_kbl_setup(ret);
}
