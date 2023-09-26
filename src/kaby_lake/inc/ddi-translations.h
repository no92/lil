#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lil_ddi_buf_trans_entry {
	uint32_t trans1;
	uint32_t trans2;
	uint8_t iboost;
};

struct lil_ddi_buf_trans {
	const struct lil_ddi_buf_trans_entry *entries;
	uint8_t num_entries;
	uint8_t hdmi_default_entry;
};

extern const struct lil_ddi_buf_trans kbl_trans_dp;
extern const struct lil_ddi_buf_trans kbl_u_trans_dp;
extern const struct lil_ddi_buf_trans kbl_y_trans_dp;
extern const struct lil_ddi_buf_trans skl_trans_edp;
extern const struct lil_ddi_buf_trans skl_u_trans_edp;
extern const struct lil_ddi_buf_trans skl_y_trans_edp;
extern const struct lil_ddi_buf_trans skl_trans_hdmi;
extern const struct lil_ddi_buf_trans skl_y_trans_hdmi;

extern uint8_t skl_translations_edp[160];
extern uint8_t skl_u_translations_edp[160];
extern uint8_t skl_y_translations_edp[160];
extern uint8_t kbl_translations_edp[160];
extern uint8_t kbl_u_translations_edp[160];
extern uint8_t kbl_y_translations_edp[160];

#ifdef __cplusplus
}
#endif
