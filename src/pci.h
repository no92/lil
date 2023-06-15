#pragma once

#include <stdint.h>

#define PCI_HDR_VENDOR_ID 0x00
#define PCI_HDR_COMMAND 0x04
#define PCI_HDR_PROG_IF 0x09
#define PCI_HDR_SUBCLASS 0x0A

#define PCI_MGGC0 0x50
#define PCI_ASLS 0xFC

void lil_get_bar(void* device, int num, uintptr_t* base, uintptr_t* len);
