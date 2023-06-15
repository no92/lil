#pragma once

#include <stdint.h>

#define PCI_ASLS 0xFC

void lil_get_bar(void* device, int num, uintptr_t* base, uintptr_t* len);
