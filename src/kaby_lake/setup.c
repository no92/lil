#include <lil/imports.h>
#include <lil/intel.h>
#include <lil/pch.h>

#include "src/pci.h"
#include "src/regs.h"
#include "src/coffee_lake/crtc.h"
#include "src/coffee_lake/plane.h"
#include "src/kaby_lake/gtt.h"
#include "src/kaby_lake/kbl.h"
#include "src/kaby_lake/pch.h"

static struct {
	uint8_t select;
	int mb;
} graphics_mode_select_table[] = {
	{ 0, 0 }, { 1, 32 }, { 2, 64 }, { 3, 96 }, { 4, 128 }, { 5, 160 }, { 6, 192 }, { 7, 224 },
	{ 8, 256 }, { 9, 288 }, { 10, 320 }, { 11, 352 }, { 12, 384 }, { 13, 416 }, { 14, 448 }, { 15, 480 },
	{ 16, 512 }, { 32, 1024 }, { 48, 1536 }, { 64, 2048 },
	{ 240, 4 }, { 241, 8 }, { 242, 12 }, { 243, 16 }, { 244, 20 }, { 245, 24 }, { 246, 28 }, { 247, 32 },
	{ 248, 36 }, { 249, 40 }, { 250, 44 }, { 251, 48 }, { 252, 52 }, { 253, 56 }, { 254, 60 },
};

static void kbl_crtc_init(LilGpu *gpu);

static uint32_t get_gtt_size(void* device) {
    uint16_t mggc0 = lil_pci_readw(device, PCI_MGGC0);
    uint8_t size = 1 << ((mggc0 >> 6) & 0b11);

    return size * 1024 * 1024;
}

void lil_kbl_setup(LilGpu *gpu) {
	/* enable Bus Mastering and Memory + I/O space access */
	uint16_t command = lil_pci_readw(gpu->dev, PCI_HDR_COMMAND);
	lil_pci_writew(gpu->dev, PCI_HDR_COMMAND, command | 7); /* Bus Master | Memory Space | I/O Space */

	/* determine the PCH generation */
	kbl_pch_get_gen(gpu);
	if(gpu->pch_gen == LPT)
		/* LPT has this meme where apparently the reference clock is 125 MHz */
		gpu->ref_clock_freq = 125;
	else
		gpu->ref_clock_freq = 24;

	/* read the `GMCH Graphics Control` register */
	uint8_t graphics_mode_select = lil_pci_readb(gpu->dev, 0x51);
	size_t pre_allocated_memory_mb = 0;

	for(size_t i = 0; i < sizeof(graphics_mode_select_table) / sizeof(*graphics_mode_select_table); i++) {
		if(graphics_mode_select == graphics_mode_select_table[i].select) {
			pre_allocated_memory_mb = graphics_mode_select_table[i].mb;
			break;
		}
	}

	gpu->stolen_memory_pages = (pre_allocated_memory_mb << 10) >> 2;

	uintptr_t bar0_base;
    uintptr_t bar0_len;
    lil_get_bar(gpu->dev, 0, &bar0_base, &bar0_len);

    gpu->mmio_start = (uintptr_t) lil_map(bar0_base, bar0_len);

	/* Half of the BAR space is registers, half is GTT PTEs */
    gpu->gtt_size = bar0_len / 2;
    gpu->gtt_address = gpu->mmio_start + (bar0_len / 2);

	uintptr_t bar2_base;
    uintptr_t bar2_len;
    lil_get_bar(gpu->dev, 2, &bar2_base, &bar2_len);
    gpu->vram = (uintptr_t) lil_map(bar2_base, gpu->stolen_memory_pages << 12);

	lil_kbl_vmem_clear(gpu);

	uint8_t dpll0_link_rate = (REG(DPLL_CTRL1) >> 1) & DPLL_CTRL1_LINK_RATE_MASK(0);
	gpu->vco_8640 = dpll0_link_rate == 4 || dpll0_link_rate == 5;
	gpu->boot_cdclk_freq = kbl_cdclk_dec_to_int(REG(SWF_6) & CDCLK_CTL_DECIMAL_MASK);
    gpu->cdclk_freq = gpu->boot_cdclk_freq;

	kbl_crtc_init(gpu);
}

static void kbl_crtc_init(LilGpu *gpu) {
	size_t crtc_id = 0;

	/* TODO: support more than one CRTC */
	LilCrtc *crtc = gpu->connectors[0].crtc;
    crtc->transcoder = TRANSCODER_EDP;
    crtc->connector = &gpu->connectors[0];
    crtc->num_planes = 1;
    crtc->planes = lil_malloc(sizeof(LilPlane));
	crtc->plane_id = 0;

    for (int i = 0; i < crtc->num_planes; i++) {
        crtc->planes[i].enabled = 0;
        crtc->planes[i].pipe_id = 0;
        crtc->planes[i].update_surface = lil_cfl_update_primary_surface;
    }

    crtc->pipe_id = 0;
    crtc->commit_modeset = lil_kbl_commit_modeset;
    crtc->shutdown = lil_cfl_shutdown;

	enum LilPllId pll_id = 0;

	uint32_t pll_choice = REG(DPLL_CTRL2) & DPLL_CTRL2_DDI_CLOCK_SELECT_MASK(gpu->ddi_id) >> DPLL_CTRL2_DDI_CLOCK_SELECT_SHIFT(gpu->ddi_id);

	switch(pll_choice) {
		case 0: {
			if(REG(LCPLL1_CTL) & (1 << 31))
				pll_id = LCPLL1;
			break;
		}
		case 1: {
			if(REG(LCPLL2_CTL) & (1 << 31))
				pll_id = LCPLL2;
			break;
		}
		case 2: {
			if(REG(WRPLL_CTL1) & (1 << 31))
				pll_id = WRPLL1;
			break;
		}
		case 3: {
			if(REG(WRPLL_CTL2) & (1 << 31))
				pll_id = WRPLL2;
			break;
		}
		default:
			lil_panic("unsound PLL choice");
	}

	crtc->pll_id = pll_id;

	/* TODO: handle the VBT bdb block for fixed mode set */
}
