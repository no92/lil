#pragma once

#include <lil/intel.h>

void lil_init_kbl_gpu(LilGpu *ret);

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait);
void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc);
