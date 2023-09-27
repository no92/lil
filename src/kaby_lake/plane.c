#include <lil/imports.h>

#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

static void wait_for_vblank(LilGpu *gpu, LilPlane *plane) {
	REG(IIR(plane->pipe_id)) |= 1;

	if(!wait_for_bit_set(REG_PTR(IIR(plane->pipe_id)), 1, 500000, 1000))
		lil_log(WARNING, "timeout on wait_for_vblank\n");
}

bool lil_kbl_update_primary_surface(struct LilGpu* gpu, struct LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride) {
	if(surface_address & 0xFFF)
		return false;

    REG(PRI_SURFACE(plane->pipe_id)) = surface_address;

	wait_for_vblank(gpu, plane);
    return true;
}

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc) {
	REG(PRI_SURFACE(crtc->pipe_id)) = REG(PRI_SURFACE(crtc->pipe_id));
}

// TODO(CLEAN;BIT): this function needs to be cleaned up
// 					specifically, we should be using enums or defines for this bit setting/clearing
void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait) {
	uint32_t htotal = crtc->current_mode.htotal;
	uint32_t pixel_clock = crtc->current_mode.clock;
	uint32_t pitch = (crtc->current_mode.hactive * 4 + 63) & 0xFFFFFFC0;
	uint32_t mem_latency = (gpu->mem_latency_first_set) ? gpu->mem_latency_first_set : 2;

	uint32_t method1 = mem_latency * pixel_clock * 4 / 1000;
	uint32_t method2 = pitch * ((mem_latency * pixel_clock + 1000 * htotal - 1) / (1000 * htotal));

	uint32_t wm = ((method2 > method1) ? method1 : method2);

	uint32_t lines = (wm + pitch - 1) / pitch;
	uint32_t blocks = ((wm + 511) >> 9) + 1;

	// REG(PLANE_WM_1(crtc->pipe_id)) = blocks | ((lines | 0xFFFE0000) << 14);
	// TODO(CLEAN): what is this?
	REG(PLANE_WM_1(crtc->pipe_id)) = 0x800080A0;

	REG(PRI_CTL(crtc->pipe_id)) |= (1 << 31) | (1 << 13) | (1 << 26) | (1 << 20);

	kbl_plane_page_flip(gpu, crtc);

	if(vblank_wait)
		wait_for_vblank(gpu, &crtc->planes[0]);
}

// TODO(CLEAN;BIT): this function needs to be cleaned up
// 					specifically, we should be using enums or defines for this bit setting/clearing
void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(PRI_CTL(crtc->pipe_id)) &= ~(1 << 31);

	kbl_plane_page_flip(gpu, crtc);
	wait_for_vblank(gpu, &crtc->planes[0]);

	for(size_t i = 0; i < 8; i++) {
		REG(PLANE_WM_1(crtc->pipe_id) + (4 * i)) = 0;
	}

	REG(WM_LINETIME(crtc->pipe_id)) = 0;
}

void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc) {
	REG(PLANE_SIZE(crtc->pipe_id)) = (crtc->current_mode.hactive - 1) | ((crtc->current_mode.vactive - 1) << 16);
}
