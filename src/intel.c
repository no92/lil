#include <lil/intel.h>
#include <lil/imports.h>

#include "src/coffee_lake/cfl.h"
#include "src/ivy_bridge/ivb.h"
#include "src/pci.h"

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

    switch (device_id) {
        case 0x0166 : {
            ret->gen = GEN_IVB;
            lil_init_ivb_gpu(ret, device);
            break;
        }

        case 0x3E9B:
        case 0x5917: {
            ret->gen = GEN_CFL;
            lil_init_cfl_gpu(ret, device);
            break;
        }
    }
}
