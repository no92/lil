#pragma once

#include <lil/intel.h>

void lil_init_kbl_gpu(LilGpu *ret);

void lil_kbl_setup(LilGpu *gpu);

uint32_t kbl_cdclk_dec_to_int(uint32_t cdclk_freq_decimal);

bool kbl_ddi_buf_enabled(LilGpu *gpu, LilCrtc *crtc);
bool kbl_ddi_hotplug_detected(LilGpu *gpu, enum LilDdiId ddi_id);
void kbl_ddi_buffer_setup_translations(LilGpu *gpu, LilEncoder *enc, uint32_t reg);
void kbl_ddi_power_enable(LilGpu *gpu, LilCrtc *crtc);
void kbl_ddi_balance_leg_set(LilGpu *gpu, enum LilDdiId ddi_id, uint8_t balance_leg);

bool kbl_pcode_rw(LilGpu *gpu, uint32_t *data0, uint32_t *data1, uint32_t mbox, uint32_t *timeout);

void lil_kbl_pci_detect(LilGpu *gpu);

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait);
void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc);

void kbl_transcoder_enable(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_configure_clock(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_timings_configure(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_bpp_set(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void kbl_transcoder_set_dp_msa_misc(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void kbl_transcoder_ddi_polarity_setup(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_ddi_setup(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes);
void kbl_transcoder_configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t max_lanes, uint32_t bpp, bool downspread);

void kbl_pipe_src_size_set(LilGpu *gpu, LilCrtc *crtc);
void kbl_pipe_dithering_enable(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
