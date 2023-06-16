#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

bool kbl_ddi_buf_enabled(LilGpu *gpu, LilCrtc *crtc) {
	return REG(DDI_BUF_CTL(crtc->connector->ddi_id) & DDI_BUF_CTL_ENABLED);
}

bool kbl_ddi_hotplug_detected(LilGpu *gpu, enum LilDdiId ddi_id) {
	uint32_t mask = 0;

	if(ddi_id == DDI_A) {
		mask = 0x1000000;
	} else if(ddi_id == DDI_D) {
		mask = 0x800000;
	} else {
		lil_panic("unimplemented DDI");
	}

	return (REG(SDEISR) & mask);
}

void kbl_ddi_power_enable(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t val = 0;
	uint32_t wait_mask = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A:
		case DDI_E: {
			val = 8;
			wait_mask = 4;
			break;
		}
		case DDI_B: {
			val = 0x20;
			wait_mask = 0x10;
			break;
		}
		case DDI_C: {
			val = 0x80;
			wait_mask = 0x40;
			break;
		}
		case DDI_D: {
			val = 0x200;
			wait_mask = 0x100;
			break;
		}
	}

	REG(HSW_PWR_WELL_CTL1) |= val;

	if(!wait_for_bit_set(REG_PTR(HSW_PWR_WELL_CTL1), wait_mask, 20, 1))
		lil_panic("timeout on DDI powerup?");
}

void kbl_ddi_balance_leg_set(LilGpu *gpu, enum LilDdiId ddi_id, uint8_t balance_leg) {
	uint32_t unset = 0;
	uint32_t set = 0;

	switch(ddi_id) {
		case DDI_A:
			unset = 0x700;
			set = balance_leg << 8;
			break;
		case DDI_B:
			unset = 0x3800;
			set = balance_leg << 11;
			break;
		case DDI_C:
			unset = 0x1C000;
			set = balance_leg << 14;
			break;
		case DDI_D:
			unset = 0xE0000;
			set = balance_leg << 17;
			break;
		case DDI_E:
			unset = 0x700000;
			set = balance_leg << 20;
			break;
		default:
			lil_panic("invalid ddi_id");
	}

	REG(DISPIO_CR_TX_BMU_CR0) = set | (REG(DISPIO_CR_TX_BMU_CR0) & ~unset);
}
