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
}
