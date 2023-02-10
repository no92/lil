#include <cpuid.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lil/imports.h>

#define FW_CFG_PORT_SEL 0x510
#define FW_CFG_PORT_DATA 0x511
#define FW_CFG_PORT_DMA 0x514

#define FW_CFG_SIGNATURE 0x0000
#define FW_CFG_ID 0x0001
#define FW_CFG_FILE_DIR 0x0019

static const char sig_hv_tcg[12] = "TCGTCGTCGTCG";
static const char sig_hv_kvm[12] = "KVMKVMKVM\0\0\0";

static bool is_hypervisor(void) {
	uint32_t eax, ebx, ecx, edx;
	__cpuid(0x40000000, eax, ebx, ecx, edx);

	char str[12];
	memcpy(str + 0, &ebx, 4);
	memcpy(str + 4, &ecx, 4);
	memcpy(str + 8, &edx, 4);

	return !memcmp(str, sig_hv_tcg, 12) || !memcmp(str, sig_hv_kvm, 12);
}

static bool fw_cfg_present(void) {
	char signature[4];
	lil_outw(FW_CFG_PORT_SEL, FW_CFG_SIGNATURE);

	for(size_t i = 0; i < 4; i++) {
		signature[i] = lil_inb(FW_CFG_PORT_DATA);
	}

	if(memcmp(signature, "QEMU", 4))
		return false;

	return true;
}

static bool fw_cfg_dma(void) {
	uint32_t dma_port_signature[2];
	dma_port_signature[0] = lil_ind(FW_CFG_PORT_DMA);
	dma_port_signature[1] = lil_ind(FW_CFG_PORT_DMA + 4);

	return !memcmp(&dma_port_signature, "QEMU CFG", 8);
}

static bool fw_cfg_read(int selector, size_t len, void *buf) {
	uint8_t *read = buf;

	if(selector)
		lil_outw(FW_CFG_PORT_SEL, selector);

	for(size_t i = 0; i < len; i++) {
		read[i] = lil_inb(FW_CFG_PORT_DATA);
	}

	return true;
}

struct fw_cfg_file {
	uint32_t size;
	uint16_t select;
	uint16_t reserved;
	char name[56];
};

bool fw_cfg_find(const char *name, size_t *size, uint16_t *file_id) {
	if(!is_hypervisor())
		return false;

	if(!fw_cfg_present())
		return false;

	// TODO: maybe implement this using DMA instead of port I/O?
	// if(!fw_cfg_dma())
	// 	return false;

	uint32_t file_count = 0;
	fw_cfg_read(FW_CFG_FILE_DIR, 4, &file_count);

	file_count = __builtin_bswap32(file_count);

	for(size_t i = 0; i < file_count; i++) {
		struct fw_cfg_file file;
		fw_cfg_read(0, sizeof(struct fw_cfg_file), &file);

		if(!strcmp(file.name, name)) {
			*size = __builtin_bswap32(file.size);
			*file_id = __builtin_bswap16(file.select);
			break;
		}
	}

	return true;
}

bool fw_cfg_readfile(uint16_t file_id, size_t len, void *buf) {
	fw_cfg_read(file_id, len, buf);

	return true;
}
