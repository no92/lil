#include "src/kaby_lake/ddi-translations.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(int[1 - 2 * !!__builtin_types_compatible_p(typeof(arr), typeof(&arr[0]))])) * 0)

/* Kabylake H and S */
static const struct lil_ddi_buf_trans_entry _kbl_trans_dp[] = {
	{ 0x00002016, 0x000000A0, 0x0 },
	{ 0x00005012, 0x0000009B, 0x0 },
	{ 0x00007011, 0x00000088, 0x0 },
	{ 0x80009010, 0x000000C0, 0x1 },
	{ 0x00002016, 0x0000009B, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000C0, 0x1 },
	{ 0x00002016, 0x00000097, 0x0 },
	{ 0x80005012, 0x000000C0, 0x1 },
};

const struct lil_ddi_buf_trans kbl_trans_dp = {
	.entries = _kbl_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_trans_dp),
};

/* Kabylake U */
static const struct lil_ddi_buf_trans_entry _kbl_u_trans_dp[] = {
	{ 0x0000201B, 0x000000A1, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000CD, 0x3 },
	{ 0x80009010, 0x000000C0, 0x3 },
	{ 0x0000201B, 0x0000009D, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
	{ 0x80007011, 0x000000C0, 0x3 },
	{ 0x00002016, 0x0000004F, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
};

const struct lil_ddi_buf_trans kbl_u_trans_dp = {
	.entries = _kbl_u_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_u_trans_dp),
};

/* Kabylake Y */
static const struct lil_ddi_buf_trans_entry _kbl_y_trans_dp[] = {
	{ 0x00001017, 0x000000A1, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000CD, 0x3 },
	{ 0x8000800F, 0x000000C0, 0x3 },
	{ 0x00001017, 0x0000009D, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
	{ 0x80007011, 0x000000C0, 0x3 },
	{ 0x00001017, 0x0000004C, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
};

const struct lil_ddi_buf_trans kbl_y_trans_dp = {
	.entries = _kbl_y_trans_dp,
	.num_entries = ARRAY_SIZE(_kbl_y_trans_dp),
};

/*
 * Skylake/Kabylake H and S
 * eDP 1.4 low vswing translation parameters
 */
static const struct lil_ddi_buf_trans_entry _skl_trans_edp[] = {
	{ 0x00000018, 0x000000A8, 0x0 },
	{ 0x00004013, 0x000000A9, 0x0 },
	{ 0x00007011, 0x000000A2, 0x0 },
	{ 0x00009010, 0x0000009C, 0x0 },
	{ 0x00000018, 0x000000A9, 0x0 },
	{ 0x00006013, 0x000000A2, 0x0 },
	{ 0x00007011, 0x000000A6, 0x0 },
	{ 0x00000018, 0x000000AB, 0x0 },
	{ 0x00007013, 0x0000009F, 0x0 },
	{ 0x00000018, 0x000000DF, 0x0 },
};

const struct lil_ddi_buf_trans skl_trans_edp = {
	.entries = _skl_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_trans_edp),
};

/*
 * Skylake/Kabylake U
 * eDP 1.4 low vswing translation parameters
 */
static const struct lil_ddi_buf_trans_entry _skl_u_trans_edp[] = {
	{ 0x00000018, 0x000000A8, 0x0 },
	{ 0x00004013, 0x000000A9, 0x0 },
	{ 0x00007011, 0x000000A2, 0x0 },
	{ 0x00009010, 0x0000009C, 0x0 },
	{ 0x00000018, 0x000000A9, 0x0 },
	{ 0x00006013, 0x000000A2, 0x0 },
	{ 0x00007011, 0x000000A6, 0x0 },
	{ 0x00002016, 0x000000AB, 0x0 },
	{ 0x00005013, 0x0000009F, 0x0 },
	{ 0x00000018, 0x000000DF, 0x0 },
};

const struct lil_ddi_buf_trans skl_u_trans_edp = {
	.entries = _skl_u_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_u_trans_edp),
};

/*
 * Skylake/Kabylake Y
 * eDP 1.4 low vswing translation parameters
 */
static const struct lil_ddi_buf_trans_entry _skl_y_trans_edp[] = {
	{ 0x00000018, 0x000000A8, 0x0 },
	{ 0x00004013, 0x000000AB, 0x0 },
	{ 0x00007011, 0x000000A4, 0x0 },
	{ 0x00009010, 0x000000DF, 0x0 },
	{ 0x00000018, 0x000000AA, 0x0 },
	{ 0x00006013, 0x000000A4, 0x0 },
	{ 0x00007011, 0x0000009D, 0x0 },
	{ 0x00000018, 0x000000A0, 0x0 },
	{ 0x00006012, 0x000000DF, 0x0 },
	{ 0x00000018, 0x0000008A, 0x0 },
};

const struct lil_ddi_buf_trans skl_y_trans_edp = {
	.entries = _skl_y_trans_edp,
	.num_entries = ARRAY_SIZE(_skl_y_trans_edp),
};

/* Skylake/Kabylake U, H and S */
static const struct lil_ddi_buf_trans_entry _skl_trans_hdmi[] = {
	{ 0x00000018, 0x000000AC, 0x0 },
	{ 0x00005012, 0x0000009D, 0x0 },
	{ 0x00007011, 0x00000088, 0x0 },
	{ 0x00000018, 0x000000A1, 0x0 },
	{ 0x00000018, 0x00000098, 0x0 },
	{ 0x00004013, 0x00000088, 0x0 },
	{ 0x80006012, 0x000000CD, 0x1 },
	{ 0x00000018, 0x000000DF, 0x0 },
	{ 0x80003015, 0x000000CD, 0x1 },
	{ 0x80003015, 0x000000C0, 0x1 },
	{ 0x80000018, 0x000000C0, 0x1 },
};

const struct lil_ddi_buf_trans skl_trans_hdmi = {
	.entries = _skl_trans_hdmi,
	.num_entries = ARRAY_SIZE(_skl_trans_hdmi),
	.hdmi_default_entry = 8,
};

/* Skylake/Kabylake Y */
static const struct lil_ddi_buf_trans_entry _skl_y_trans_hdmi[] = {
	{ 0x00000018, 0x000000A1, 0x0 },
	{ 0x00005012, 0x000000DF, 0x0 },
	{ 0x80007011, 0x000000CB, 0x3 },
	{ 0x00000018, 0x000000A4, 0x0 },
	{ 0x00000018, 0x0000009D, 0x0 },
	{ 0x00004013, 0x00000080, 0x0 },
	{ 0x80006013, 0x000000C0, 0x3 },
	{ 0x00000018, 0x0000008A, 0x0 },
	{ 0x80003015, 0x000000C0, 0x3 },
	{ 0x80003015, 0x000000C0, 0x3 },
	{ 0x80000018, 0x000000C0, 0x3 },
};

const struct lil_ddi_buf_trans skl_y_trans_hdmi = {
	.entries = _skl_y_trans_hdmi,
	.num_entries = ARRAY_SIZE(_skl_y_trans_hdmi),
	.hdmi_default_entry = 8,
};
