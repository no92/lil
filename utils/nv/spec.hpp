#pragma once

#include <stdint.h>

namespace pci {

struct [[gnu::packed]] ifr_header {
	uint32_t signature;
	uint8_t unknown;
	uint8_t ifr_version;
	uint16_t fixed_data_size;
	uint32_t total_data_size: 20;
	uint32_t reserved: 12;
};

struct [[gnu::packed]] rfrd_header {
	uint32_t signature;
	uint32_t unknown;
	uint16_t rfrd_size;
};

namespace rom {

struct [[gnu::packed]] header {
	char signature[2];
	uint8_t reserved[0x16];
	uint16_t pcir_offset;
};

struct [[gnu::packed]] efi_header {
	char signature[2];
	// in units of 512 bytes
	uint16_t initialization_size;
	uint32_t efi_signature;
	uint16_t efi_subsystem;
	uint16_t efi_machine_type;
	uint16_t compression_type;
	uint8_t reserved[8];
	uint16_t efi_image_offset;
	uint16_t pcir_offset;
};

static_assert(sizeof(header) == sizeof(efi_header));

struct [[gnu::packed]] pcir {
	uint32_t signature;
	uint16_t vendor;
	uint16_t device;
	uint16_t reserved0;
	uint16_t pcir_length;
	uint8_t pcir_revision;
	uint8_t class_code[3];
	uint16_t image_length;
	uint16_t revision_level;
	uint8_t code_type;
	uint8_t indicator;
	uint16_t reserved1;
};

static_assert(sizeof(pcir) == 0x18);

struct [[gnu::packed]] npde {
	uint32_t signature;
	uint16_t npde_revision;
	uint16_t npde_length;
	uint16_t subimage_len;
	uint8_t last;
	uint8_t flags;
};

}

}

namespace bit {

struct [[gnu::packed]] header {
	uint16_t id;
	uint32_t signature;
	uint16_t bcd_version;
	uint8_t header_size;
	uint8_t token_size;
	uint8_t token_entries;
	uint8_t bit_header_checksum;
};

struct [[gnu::packed]] token {
	uint8_t id;
	uint8_t data_version;
	uint16_t data_size;
	uint16_t data_offset;
};

namespace falcon {

struct [[gnu::packed]] header {
	uint8_t version;
	uint8_t header_size;
	uint8_t entry_size;
	uint8_t entry_count;
	uint8_t desc_version;
	uint8_t desc_size;
};

struct [[gnu::packed]] entry {
	uint8_t application_id;
	uint8_t target_id;
	uint32_t desc_offset;
};

struct [[gnu::packed]] desc_header {
	uint8_t version_available: 1;
	uint8_t reserved0: 1;
	uint8_t encrypted: 1;
	uint8_t reserved1: 5;
	uint8_t version;
	uint16_t size;
};

static_assert(sizeof(desc_header) == 4);

}

}
