#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/kbl.h"

static uint16_t SKYLAKE_IDS[12] = { 0x1902, 0x1912, 0x1932, 0x193A, 0x190B, 0x191B, 0x192B, 0x193B, 0x191D, 0x192D, 0x193D, 0 };
static uint16_t SKYLAKE_ULT_IDS[8] = { 0x1906, 0x1916, 0x1926, 0x1927, 0x1913, 0x1923, 0x1921, 0 };
static uint16_t SKYLAKE_ULX_IDS[3] = { 0x190E, 0x191E, 0 };

static uint16_t KABY_LAKE_IDS[11] = { 0x5902, 0x5912, 0x5917, 0x5908, 0x590A, 0x591A, 0x590B, 0x591B, 0x593B, 0x591D, 0 };
static uint16_t KABY_LAKE_ULT_IDS[8] = { 0x5906, 0x5916, 0x5926, 0x5913, 0x5923, 0x5921, 0x5927, 0 };
static uint16_t KABY_LAKE_ULX_IDS[4] = { 0x5915, 0x590E, 0x591E, 0 };

static struct variant_desc {
	uint16_t *id_list;
	enum LilGpuGen gen;
	enum LilGpuVariant variant;
} variants[] = {
	{ SKYLAKE_IDS, GEN_SKL, H },
	{ SKYLAKE_ULT_IDS, GEN_SKL, ULT },
	{ SKYLAKE_ULX_IDS, GEN_SKL, ULX },
	{ KABY_LAKE_IDS, GEN_KBL, H },
	{ KABY_LAKE_ULT_IDS, GEN_KBL, ULT },
	{ KABY_LAKE_ULX_IDS, GEN_KBL, ULX },
};

void lil_kbl_pci_detect(LilGpu *gpu) {
	uint16_t dev_id = lil_pci_readw(gpu->dev, 2);

	for(size_t i = 0; i < (sizeof(variants)/sizeof(*variants)); i++) {
		struct variant_desc *desc = &variants[i];

		for(size_t j = 0; desc->id_list[j]; j++) {
			if(desc->id_list[j] == dev_id) {
				gpu->gen = desc->gen;
				gpu->variant = desc->variant;
			}
		}
	}
}
