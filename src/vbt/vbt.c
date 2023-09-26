#include <lil/intel.h>
#include <lil/imports.h>
#include <lil/vbt.h>

#include "src/kaby_lake/inc/plane.h"
#include "src/kaby_lake/inc/dp.h"
#include "src/kaby_lake/inc/kbl.h"
#include "src/pci.h"
#include "src/vbt/opregion.h"

//
// TODO: vbt parsing should be generation independent
//

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
		lil_log(VERBOSE, "Raw RVDA in asle->rvda: 0x%lx\n", asle->rvda);

		// In OpRegion v2.1+, rvda was changed to a relative offset
		if(opregion->over.major > 2 || (opregion->over.major == 2 && opregion->over.minor >= 1)) {
			if(rvda < OPREGION_SIZE) {
				lil_log(WARNING, "VBT base shouldn't be within OpRegion, but it is!\n");
			}

			rvda += asls_phys;
		}

		lil_log(INFO, "VBT RVDA: 0x%lx\n", rvda);

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
}

static uint32_t vbt_handle_to_port(uint32_t handle) {
	switch(handle) {
		case 4:
			return 0;
		case 0x10:
			return 3;
		case 0x20:
			return 2;
		case 0x40:
			return 1;
		default: {
			lil_log(ERROR, "unexpected VBT child handle %#x\n", handle);
			lil_panic("unhandled VBT child handle");
		}
	}
}

static enum LilAuxChannel vbt_parse_aux_channel(uint8_t aux_ch) {
	switch(aux_ch) {
		case 0x40: return AUX_CH_A;
		case 0x10: return AUX_CH_B;
		case 0x20: return AUX_CH_C;
		case 0x30: return AUX_CH_D;
		case 0x50: return AUX_CH_E;
		case 0x60: return AUX_CH_F;
		case 0x70: return AUX_CH_G;
		case 0x80: return AUX_CH_H;
		case 0x90: return AUX_CH_I;
		default: {
			lil_log(WARNING, "Unhandled VBT child AUX channel %#x; falling back to AUX channel A\n", aux_ch);
			return AUX_CH_A;
		}
	}
}

void vbt_setup_children(LilGpu *gpu) {
	size_t con_id = 0;

	const struct bdb_driver_features *driver_features = (void *) vbt_get_bdb_block(gpu->vbt_header, BDB_DRIVER_FEATURES);
	const struct bdb_general_definitions *general_defs = (void *) vbt_get_bdb_block(gpu->vbt_header, BDB_GENERAL_DEFINITIONS);

	if(driver_features->lvds_config != LVDS_CONFIG_NONE) {
		struct child_device *dev = (void *) &general_defs->child_dev;
		LilConnector *con = &gpu->connectors[0];

		lil_assert((dev->device_type & (DEVICE_TYPE_DISPLAYPORT_OUTPUT | DEVICE_TYPE_INTERNAL_CONNECTOR)));

		con->id = 0;
		con->type = EDP;
		con->on_pch = true;
		con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
		lil_assert(con->ddi_id == 0 || con->ddi_id == 3);
		con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

		con->get_connector_info = lil_kbl_dp_get_connector_info;
		con->is_connected = lil_kbl_dp_is_connected;
		// con->get_state = lil_kbl_dp_get_state;
		// con->set_state = lil_kbl_dp_set_state;

		con->encoder = lil_malloc(sizeof(LilEncoder));

		con->crtc = lil_malloc(sizeof(LilCrtc));
		con->crtc->transcoder = (con->ddi_id == DDI_A) ? TRANSCODER_EDP : TRANSCODER_A;
		con->crtc->connector = &gpu->connectors[0];
		con->crtc->pipe_id = 0;
		con->crtc->commit_modeset = lil_kbl_commit_modeset;
		con->crtc->shutdown = lil_kbl_crtc_dp_shutdown;

		con->crtc->num_planes = 1;
		con->crtc->planes = lil_malloc(sizeof(LilPlane));
		for(size_t i = 0; i < con->crtc->num_planes; i++) {
			con->crtc->planes[i].enabled = 0;
			con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
			con->crtc->planes[i].update_surface = lil_kbl_update_primary_surface;
		}

		kbl_encoder_edp_init(gpu, con->encoder);

		con_id++;
	}

	size_t children = (general_defs->header.size - sizeof(*general_defs) + sizeof(struct bdb_block_header)) / general_defs->child_dev_size;

	for(size_t child = con_id; child < 8; child++) {
		struct child_device *dev = (void *) ((uintptr_t) &general_defs->child_dev + (child * general_defs->child_dev_size));
		uint32_t device_type = dev->device_type;

		switch(device_type) {
			case 0: {
				continue;
			}
			case DEVICE_TYPE_DP: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				con->type = DISPLAYPORT;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = lil_kbl_dp_get_connector_info;
				con->is_connected = lil_kbl_dp_is_connected;

				con->encoder = lil_malloc(sizeof(LilEncoder));

				con->crtc = lil_malloc(sizeof(LilCrtc));
				con->crtc->connector = con;

				// TODO(CLEAN):	not optimal
				// 				on gemini lake this seems to always be 0
				con->crtc->pipe_id = 0;
				con->crtc->transcoder = con->crtc->pipe_id;
				con->crtc->commit_modeset = lil_kbl_commit_modeset;
				con->crtc->shutdown = lil_kbl_crtc_dp_shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = lil_malloc(sizeof(LilPlane));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = lil_kbl_update_primary_surface;
				}

				kbl_encoder_dp_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}

			case DEVICE_TYPE_HDMI: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				con->type = HDMI;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = lil_kbl_hdmi_get_connector_info;
				con->is_connected = lil_kbl_hdmi_is_connected;

				con->encoder = lil_malloc(sizeof(LilEncoder));

				con->crtc = lil_malloc(sizeof(LilCrtc));
				con->crtc->connector = con;

				con->crtc->pipe_id = 1;
				con->crtc->transcoder = con->crtc->pipe_id;
				con->crtc->commit_modeset = lil_kbl_hdmi_commit_modeset;
				con->crtc->shutdown = lil_kbl_hdmi_shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = lil_malloc(sizeof(LilPlane));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = lil_kbl_update_primary_surface;
				}

				kbl_encoder_hdmi_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}

			case DEVICE_TYPE_DP_DUAL_MODE: {
				LilConnector *con = &gpu->connectors[con_id];

				con->id = vbt_handle_to_port(dev->handle);
				// con->type = DISPLAYPORT;
				con->type = HDMI;
				con->ddi_id = vbt_dvo_to_ddi(dev->dvo_port);
				con->aux_ch = vbt_parse_aux_channel(dev->aux_channel);

				con->get_connector_info = lil_kbl_hdmi_get_connector_info;
				con->is_connected = lil_kbl_hdmi_is_connected;

				con->encoder = lil_malloc(sizeof(LilEncoder));

				con->crtc = lil_malloc(sizeof(LilCrtc));
				con->crtc->connector = con;

				con->crtc->pipe_id = 1;
				con->crtc->transcoder = con->crtc->pipe_id;
				con->crtc->commit_modeset = lil_kbl_hdmi_commit_modeset;
				con->crtc->shutdown = lil_kbl_hdmi_shutdown;

				con->crtc->num_planes = 1;
				con->crtc->planes = lil_malloc(sizeof(LilPlane));
				for(size_t i = 0; i < con->crtc->num_planes; i++) {
					con->crtc->planes[i].enabled = true;
					con->crtc->planes[i].pipe_id = con->crtc->pipe_id;
					con->crtc->planes[i].update_surface = lil_kbl_update_primary_surface;
				}

				// kbl_encoder_dp_init(gpu, con->encoder, con_id);
				kbl_encoder_hdmi_init(gpu, con->encoder, dev);

				con_id++;
				break;
			}
			default: {
				lil_log(WARNING, "VBT: found unknown device type %#x\n", device_type);
				break;
			}
		}
	}

	gpu->num_connectors = con_id;
}
