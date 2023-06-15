#pragma once

#include <lil/intel.h>

void lil_init_kbl_gpu(LilGpu *ret);

void lil_kbl_setup(LilGpu *gpu);

uint32_t kbl_cdclk_dec_to_int(uint32_t cdclk_freq_decimal);

void lil_kbl_pci_detect(LilGpu *gpu);

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait);
void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc);
