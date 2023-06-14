#include <lil/imports.h>

#include "kbl.h"
#include "../regs.h"

static void wait_for_vblank(LilGpu *gpu, LilCrtc *crtc) {
	REG(IIR(crtc->pipe_id)) |= 1;

	for(size_t timeout = 500000; timeout; timeout -= 1000) {
		lil_usleep(1000);
		if(REG(IIR(crtc->pipe_id)) & 1)
			return;
	}

	lil_panic("timeout on wait_for_vblank");
}

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc) {
	REG(PRI_SURFACE(crtc->plane_id)) = REG(PRI_SURFACE(crtc->plane_id));
}

void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait) {
	REG(PRI_CTL(crtc->plane_id)) |= (1 << 31) | (1 << 26) | (1 << 20);

	kbl_plane_page_flip(gpu, crtc);

	if(vblank_wait)
		wait_for_vblank(gpu, crtc);
}

void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(PRI_CTL(crtc->plane_id)) &= ~(1 << 31);

	kbl_plane_page_flip(gpu, crtc);
	wait_for_vblank(gpu, crtc);
}

void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc) {
	REG(PLANE_SIZE(crtc->pipe_id)) = (crtc->current_mode.hactive - 1) | ((crtc->current_mode.vactive - 1) << 16);
}
