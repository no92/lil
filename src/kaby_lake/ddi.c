#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/ddi-translations.h"
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

static uint8_t *ddi_translation_table(LilGpu *gpu) {
	if(gpu->gen == GEN_SKL || gpu->gen == GEN_KBL) {
		if(gpu->variant == ULT) {
			return skl_u_translations_edp;
		} else if(gpu->variant == ULX) {
			return skl_y_translations_edp;
		} else {
			return skl_translations_edp;
		}
	} else if(gpu->gen == GEN_KBL) {
		if(gpu->variant == ULT) {
			return kbl_u_translations_edp;
		} else if(gpu->variant == ULX) {
			return kbl_y_translations_edp;
		} else {
			return kbl_translations_edp;
		}
	} else {
		lil_panic("kbl_ddi_buffer_setup_translations unsupported for this GPU gen");
	}
}

void kbl_ddi_buffer_setup_translations(LilGpu *gpu, LilEncoder *enc, uint32_t reg) {
	uint8_t *table = ddi_translation_table(gpu);

	if(!table)
		lil_panic("no DDI translations table found");

	uint32_t *translation = (uint32_t *) &(table[80 * enc->edp.edp_vswing_preemph]);

	for(size_t i = 0; i < 10; i++) {
		REG(reg + (8 * i) + 0) = translation[(i * 2) + 0];
		if(enc->edp.edp_iboost) {
			REG(reg + (8 * i) + 0) |= 0x80000000;
		}
		REG(reg + (8 * i) + 4) = translation[(i * 2) + 1];
	}
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
