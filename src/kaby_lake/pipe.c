#include <lil/intel.h>

#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

void kbl_pipe_src_size_set(LilGpu *gpu, LilCrtc *crtc) {
	REG(PIPE_SRCSZ(crtc->pipe_id)) = (crtc->current_mode.vactive - 1) | ((crtc->current_mode.hactive - 1) << 16);
}

void kbl_pipe_dithering_enable(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	uint32_t dithering_bpc = 0;

	switch(bpp) {
		case 18:
			dithering_bpc = 2;
			break;
		case 24:
			dithering_bpc = 0;
			break;
		case 30:
			dithering_bpc = 1;
			break;
		case 36:
			dithering_bpc = 3;
			break;
		default:
			lil_panic("unsupported bpp");
	}

	REG(PIPE_MISC(crtc->pipe_id)) = (0x20 * dithering_bpc) | (REG(PIPE_MISC(crtc->pipe_id)) & 0xFFFFFF1F);
	REG(PIPE_MISC(crtc->pipe_id)) &= 0xFFFFFFF3;

	if(bpp != 24)
		REG(PIPE_MISC(crtc->pipe_id)) |= 0x10;
}
