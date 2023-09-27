#include <lil/imports.h>
#include <lil/intel.h>
#include <stdint.h>

#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

// TODO(CLEAN;BIT): this function needs to be cleaned up
// 					specifically, we should be using enums or defines for this bit setting/clearing
void kbl_pll_disable(LilGpu *gpu, LilConnector *con) {
	uint8_t shift = 0;
	uint32_t mask = 0;

	switch(con->crtc->pll_id) {
		case LCPLL1: {
			shift = 0x01;
			mask = 0x0E;
			break;
		}
		case LCPLL2: {
			shift = 7;
			mask = 0x380;
			REG(LCPLL2_CTL) &= ~0x80000000;
			break;
		}
		case WRPLL1: {
			shift = 13;
			mask = 0xE000;
			lil_panic("WRPLL1 disable unimplemented");
		}
		case WRPLL2: {
			shift = 19;
			mask = 0x380000;
			lil_panic("WRPLL2 disable unimplemented");
		}
	}

	uint32_t link_rate = (REG(DPLL_CTRL1) & mask) >> shift;

	if(con->type == EDP && (link_rate == 4 || link_rate == 5)) {
		lil_panic("eDP PLL disable logic unimplemented");
	}
}
