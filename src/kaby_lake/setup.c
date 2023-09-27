#include <lil/imports.h>
#include <lil/intel.h>
#include <lil/pch.h>
#include <lil/vbt.h>

#include "lil/vbt-types.h"
#include "src/pci.h"
#include "src/regs.h"
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

static void kbl_crtc_init(LilGpu *gpu, LilCrtc *crtc);

static uint32_t get_gtt_size(void* device) {
    uint16_t mggc0 = lil_pci_readw(device, PCI_MGGC0);
    uint8_t size = 1 << ((mggc0 >> 6) & 0b11);

    return size * 1024 * 1024;
}

static void wm_latency_setup(LilGpu *gpu) {
	uint32_t data0 = 0;
	uint32_t data1 = 0;
	uint32_t timeout = 100;
	if(kbl_pcode_rw(gpu, &data0, &data1, 6, &timeout)) {
		gpu->mem_latency_first_set = data0;

		data0 = 1;
		data1 = 0;
		timeout = 100;

		if(kbl_pcode_rw(gpu, &data0, &data1, 6, &timeout)) {
			gpu->mem_latency_second_set = data0;
		}
	}
}

void lil_kbl_setup(LilGpu *gpu) {
	/* enable Bus Mastering and Memory + I/O space access */
	uint16_t command = lil_pci_readw(gpu->dev, PCI_HDR_COMMAND);
	lil_pci_writew(gpu->dev, PCI_HDR_COMMAND, command | 7); /* Bus Master | Memory Space | I/O Space */
	command = lil_pci_readw(gpu->dev, PCI_HDR_COMMAND);
	lil_assert(command & 2);

	/* determine the PCH generation */
	kbl_pch_get_gen(gpu);
	lil_assert(gpu->pch_gen != INVALID_PCH_GEN);
	if(gpu->pch_gen == LPT) {
		/* LPT has this meme where apparently the reference clock is 125 MHz */
		gpu->ref_clock_freq = 125;
	} else {
		gpu->ref_clock_freq = 24;
	}

	if(gpu->pch_gen != NO_PCH)
		lil_log(VERBOSE, "\tPCH gen %u\n", gpu->pch_gen);

	/* read the `GMCH Graphics Control` register */
	uint8_t graphics_mode_select = lil_pci_readb(gpu->dev, 0x51);
	size_t pre_allocated_memory_mb = 0;

	for(size_t i = 0; i < sizeof(graphics_mode_select_table) / sizeof(*graphics_mode_select_table); i++) {
		if(graphics_mode_select == graphics_mode_select_table[i].select) {
			pre_allocated_memory_mb = graphics_mode_select_table[i].mb;
			break;
		}
	}

	lil_log(VERBOSE, "%lu MiB pre-allocated memory\n", pre_allocated_memory_mb);

	gpu->stolen_memory_pages = (pre_allocated_memory_mb << 10) >> 2;

	uintptr_t bar0_base;
    uintptr_t bar0_len;
    lil_get_bar(gpu->dev, 0, &bar0_base, &bar0_len);

    gpu->mmio_start = (uintptr_t) lil_map(bar0_base, bar0_len);
	gpu->gpio_start = 0xC0000;

	/* Half of the BAR space is registers, half is GTT PTEs */
    gpu->gtt_size = bar0_len / 2;
    gpu->gtt_address = gpu->mmio_start + (bar0_len / 2);

	uintptr_t bar2_base;
    uintptr_t bar2_len;
    lil_get_bar(gpu->dev, 2, &bar2_base, &bar2_len);
    gpu->vram = (uintptr_t) lil_map(bar2_base, gpu->stolen_memory_pages << 12);

	lil_kbl_vmem_clear(gpu);

	kbl_hotplug_enable(gpu);
	kbl_psr_disable(gpu);

	/* TODO: on cold boot, perform the display init sequence */
	// Disable every transcoder
	for(LilTranscoder transcoder = TRANSCODER_A; transcoder <= TRANSCODER_C; transcoder++) {
		kbl_transcoder_disable(gpu, transcoder);
		kbl_transcoder_ddi_disable(gpu, transcoder);
		kbl_transcoder_clock_disable_by_id(gpu, transcoder);
	}

	REG(DPLL_CTRL2) |= (1 << 16);

	uint8_t dpll0_link_rate = (REG(DPLL_CTRL1) & DPLL_CTRL1_LINK_RATE_MASK(0)) >> 1;
	gpu->vco_8640 = dpll0_link_rate == 4 || dpll0_link_rate == 5;
	gpu->boot_cdclk_freq = kbl_cdclk_dec_to_int(REG(SWF_6) & CDCLK_CTL_DECIMAL_MASK);
    gpu->cdclk_freq = kbl_cdclk_dec_to_int(REG(CDCLK_CTL) & CDCLK_CTL_DECIMAL_MASK);

	wm_latency_setup(gpu);

	vbt_setup_children(gpu);

	for(size_t i = 0; i < gpu->num_connectors; i++) {
		lil_log(INFO, "pre-enabling connector %lu\n", i);

		switch(gpu->connectors[i].type) {
			case EDP: {
				if(!kbl_edp_pre_enable(gpu, &gpu->connectors[i]))
					lil_panic("eDP pre-enable failed");
				break;
			}
			case DISPLAYPORT: {
				if(!kbl_dp_pre_enable(gpu, &gpu->connectors[i]))
					lil_log(INFO, "DP pre-enable for connector %lu failed\n", i);
				break;
			}
			case HDMI: {
				if(!kbl_hdmi_pre_enable(gpu, &gpu->connectors[i]))
					lil_log(INFO, "HDMI pre-enable for connector %lu failed\n", i);
				break;
			}
			default: {
				lil_log(WARNING, "unhandled pre-enable for type %u\n", gpu->connectors[i].type);
				break;
			}
		}
	}

	for(size_t i = 0; i < gpu->num_connectors; i++) {
		kbl_crtc_init(gpu, gpu->connectors[i].crtc);
	}
}

static void kbl_crtc_init(LilGpu *gpu, LilCrtc *crtc) {
	enum LilPllId pll_id = 0;

	// if((REG(SWF_24) & 0xFFFFFF) == 0) {
	// 	return;
	// }

	if(crtc->connector->type != EDP) {
		uint32_t pll_choice = REG(DPLL_CTRL2) & DPLL_CTRL2_DDI_CLOCK_SELECT_MASK(crtc->connector->ddi_id) >> DPLL_CTRL2_DDI_CLOCK_SELECT_SHIFT(crtc->connector->ddi_id);

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
	}

	crtc->pll_id = pll_id;

	const struct bdb_fixed_mode_set *f = (const void *) vbt_get_bdb_block(gpu->vbt_header, BDB_FIXED_MODE_SET);
	if(f) {
		/* TODO: handle the VBT bdb block for fixed mode set */
		lil_assert(!f->feature_enable);
	}
}
