#include "lil/vbt-types.h"
#include <lil/intel.h>
#include <lil/imports.h>
#include <lil/vbt.h>

const struct vbt_header *vbt_locate(void) {
	size_t size = 0;
	uint16_t file_id = 0;

	if(!fw_cfg_find("etc/igd-opregion", &size, &file_id))
		lil_panic("TODO: vbt_locate on real hardware is unsupported (for now)");

	void *file_buf = lil_malloc(size);
	fw_cfg_readfile(file_id, size, file_buf);

	return vbt_get_header(file_buf, size);
}

void vbt_init(LilGpu *gpu) {

}
