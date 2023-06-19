#include <lil/intel.h>
#include <lil/imports.h>
#include <lil/vbt.h>

#include "src/pci.h"
#include "src/vbt/opregion.h"

const struct vbt_header *vbt_locate(LilGpu *gpu) {
	uint32_t asls_phys = lil_pci_readd(gpu->dev, PCI_ASLS);
	if(!asls_phys) {
		lil_panic("ACPI OpRegion not supported");
	}

	struct opregion_header *opregion = lil_map(asls_phys, 0x2000);
	if(memcmp(opregion, OPREGION_SIGNATURE, 16)) {
		lil_panic("ACPI OpRegion signature mismatch");
	}

	lil_log(VERBOSE, "ACPI OpRegion %u.%u.%u\n", opregion->over.major, opregion->over.minor, opregion->over.revision);

	struct opregion_asle *asle = NULL;

	if(opregion->mbox & MBOX_ASLE) {
		asle = (void *) ((uintptr_t) opregion + OPREGION_ASLE_OFFSET);
	}

	if(opregion->over.major >= 2 && asle && asle->rvda && asle->rvds) {
		uint64_t rvda = asle->rvda;

		/* OpRegion v2.1+: rvda is an unsigned relative offset from the OpRegion base address */
		if(opregion->over.major > 2 || opregion->over.minor >= 1) {
			if(rvda < OPREGION_SIZE) {
				lil_log(WARNING, "VBT base shouldn't be within OpRegion, but it is!");
			}

			rvda += (uintptr_t) opregion;
		}

		/* OpRegion 2.0: rvda is a physical address */
		void *vbt = lil_map(rvda, asle->rvds);

		const struct vbt_header *vbt_header = vbt_get_header(vbt, asle->rvds);
		if(!vbt_header) {
			lil_log(WARNING, "VBT header from ACPI OpRegion ASLE is invalid!");
			lil_unmap(vbt, asle->rvds);
		} else {
			return vbt_header;
		}
	}

	if(!(opregion->mbox & MBOX_VBT)) {
		lil_log(ERROR, "ACPI OpRegion does not support VBT mailbox when it should!");
		lil_panic("Invalid ACPI OpRegion");
	}

	size_t vbt_size = ((opregion->mbox & MBOX_ASLE_EXT) ? OPREGION_ASLE_EXT_OFFSET : OPREGION_SIZE) - OPREGION_VBT_OFFSET;
	void *vbt = lil_map(asls_phys + OPREGION_VBT_OFFSET, vbt_size);

	const struct vbt_header *vbt_header = vbt_get_header(vbt, vbt_size);

	if(!vbt_header) {
		lil_log(ERROR, "Reading VBT from ACPI OpRegion mailbox 4 failed!");
		lil_panic("No supported method to obtain VBT left");
	}

	return vbt_header;
}

void vbt_init(LilGpu *gpu) {
	gpu->vbt_header = vbt_locate(gpu);

	const struct bdb_header *bdb_hdr = vbt_get_bdb_header(gpu->vbt_header);
	if(!bdb_hdr)
		lil_panic("BDB header not found");

	const struct bdb_driver_features *driver_features_block = (const struct bdb_driver_features *) vbt_get_bdb_block(gpu->vbt_header, BDB_DRIVER_FEATURES);
	if(!driver_features_block)
		lil_panic("BDB driver features not found");

	const struct bdb_general_definitions *general_defs = (const struct bdb_general_definitions *) vbt_get_bdb_block(gpu->vbt_header, BDB_GENERAL_DEFINITIONS);
	if(!general_defs)
		lil_panic("BDB general definitions not found");

	uint8_t dvo = general_defs->child_dev[0].dvo_port;

	if(driver_features_block->lvds_config == LVDS_CONFIG_EDP) {
		gpu->ddi_id = vbt_dvo_to_ddi(dvo);
	}

	const struct bdb_edp *edp = (const struct bdb_edp *) vbt_get_bdb_block(gpu->vbt_header, BDB_EDP);
	if(!edp)
		lil_panic("BDB eDP not found");

	if(gpu->ddi_id) {
		gpu->edp_max_lanes = 4;
	} else {
		uint8_t max_lanes = 4;

		for(size_t child = 0; child < 8; child++) {
			if(general_defs->child_dev[child].device_type) {
				if(general_defs->child_dev[child].dvo_port == 12 || general_defs->child_dev[child].dvo_port == 11) {
					max_lanes = 2;
					break;
				}
			}
		}

		gpu->edp_max_lanes = max_lanes;
	}

	gpu->edp_vswing_preemph = (edp->edp_vswing_preemph >> (4 * panel_type)) & 0xF;
	gpu->t8 = edp->power_seqs[panel_type].t8;
	gpu->vbt_edp_fast_link_training = (edp->fast_link_training >> panel_type) & 1;
	gpu->edp_fast_link_lanes = edp->fast_link_params[panel_type].lanes;
	gpu->edp_iboost = general_defs->child_dev[0].iboost;
	gpu->edp_port_reversal = general_defs->child_dev[0].lane_reversal;
	gpu->pwm_on_to_backlight_enable = edp->pwm_delays[panel_type].pwm_on_to_backlight_enable;

	uint32_t color_depth = (edp->color_depth >> (2 * panel_type)) & 3;

	switch(color_depth) {
		case 0:
			gpu->edp_color_depth = 18;
			break;
		case 1:
			gpu->edp_color_depth = 24;
			break;
		case 2:
			gpu->edp_color_depth = 30;
			break;
		case 3:
			gpu->edp_color_depth = 36;
			break;
	}

	switch(general_defs->child_dev[0].dp_iboost_level) {
		case 0:
			gpu->edp_balance_leg_val = 1;
			break;
		case 1:
			gpu->edp_balance_leg_val = 3;
			break;
		case 2:
			gpu->edp_balance_leg_val = 7;
			break;
	}

	const struct bdb_lfp_blc *lfp_blc = (const struct bdb_lfp_blc *) vbt_get_bdb_block(gpu->vbt_header, BDB_LVDS_BLC);

	gpu->backlight_control_method_type = lfp_blc->backlight_control[panel_type].type & 0xF;
	gpu->pwm_inv_freq = lfp_blc->panel[panel_type].pwm_freq_hz;

	const struct bdb_general_features *general_features = (const struct bdb_general_features *) vbt_get_bdb_block(gpu->vbt_header, BDB_GENERAL_FEATURES);
	if(!general_features)
		lil_panic("BDB general features not found");

	gpu->dynamic_cdclk_supported = general_features->display_clock_mode;
}
