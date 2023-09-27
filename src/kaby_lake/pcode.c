#include <lil/imports.h>

#include "src/kaby_lake/kbl.h"
#include "src/regs.h"

// TODO(CLEAN;UNCLEAR_ACTIONS)	what does timeout do and what does that weird if statement do?
bool kbl_pcode_rw(LilGpu *gpu, uint32_t *data0, uint32_t *data1, uint32_t mbox, uint32_t *timeout) {
	if(*timeout != (10 * (*timeout / 10)))
		*timeout = *timeout - (*timeout % 10) + 10;

	if(!wait_for_bit_unset(REG_PTR(GEN6_PCODE_MAILBOX), GEN6_PCODE_MAILBOX_READY, 100, 10))
		return false;

	REG(GEN6_PCODE_DATA) = *data0;
	REG(GEN6_PCODE_DATA1) = *data1;
	REG(GEN6_PCODE_MAILBOX) = mbox | GEN6_PCODE_MAILBOX_READY;

	bool success = false;

	if(*timeout) {
		if(!wait_for_bit_unset(REG_PTR(GEN6_PCODE_MAILBOX), GEN6_PCODE_MAILBOX_READY, *timeout, 10)) {
			return success;
		}

		success = true;
		*data0 = REG(GEN6_PCODE_DATA);
		*data1 = REG(GEN6_PCODE_DATA1);
	}

	return success;
}
