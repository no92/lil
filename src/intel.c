#include <lil/intel.h>
#include <lil/imports.h>
#include <lil/vbt.h>

#include "src/ivy_bridge/ivb.h"
#include "src/kaby_lake/kbl.h"
#include "src/pci.h"
#include "src/regs.h"

void lil_init_gpu(LilGpu* ret, void* device) {
   uint32_t config_0 = lil_pci_readd(device, PCI_HDR_VENDOR_ID);
    if (config_0 == 0xffffffff) {
        return;
    }

    uint16_t device_id = (uint16_t) (config_0 >> 16);
    uint16_t vendor_id = (uint16_t) config_0;
    if (vendor_id != 0x8086) {
        return;
    }

    uint32_t class = lil_pci_readw(device, PCI_HDR_SUBCLASS);
    if (class != 0x300) {
        return;
    }

	ret->dev = device;

	vbt_init(ret);

    ret->subgen = SUBGEN_NONE;

    switch (device_id) {
        case 0x0166 : {
            ret->gen = GEN_IVB;
            lil_init_ivb_gpu(ret, device);
            break;
        }

        case 0x3184:
        case 0x3185:
        case 0x5916: {
			uint8_t prog_if = lil_pci_readb(device, PCI_HDR_PROG_IF);
			lil_assert(!prog_if);
			lil_init_kbl_gpu(ret);
			break;
		}

        //case 0x3E9B:
        //case 0x5917: {
        //    ret->gen = GEN_CFL;
        //    lil_init_cfl_gpu(ret, device);
        //    break;
        //}
    }
}

void kbl_dump_registers(LilGpu* gpu) {
	lil_log(VERBOSE, "power control:\n");
	lil_log(VERBOSE, "\t0x00045400 0x%08x (PWR_WELL_CTL_BIOS)\n", REG(0x00045400));
	lil_log(VERBOSE, "\t0x00045404 0x%08x (PWR_WELL_CTL_DRIVER)\n", REG(0x00045404));
	lil_log(VERBOSE, "\t combined: 0x%08x\n", REG(0x00045400) | REG(0x00045404));
	lil_log(VERBOSE, "\t0x00045408 0x%08x (PWR_WELL_CTL_KVMR)\n", REG(0x00045408));
	lil_log(VERBOSE, "\t0x0004540c 0x%08x (PWR_WELL_CTL_DEBUG)\n", REG(0x0004540c));
	lil_log(VERBOSE, "\t0x00045410 0x%08x (PWR_WELL_CTL5)\n", REG(0x00045410));
	lil_log(VERBOSE, "\t0x00045414 0x%08x (PWR_WELL_CTL6)\n", REG(0x00045414));
	lil_log(VERBOSE, "\t all combined: 0x%08x\n", REG(0x00045400) | REG(0x00045404) | REG(0x00045408) | REG(0x0004540c) | REG(0x00045410) | REG(0x00045414));

	lil_log(VERBOSE, "pipe EDP:\n");
	lil_log(VERBOSE, "\t0x0006f030 0x%08x (PIPE_EDP_DATA_M1)\n", REG(0x0006f030));
	lil_log(VERBOSE, "\t0x0006f034 0x%08x (PIPE_EDP_DATA_N1)\n", REG(0x0006f034));
	lil_log(VERBOSE, "\t0x0006f040 0x%08x (PIPE_EDP_LINK_M1)\n", REG(0x0006f040));
	lil_log(VERBOSE, "\t0x0006f044 0x%08x (PIPE_EDP_LINK_N1)\n", REG(0x0006f044));
	lil_log(VERBOSE, "\t0x0006f400 0x%08x (PIPE_EDP_DDI_FUNC_CTL)\n", REG(0x0006f400));
	lil_log(VERBOSE, "\t0x0006f410 0x%08x (PIPE_EDP_MSA_MISC)\n", REG(0x0006f410));
	lil_log(VERBOSE, "\t0x0007f008 0x%08x (PIPE_EDP_CONF)\n", REG(0x0007f008));
	lil_log(VERBOSE, "\t0x0006f000 0x%08x (HTOTAL_EDP)\n", REG(0x0006f000));
	lil_log(VERBOSE, "\t0x0006f004 0x%08x (HBLANK_EDP)\n", REG(0x0006f004));
	lil_log(VERBOSE, "\t0x0006f008 0x%08x (HSYNC_EDP)\n", REG(0x0006f008));
	lil_log(VERBOSE, "\t0x0006f00c 0x%08x (VTOTAL_EDP)\n", REG(0x0006f00c));
	lil_log(VERBOSE, "\t0x0006f010 0x%08x (VBLANK_EDP)\n", REG(0x0006f010));
	lil_log(VERBOSE, "\t0x0006f014 0x%08x (VSYNC_EDP)\n", REG(0x0006f014));

	for(size_t idx_con = 0; idx_con < gpu->num_connectors; idx_con++) {
		LilConnector* con = gpu->connectors + idx_con;
		char* connector_type = NULL;
		switch(con->type) {
			case DISPLAYPORT: { connector_type = "DP"; break;}
			case HDMI: { connector_type = "HDMI"; break; }
			case EDP: { connector_type = "eDP"; break; }
			case LVDS: { connector_type = "LVDS"; break; }
		}
		lil_assert(connector_type);
		lil_log(VERBOSE, "connector %i (%s):\n", (int)idx_con, connector_type);

		// Print plane information
		for(size_t idx_con_plane = 0; idx_con_plane < con->crtc->num_planes; idx_con_plane++) {
			LilPlane* plane = con->crtc->planes + idx_con_plane;
			char plane_char = (char)('A' + plane->pipe_id);
			lil_log(VERBOSE, "\tplane %c:\n", plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (DSP%cCNTR)\n", PRI_CTL(plane->pipe_id), REG(PRI_CTL(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (DSP%cSTRIDE)\n", PRI_STRIDE(plane->pipe_id), REG(PRI_STRIDE(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (PLANE_POS_1_%c)\n", PLANE_POS(plane->pipe_id), REG(PLANE_POS(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (PLANE_SIZE_1_%c)\n", PLANE_SIZE(plane->pipe_id), REG(PLANE_SIZE(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (DSP%cSURF)\n", PRI_SURFACE(plane->pipe_id), REG(PRI_SURFACE(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (PLANE_WM_1_%c_0)\n", PLANE_WM_1(plane->pipe_id), REG(PLANE_WM_1(plane->pipe_id)), plane_char);
			lil_log(VERBOSE, "\t\t0x%08x 0x%08x (PLANE_BUF_CFG_1_%c_0)\n", PLANE_BUG_CFG_1(plane->pipe_id), REG(PLANE_BUG_CFG_1(plane->pipe_id)), plane_char);
		}

		// Print DDI information
		char ddi_char = (char)('A' + con->ddi_id);
		uint32_t ddi_buf_ctl = REG(DDI_BUF_CTL(con->ddi_id));
		lil_log(VERBOSE, "\tDDI%c:\n", ddi_char);
		lil_log(VERBOSE, "\t\t0x%08x 0x%08x (DDI_BUF_CTL_%c) (%sabled)\n", DDI_BUF_CTL(con->ddi_id), ddi_buf_ctl, ddi_char,
			ddi_buf_ctl & DDI_BUF_CTL_ENABLE ? "en" : "dis");
	}
	lil_log(VERBOSE, "PLLs:\n");
	lil_log(VERBOSE, "\t0x00046010 0x%08x (LCPLL1_CTL)\n", REG(0x00046010));
	lil_log(VERBOSE, "\t0x00046014 0x%08x (LCPLL2_CTL)\n", REG(0x00046014));
	lil_log(VERBOSE, "\t0x00046040 0x%08x (WRPLL_CTL_1)\n", REG(0x00046040));
	lil_log(VERBOSE, "\t0x00046060 0x%08x (WRPLL_CTL_2)\n", REG(0x00046060));
	lil_log(VERBOSE, "\t0x0006C040 0x%08x (DPLL1_CFGR1)\n", REG(0x0006C040));
	lil_log(VERBOSE, "\t0x0006C044 0x%08x (DPLL1_CFGR2)\n", REG(0x0006C044));
	lil_log(VERBOSE, "\t0x0006C058 0x%08x (DPLL_CTRL1)\n", REG(0x0006C058));
	lil_log(VERBOSE, "\t0x0006C05C 0x%08x (DPLL_CTRL2)\n", REG(0x0006C05C));
	lil_log(VERBOSE, "\t0x0006C060 0x%08x (DPLL_STATUS)\n", REG(0x0006C060));
}

struct testcase {
	size_t reg;
	const char *name;
	uint32_t val;
};

static struct testcase kaby_lake_edp_hdmi[] = {
	{ 0x0006f030, "PIPE_EDP_DATA_M1", 0x7e62a190 },
	{ 0x0006f034, "PIPE_EDP_DATA_N1", 0x00800000 },
	{ 0x0006f040, "PIPE_EDP_LINK_M1", 0x00041c10 },
	{ 0x0006f044, "PIPE_EDP_LINK_N1", 0x00080000 },
	{ 0x0006f410, "PIPE_EDP_MSA_MISC", 0x00000021 },

	{ 0x0006f000, "HTOTAL_EDP", 0x081f077f },
	{ 0x0006f004, "HBLANK_EDP", 0x081f077f },
	{ 0x0006f008, "HSYNC_EDP", 0x07cf07af },
	{ 0x0006f00c, "VTOTAL_EDP", 0x04560437 },
	{ 0x0006f010, "VBLANK_EDP", 0x04560437 },
	{ 0x0006f014, "VSYNC_EDP", 0x043f043a },

	{ 0x00064040, "DP_TP_CTL_A", 0x80040300 },

	{ 0x00046144, "TRANSB_CLK_SEL", 0x60000000 },
	{ 0x00061000, "HTOTAL_B", 0x0a9f09ff },
	{ 0x00061004, "HBLANK_B", 0x0a9f09ff },
	{ 0x00061008, "HSYNC_B", 0x0a4f0a2f },
	{ 0x0006100c, "VTOTAL_B", 0x05c8059f },
	{ 0x00061010, "VBLANK_B", 0x05c8059f },
	{ 0x00061014, "VSYNC_B", 0x05a705a2 },
	{ 0x0007027c, "PLANE_BUF_CFG_1_A", 0x009F0000 },
	{ 0x0007127c, "PLANE_BUF_CFG_1_B", 0x013F00A0 },
	{ 0x00070240, "PLANE_WM_1_A_0", 0x80008098 },
	{ 0x00071240, "PLANE_WM_1_B_0", 0x80008098 },
	{ 0x00068980, "PS_CTRL_1_B", 0x00000000 },
	{ 0x00068974, "PS_WIN_SZ_1_B", 0x00000000 },
	{ 0x00061400, "PIPEB_DDI_FUNC_CTL", 0xa0010000 },
	// { 0x00064200, "DDI_BUF_CTL_C", 0x80000080 },
	{ 0x00064200, "DDI_BUF_CTL_C", 0x80000000 },

	{ 0x00071180, "DSPBCNTR", 0x84002000 },
	{ 0x000711a4, "DSPBTILEOFF", 0x00000000 },
	{ 0x00071188, "DSPBSTRIDE", 0x000000a0 },

	{ 0x00071190, "PLANE_SIZE_1_B", 0x059f09ff },
	{ 0x0007118c, "PLANE_POS_1_B", 0x00000000 },

	{ 0x00070008, "PIPEACONF", 0x00000000 },
	{ 0x0006001c, "PIPEASRC", 0x077f0437 },
	{ 0x00070030, "PIPEAMISC", 0x00000000 },

	{ 0x00071008, "PIPEBCONF", 0xc0000000 },
	{ 0x0006101c, "PIPEBSRC", 0x09ff059f },
	{ 0x00071030, "PIPEBMISC", 0x00000000 },

	{ 0x00046010, "LCPLL1_CTL", 0xC0000000 },
	{ 0x00046014, "LCPLL2_CTL", 0x80000000 },
	{ 0x0006c040, "DPLL1_CFGR1", 0x80800192 },
	{ 0x0006c044, "DPLL1_CFGR2", 0x000002a4 },
	{ 0x0006c058, "DPLL_CTRL1", 0x000008c3 },
	{ 0x0006c05c, "DPLL_CTRL2", 0x00a100c1 },

	{ 0x00070190, "PLANE_SIZE_1_A", 0x0437077f },
	{ 0x0007018c, "PLANE_POS_1_A", 0x00000000 },

	{ 0x00070180, "DSPACNTR", 0x84002000 },
	{ 0x000701a4, "DSPATILEOFF", 0x00000000 },
	{ 0x00070188, "DSPASTRIDE", 0x00000078 },
	{ 0x0007019c, "DSPASURF", 0x00000000 },
	{ 0x0006001c, "PIPEASRC", 0x077f0437 },
	{ 0x00070030, "PIPEAMISC", 0x00000000 },
	{ 0x00068180, "PS_CTRL_1_A", 0x00000000 },
	{ 0x00068174, "PS_WIN_SZ_1_A", 0x00000000 },
	{ 0x0007f008, "PIPE_EDP_CONF", 0xc0000000 },
	{ 0x0006f400, "PIPE_EDP_DDI_FUNC_CTL", 0x82000002 },

	{ 0x00064040, "DP_TP_CTL_A", 0x80040300 },

	{ 0x00045270, "WM_LINETIME_A", 0x00000000 },
	{ 0x00045274, "WM_LINETIME_B", 0x00000000 },
	{ 0x00045278, "WM_LINETIME_C", 0x00000000 },

	{ 0x000c7204, "PCH_PP_CONTROL", 0x00000007 },

	{ 0, NULL, 0 },
};

static struct testcase kaby_lake_edp[] = {
	{ 0x000c7204, "PCH_PP_CONTROL", 0x00000007 },
	{ 0x0007f008, "PIPE_EDP_CONF", 0xc0000000 },
	{ 0x0006f400, "PIPE_EDP_DDI_FUNC_CTL", 0x82000002 },
	{ 0x00068280, "PS_CTRL_2_A", 0x00000000 },
	{ 0x00068274, "PS_WIN_SZ_2_A", 0x00000000 },
	{ 0x00068180, "PS_CTRL_1_A", 0x00000000 },
	{ 0x00068174, "PS_WIN_SZ_1_A", 0x00000000 },
	{ 0x00070180, "DSPACNTR", 0x84002000 },
	{ 0x000701a4, "DSPATILEOFF", 0x00000000 },
	{ 0x00070190, "PLANE_SIZE_1_A", 0x0437077f },
	{ 0x00070188, "DSPASTRIDE", 0x00000078 },
	{ 0x0007018c, "PLANE_POS_1_A", 0x00000000 },
	{ 0x0007019c, "DSPASURF", 0x00000000 },
	{ 0x0006001c, "PIPEASRC", 0x077f0437 },
	{ 0x00070030, "PIPEAMISC", 0x00000000 },
	{ 0x0007027c, "PLANE_BUF_CFG_1_A", 0x009f0008 },
	{ 0x00070240, "PLANE_WM_1_A_0", 0x80008098 },
	{ 0x0006f030, "PIPE_EDP_DATA_M1", 0x7e62a190 },
	{ 0x0006f034, "PIPE_EDP_DATA_N1", 0x00800000 },
	{ 0x0006f040, "PIPE_EDP_LINK_M1", 0x00041c10 },
	{ 0x0006f044, "PIPE_EDP_LINK_N1", 0x00080000 },
	{ 0x0006f410, "PIPE_EDP_MSA_MISC", 0x00000021 },
	{ 0x0006f000, "HTOTAL_EDP", 0x081f077f },
	{ 0x0006f004, "HBLANK_EDP", 0x081f077f },
	{ 0x0006f008, "HSYNC_EDP", 0x07cf07af },
	{ 0x0006f00c, "VTOTAL_EDP", 0x04560437 },
	{ 0x0006f010, "VBLANK_EDP", 0x04560437 },
	{ 0x0006f014, "VSYNC_EDP", 0x043f043a },
	{ 0, NULL, 0 },
};

static void run_testcase(LilGpu *gpu, struct testcase *t, const char *name) {
	lil_log(INFO, "running test case %s:\n", name);

	for(size_t i = 0; t[i].reg; i++) {
		uint32_t val = REG(t[i].reg);

		if(val != t[i].val) {
			lil_log(WARNING, "\t%s (0x%06lx) is 0x%08x instead of 0x%08x\n", t[i].name, t[i].reg, val, t[i].val);
		}
	}
}

void lil_testcase(LilGpu *gpu) {
	// run_testcase(gpu, kaby_lake_edp_hdmi, "Kaby Lake eDP + HDMI");
	// run_testcase(gpu, kaby_lake_edp, "Kaby Lake eDP");
}
