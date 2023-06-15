#include <lil/imports.h>
#include <lil/intel.h>

#include "src/pci.h"
#include "src/kaby_lake/gtt.h"
#include "src/kaby_lake/kbl.h"

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

static uint32_t get_gtt_size(void* device) {
    uint16_t mggc0 = lil_pci_readw(device, PCI_MGGC0);
    uint8_t size = 1 << ((mggc0 >> 6) & 0b11);

    return size * 1024 * 1024;
}

void lil_kbl_setup(LilGpu *gpu) {
	/* enable Bus Mastering and Memory + I/O space access */
	uint16_t command = lil_pci_readw(gpu->dev, PCI_HDR_COMMAND);
	lil_pci_writew(gpu->dev, PCI_HDR_COMMAND, command | 7); /* Bus Master | Memory Space | I/O Space */

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
}
