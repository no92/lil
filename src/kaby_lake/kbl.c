#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/kbl.h"
#include "src/kaby_lake/gtt.h"

void lil_init_kbl_gpu(LilGpu* ret) {
	ret->vmem_clear = lil_kbl_vmem_clear;
	ret->vmem_map = lil_kbl_vmem_map;

	ret->max_connectors = 4;
	ret->connectors = lil_malloc(sizeof(LilConnector) * ret->max_connectors);

	lil_kbl_pci_detect(ret);

	switch(ret->gen) {
		case GEN_SKL: {
			lil_log(VERBOSE, "\tGPU gen: Skylake\n");
			break;
		}
		case GEN_KBL: {
			lil_log(VERBOSE, "\tGPU gen: Kaby Lake\n");
			break;
		}
		case GEN_CFL: {
			lil_log(VERBOSE, "\tGPU gen: Coffee Lake\n");
			break;
		}
		default: {
			lil_assert(false);
		}
	}

	switch(ret->variant) {
		case H: {
			lil_log(VERBOSE, "\tGPU variant: H\n");
			break;
		}
		case ULT: {
			lil_log(VERBOSE, "\tGPU variant: ULT\n");
			break;
		}
		case ULX: {
			lil_log(VERBOSE, "\tGPU variant: ULX\n");
			break;
		}
	}

	lil_kbl_setup(ret);
}
