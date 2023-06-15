#include <lil/intel.h>
#include <lil/imports.h>

#include "src/kaby_lake/kbl.h"

struct vco_lookup {
	unsigned int decimal;
	unsigned int integer;
};

static struct vco_lookup vco8100_lookup[] = {
	{ 0x2A1, 0x152 },
	{ 0x382, 0x1C2 },
	{ 0x436, 0x21C },
	{ 0x544, 0x2A3 },
};

static struct vco_lookup vco8640_lookup[] = {
	{ 0x267, 0x135 },
	{ 0x35E, 0x1B0 },
	{ 0x436, 0x21C },
	{ 0x4D0, 0x26A },
};

uint32_t kbl_cdclk_dec_to_int(uint32_t cdclk_freq_decimal) {
	for(size_t i = 0; i < 4; i++) {
		if(vco8100_lookup[i].decimal == cdclk_freq_decimal)
			return vco8100_lookup[i].integer;
		if(vco8640_lookup[i].decimal == cdclk_freq_decimal)
			return vco8640_lookup[i].integer;
	}

	return 0;
}

