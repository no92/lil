#pragma once

#include <lil/intel.h>
#include <lil/vbt-types.h>

//
// TODO(CLEAN;SEPARATE) this header file is just plain stupid
//

#ifdef __cplusplus
extern "C" {
#endif

void lil_init_kbl_gpu(LilGpu *ret);

void lil_kbl_setup(LilGpu *gpu);

uint32_t kbl_cdclk_dec_to_int(uint32_t cdclk_freq_decimal);
void kbl_cdclk_set_freq(LilGpu *gpu, uint32_t cdclk_freq_int);
void kbl_cdclk_set_for_pixel_clock(LilGpu *gpu, uint32_t *clock);

void kbl_dpll_clock_set(LilGpu *gpu, LilCrtc *crtc);
void kbl_dpll_ctrl_enable(LilGpu *gpu, LilCrtc *crtc, uint32_t link_rate);

bool kbl_ddi_buf_enabled(LilGpu *gpu, LilCrtc *crtc);
bool kbl_ddi_hotplug_detected(LilGpu *gpu, enum LilDdiId ddi_id);
void kbl_ddi_buffer_setup_translations(LilGpu *gpu, LilConnector *con, uint32_t reg);
void kbl_ddi_power_enable(LilGpu *gpu, LilCrtc *crtc);
void kbl_ddi_power_disable(LilGpu *gpu, LilConnector *con);
void kbl_ddi_balance_leg_set(LilGpu *gpu, enum LilDdiId ddi_id, uint8_t balance_leg);
void kbl_ddi_clock_disable(LilGpu *gpu, LilCrtc *crtc);

bool kbl_pcode_rw(LilGpu *gpu, uint32_t *data0, uint32_t *data1, uint32_t mbox, uint32_t *timeout);

bool kbl_dp_pre_enable(LilGpu *gpu, LilConnector *con);

bool kbl_edp_pre_enable(LilGpu *gpu, LilConnector *con);
bool kbl_edp_aux_readable(LilGpu *gpu, LilConnector *con);
void kbl_edp_validate_clocks_for_bpp(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t link_rate, uint32_t *bpp);
bool kbl_edp_link_training(LilGpu *gpu, LilCrtc *crtc, uint32_t max_link_rate, uint8_t lane_count);

void lil_kbl_crtc_dp_shutdown(LilGpu *gpu, LilCrtc *crtc);
void lil_kbl_commit_modeset(struct LilGpu* gpu, struct LilCrtc* crtc);

void lil_kbl_pci_detect(LilGpu *gpu);

void kbl_plane_page_flip(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait);
void kbl_plane_disable(LilGpu *gpu, LilCrtc *crtc);
void kbl_plane_size_set(LilGpu *gpu, LilCrtc *crtc);

uint32_t kbl_transcoder_base(LilTranscoder id);
void kbl_transcoder_enable(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_disable(LilGpu *gpu, LilTranscoder transcoder);
void kbl_transcoder_ddi_disable(LilGpu *gpu, LilTranscoder transcoder);
void kbl_transcoder_clock_disable(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_clock_disable_by_id(LilGpu *gpu, LilTranscoder transcoder);
void kbl_transcoder_configure_clock(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_timings_configure(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_bpp_set(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void kbl_transcoder_set_dp_msa_misc(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void kbl_transcoder_ddi_polarity_setup(LilGpu *gpu, LilCrtc *crtc);
void kbl_transcoder_ddi_setup(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes);
void kbl_transcoder_configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t max_lanes, uint32_t bits_per_pixel);

void kbl_pipe_src_size_set(LilGpu *gpu, LilCrtc *crtc);
void kbl_pipe_dithering_enable(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void kbl_pipe_scaler_disable(LilGpu *gpu, LilCrtc *crtc);

void kbl_encoder_edp_init(LilGpu *gpu, LilEncoder *enc);
void kbl_encoder_dp_init(LilGpu *gpu, LilEncoder *enc, struct child_device *dev);
void kbl_encoder_hdmi_init(LilGpu *gpu, LilEncoder *enc, struct child_device *dev);

bool kbl_hdmi_pre_enable(LilGpu *gpu, LilConnector *con);
bool lil_kbl_hdmi_is_connected(LilGpu *gpu, LilConnector *con);
LilConnectorInfo lil_kbl_hdmi_get_connector_info(LilGpu *gpu, LilConnector *con);
void lil_kbl_hdmi_shutdown(LilGpu *gpu, LilCrtc *crtc);
void lil_kbl_hdmi_commit_modeset(LilGpu *gpu, LilCrtc *crtc);
void hdmi_pll_enable_sequence(LilGpu *gpu, LilCrtc *crtc);

void pll_find(LilGpu *gpu, LilCrtc *crtc);
void kbl_pll_disable(LilGpu *gpu, LilConnector *con);

void kbl_hotplug_enable(LilGpu *gpu);
void kbl_psr_disable(LilGpu *gpu);

uint16_t lil_brightness_get(LilGpu *gpu, LilConnector *con);
void lil_brightness_set(LilGpu *gpu, LilConnector *con, uint16_t level);

void hdmi_avi_infoframe_populate(LilCrtc *crtc, void *data);

#ifdef __cplusplus
}
#endif
