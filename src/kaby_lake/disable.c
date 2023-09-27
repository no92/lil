#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

static uint32_t transcoders[4] = { TRANSCODER_A_BASE, TRANSCODER_B_BASE, TRANSCODER_C_BASE, TRANSCODER_EDP_BASE };

#define PORT_COUNT 4

static struct port_info {
	uint32_t strap_reg;
	uint32_t strap_mask;
	uint32_t hpd_enable_mask;
	uint32_t hpd_status_mask;
} ports[PORT_COUNT] = {
	{ DDI_BUF_CTL(0), (1 << 0), (1 << 28), (3 << 24), },
	{ SFUSE_STRAP, (1 << 2), (1 << 4), (3 << 0), },
	{ SFUSE_STRAP, (1 << 1), (1 << 12), (3 << 8), },
	{ SFUSE_STRAP, (1 << 0), (1 << 20), (3 << 16), },
};

void kbl_psr_disable(LilGpu *gpu) {
	lil_log(VERBOSE, "disabling PSR for all transcoders\n");

	for(size_t i = 0; i < 4; i++) {
		uint32_t srd_ctl = transcoders[i] + SRD_CTL;
		uint32_t srd_status = transcoders[i] + SRD_STATUS;

		if(REG(srd_ctl) & (1 << 31)) {
			REG(srd_ctl) &= ~(1 << 31);
			wait_for_bit_unset(REG_PTR(srd_status), 7 << 29, 10, 1);
		}
	}
}

void kbl_hotplug_enable(LilGpu *gpu) {
	lil_log(VERBOSE, "enabling HPD for all ports\n");

	for(size_t i = 0; i < PORT_COUNT; i++) {
		struct port_info *info = &ports[i];

		if(REG(info->strap_reg) & info->strap_mask) {
			REG(SHOTPLUG_CTL) = (REG(SHOTPLUG_CTL) & 0x03030303) | info->hpd_enable_mask | info->hpd_status_mask;
		}
	}
}
