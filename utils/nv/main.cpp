#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <span>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <print>
#include <span>
#include <vector>

#include "spec.hpp"

namespace {

struct option longopts[] = {
	{"help", no_argument, 0, 'h'},
	{"all", no_argument, 0, 'a'},
	{0, 0, 0, 0},
};

template<typename T>
T *biosRead(std::span<std::byte> buf, size_t offset) {
	assert(offset + sizeof(T) <= buf.size_bytes());

	return reinterpret_cast<T *>(buf.subspan(offset, sizeof(T)).data());
}

bool is_efi_compressed(pci::rom::header *hdr, pci::rom::pcir *pcir) {
	if(pcir->code_type != 3)
		return false;

	auto efi_hdr = reinterpret_cast<pci::rom::efi_header *>(hdr);

	return efi_hdr->compression_type != 0;
}

std::pair<pci::rom::header *, size_t> findPciHeader(std::span<std::byte> buf) {
	auto ifr = biosRead<pci::ifr_header>(buf, 0);
	size_t ifr_size = 0;

	if(ifr->signature == 0x4947564E) {
		switch(ifr->ifr_version) {
			case 0x01:
			case 0x02:
				ifr_size = *biosRead<uint32_t>(buf, ifr->fixed_data_size + 4);
				break;
			case 0x03: {
				auto flash_status_offset = *biosRead<uint32_t>(buf, ifr->total_data_size);
				auto rom_directory_signature = *biosRead<uint32_t>(buf, flash_status_offset + 0x1000);
				if(rom_directory_signature == 0x44524652) {
					ifr_size = *biosRead<uint32_t>(buf, flash_status_offset + 0x1008);
				} else {
					assert(!"unknown ifr version");
				}
				break;
			}
			default:
				assert(!"unknown ifr version");
		}
	}

	auto hdr = biosRead<pci::rom::header>(buf, ifr_size);
	std::array<uint8_t, 2> pci_rom_signature = {{ 0x55, 0xAA }};
	auto signature = hdr->signature;
	std::println("PCI ROM offset={:#x} signature='{:02x} {:02x}'", ifr_size, signature[0], signature[1]);

	if(!memcmp(hdr->signature, pci_rom_signature.data(), 2)) {
		return {hdr, ifr_size};
	}

	return {nullptr, 0};
}

void parseFalcon(std::span<std::byte> image, std::span<std::byte> rom, std::span<std::byte> expansion_rom, bit::token *token, uint32_t ucode_table_offset) {
	std::println("Falcon version={} table_offset={:#x}", token->data_version, ucode_table_offset);

	auto hdr = biosRead<bit::falcon::header>(expansion_rom, ucode_table_offset);

	std::println("Falcon header version={:#x} header_size={:#x} entries={}", hdr->version, hdr->header_size, hdr->entry_count);

	for(size_t i = 0; i < hdr->entry_count; i++) {
		auto entry = biosRead<bit::falcon::entry>(expansion_rom, ucode_table_offset + hdr->header_size + (i * hdr->entry_size));
		if(entry->application_id != 0x05 && entry->application_id != 0x45 && entry->application_id != 0x85)
			continue;

		auto desc_hdr = biosRead<bit::falcon::desc_header>(expansion_rom, entry->desc_offset);
		assert(desc_hdr->version_available);

		auto desc_size = desc_hdr->size;

		if(desc_hdr->version == 2) {
			assert(desc_size >= 60);
		} else if(desc_hdr->version == 3) {
			assert(desc_size >= 44);
		} else {
			assert(!"unsupported Falcon ucode table version");
		}

		std::println("Falcon entry type={:#x} desc_version={} size={}",
			entry->application_id, desc_hdr->version, desc_size);
	}
}

void processImage(std::span<std::byte> image, std::span<std::byte> rom, std::span<std::byte>expansion_rom) {
	std::array<std::byte, 5> bit_signature = { {
		std::byte(0xFF), std::byte(0xB8), std::byte('B'), std::byte('I'), std::byte('T')
	}};

	auto res = std::ranges::search(rom, bit_signature);
	assert(!res.empty());
	auto bit_header_offset = std::distance(rom.begin(), res.begin());

	auto bit_header = biosRead<bit::header>(rom, bit_header_offset);

	auto entries = bit_header->token_entries;
	auto signature = bit_header->signature;
	std::println("BIT header signature={:x} entries={}", signature, entries);

	auto token = reinterpret_cast<bit::token *>(uintptr_t(bit_header) + sizeof(bit::header));
	for(size_t i = 0; i < entries; i++) {
		auto data_offset = token->data_offset;
		std::println("Token ID={:#x} ({:c}) version={} data_offset={:#x}", token->id, token->id, token->data_version, data_offset);

		switch(token->id) {
			case 'p': {
				assert(token->data_version == 2);
				assert(token->data_size >= 4);
				auto ucode_table_offset = *biosRead<uint32_t>(rom, token->data_offset);
				parseFalcon(image, rom, expansion_rom, token, ucode_table_offset);
				break;
			}
			default:
				break;
		}

		token = reinterpret_cast<bit::token *>(uintptr_t(token) + bit_header->token_size);
	}
}

std::pair<size_t, size_t> locateExpansionRoms(std::span<std::byte> image, size_t pci_offset) {
	uint32_t block_offset = 0;
	uint32_t block_size = 0;

	uint32_t curr_block = pci_offset;

	uint32_t ext_rom_offset = 0;
	uint32_t base_rom_size = 0;

	while(true) {
		auto src = image.subspan(curr_block);

		auto hdr = biosRead<pci::rom::header>(src, 0);
		src = image.subspan(curr_block + hdr->pcir_offset);

		auto pcir = biosRead<pci::rom::pcir>(src, 0);
		std::array<uint32_t, 3> accepted_signatures = {{
			0x52494350, // PCIR
			0x5344504E, // NPDS
			0x53494752, // RGIS
		}};
		assert(std::ranges::find(accepted_signatures, pcir->signature) != accepted_signatures.end());

		bool is_last_image = (pcir->indicator & (1 << 7)) != 0;
		uint32_t img_len = pcir->image_length;
		uint32_t sub_img_len = img_len;

		{
			uint16_t pcir_len = pcir->pcir_length;
			uint32_t pci_data_ext_at = (curr_block + hdr->pcir_offset + pcir_len + 0xF) & ~0xF;

			auto ext_src = image.subspan(pci_data_ext_at);
			auto npde = biosRead<pci::rom::npde>(ext_src, 0);

			if(npde->signature == 0x4544504E) {
				uint16_t pci_data_ext_rev = npde->npde_revision;

				if(pci_data_ext_rev == 0x100 || pci_data_ext_rev == 0x101) {
					uint16_t pci_data_ext_len = npde->npde_length;
					sub_img_len = npde->subimage_len;

					if(offsetof(pci::rom::npde, last) + sizeof(uint8_t) <= pci_data_ext_len) {
						is_last_image = (npde->last & (1 << 7)) != 0;
					} else if(sub_img_len < img_len) {
						is_last_image = false;
					}
				}
			}
		}

		uint8_t type = pcir->code_type;
		block_offset = curr_block - pci_offset;
		block_size = sub_img_len * 512U;

		if(ext_rom_offset == 0 && type == 0xE0) {
			ext_rom_offset = block_offset;
		} else if(base_rom_size == 0 && type == 0x00) {
			base_rom_size = block_size;
		}

		if(is_last_image)
			break;
		else
			curr_block = curr_block + sub_img_len * 512U;
	}

	auto bios_size = block_offset + block_size;
	auto expansion_rom_offset = 0;

	if(ext_rom_offset > 0 && base_rom_size > 0)
		expansion_rom_offset = ext_rom_offset - base_rom_size;

	return {bios_size, expansion_rom_offset};
}

}

int main(int argc, char *argv[]) {
	opterr = 0;

	std::vector<int> dump_block_ids = {};

	while(1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "ah", longopts, &option_index);

		if(c == -1) {
			break;
		}

		switch(c) {
			case 'h': {
				printf("help: nv [-a] [-h] input\n");
				exit(0);
			}
			case '?': {
				fprintf(stderr, "error: unknown option '-%c'\n", optopt);
				exit(1);
			}
			default: {
				fprintf(stderr, "error: getopt_long returned '%c'\n", c);
				break;
			}
		}
	}

	if((optind + 1) != argc) {
		fprintf(stderr, "no file given\n");
		exit(1);
	}

	const char *filepath = argv[optind];
	int fd = open(filepath, O_RDONLY);
	if(fd < 0) {
		fprintf(stderr, "could not open file '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}

	struct stat statbuf;
	int err = fstat(fd, &statbuf);
	assert(err >= 0);

	size_t rom_len = 0;
	void *rom_buf;

	if(statbuf.st_size) {
		rom_buf = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		assert(rom_buf != MAP_FAILED);
		close(fd);
		rom_len = statbuf.st_size;
	} else {
		int ret;
		size_t len = 0;
		size_t size = 0x2000;
		rom_buf = malloc(size);

		while((ret = read(fd, reinterpret_cast<void *>(uintptr_t(rom_buf) + len), size - len))) {
			if(ret < 0) {
				fprintf(stderr, "Failed to read \"%s\": %s\n", filepath, strerror(errno));
				return EXIT_FAILURE;
			}

			len += ret;
			if(len == size) {
				size *= 2;
				rom_buf = realloc(rom_buf, size);
			}
		}

		rom_len = len;
	}


	auto image = std::span<std::byte>{reinterpret_cast<std::byte *>(rom_buf), rom_len};
	auto [hdr, pci_rom_offset] = findPciHeader(image);
	auto [bios_size, expansion_rom_offset] = locateExpansionRoms(image, pci_rom_offset);

	std::println("BIOS size={:#x} Expansion ROM offset={:#x}", bios_size, expansion_rom_offset);

	auto expansion_rom = image.subspan(pci_rom_offset + expansion_rom_offset);

	while(true) {
		std::println("--------------------------------------------------------------------------------");
		uint8_t expected_rom_signature[2] = {0x55, 0xAA};
		if(memcmp(&hdr->signature, &expected_rom_signature, 2)) {
			std::println("unexpected PCI ROM header signature {:02x} {:02x}", hdr->signature[0], hdr->signature[1]);
			exit(1);
		}

		auto pcir = reinterpret_cast<pci::rom::pcir *>(uintptr_t(hdr) + hdr->pcir_offset);
		assert(pcir->signature == *reinterpret_cast<const uint32_t *>("PCIR"));

		if(pcir->indicator & (1 << 7))
			break;

		std::println("Image offset={:#x} CodeType={} length={:#x}",
			uintptr_t(hdr) - uintptr_t(rom_buf), pcir->code_type, pcir->image_length * 512);

		auto rom = std::span<std::byte>{reinterpret_cast<std::byte *>(hdr), rom_len - std::distance(reinterpret_cast<std::byte *>(rom_buf), reinterpret_cast<std::byte *>(hdr))};
		auto npde_offset = (hdr->pcir_offset + pcir->pcir_length + 0x0F) & ~0x0F;

		auto npde = biosRead<pci::rom::npde>(rom, npde_offset);
		assert(npde->signature == 0x4544504E);

		if(!is_efi_compressed(hdr, pcir))
			processImage(image, rom, expansion_rom);
		else
			std::println("Skipping compressed EFI section");

		hdr = reinterpret_cast<pci::rom::header *>(uintptr_t(hdr) + (pcir->image_length * 512));

		if(uintptr_t(hdr) - uintptr_t(rom_buf) >= rom_len) {
			break;
		}
	}

	return 0;
}
