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
#include <utility>
#include <vector>

#include "spec.hpp"
#include "vbios.hpp"

using namespace vbios;

namespace {

struct option longopts[] = {
	{"help", no_argument, nullptr, 'h'},
	{"all", no_argument, nullptr, 'a'},
	{0, 0, 0, 0},
};

void parseFalcon(std::span<std::byte> expansion_rom, bit::token *token, uint32_t ucode_table_offset) {
	std::println("Falcon version={} table_offset={:#x}", token->data_version, ucode_table_offset);

	auto hdr = biosRead<bit::falcon::header>(expansion_rom, ucode_table_offset);

	std::println("Falcon header version={:#x} header_size={:#x} entries={}", hdr.version, hdr.header_size, hdr.entry_count);

	for(size_t i = 0; i < hdr.entry_count; i++) {
		auto entry = biosRead<bit::falcon::entry>(expansion_rom, ucode_table_offset + hdr.header_size + (i * hdr.entry_size));
		if(entry.application_id != 0x05 && entry.application_id != 0x45 && entry.application_id != 0x85)
			continue;

		auto desc_hdr = biosRead<bit::falcon::desc_header>(expansion_rom, entry.desc_offset);
		assert(desc_hdr.version_available);

		auto desc_size = desc_hdr.size;

		if(desc_hdr.version == 2) {
			assert(desc_size >= 60);
		} else if(desc_hdr.version == 3) {
			assert(desc_size >= 44);
		} else {
			assert(!"unsupported Falcon ucode table version");
		}

		std::println("Falcon entry type={:#x} desc_version={} size={}",
			entry.application_id, desc_hdr.version, desc_size);
	}
}

void processImage(std::span<std::byte> rom, std::span<std::byte>expansion_rom) {
	std::array<std::byte, 5> bit_signature = { {
		std::byte(0xFF), std::byte(0xB8), std::byte('B'), std::byte('I'), std::byte('T')
	}};

	auto res = std::ranges::search(rom, bit_signature);
	assert(!res.empty());
	auto bit_header_offset = std::distance(rom.begin(), res.begin());

	auto bit_header = biosRead<bit::header>(rom, bit_header_offset);

	auto entries = bit_header.token_entries;
	auto signature = bit_header.signature;
	std::println("BIT header signature={:x} entries={}", signature, entries);

	auto token = biosRead<bit::token>(rom, bit_header_offset + sizeof(bit::header));
	for(size_t i = 0; i < entries; i++) {
		auto data_offset = token.data_offset;
		std::println("Token ID={:#x} ({:c}) version={} data_offset={:#x}", token.id, token.id, token.data_version, data_offset);

		switch(token.id) {
			case 'p': {
				assert(token.data_version == 2);
				assert(token.data_size >= 4);
				auto ucode_table_offset = biosRead<uint32_t>(rom, token.data_offset);
				parseFalcon(expansion_rom, &token, ucode_table_offset);
				break;
			}
			default:
				break;
		}

		token = biosRead<bit::token>(rom, bit_header_offset + sizeof(bit::header) + (i * bit_header.token_size));
	}
}

std::pair<int, std::span<std::byte>> readFile(const char *filepath) {
	int fd = open(filepath, O_RDONLY);
	if(fd < 0) {
		fprintf(stderr, "could not open file '%s': %s\n", filepath, strerror(errno));
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
				return {-1, {}};
			}

			len += ret;
			if(len == size) {
				size *= 2;
				rom_buf = realloc(rom_buf, size);
			}
		}

		rom_len = len;
	}

	return {fd, std::span<std::byte>(reinterpret_cast<std::byte *>(rom_buf), rom_len)};
}

} // namespace

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
	auto [fd, image] = readFile(filepath);
	assert(fd != -1);

	auto pci_rom_offset = locatePciHeader(image);
	auto [bios_size, expansion_rom_offset] = locateExpansionRoms(image, pci_rom_offset);
	image = image.subspan(0, bios_size);

	std::println("BIOS total size {:#x} bytes, Expansion ROM offset {:#x}", bios_size, expansion_rom_offset);

	auto expansion_rom = image.subspan(pci_rom_offset + expansion_rom_offset);
	auto hdr = biosRead<pci::rom::header>(image, pci_rom_offset);
	auto pcir = biosRead<pci::rom::pcir>(image, pci_rom_offset + hdr.pcir_offset);
	assert(pcir.signature == pci::rom::PCIR_SIGNATURE);

	std::println("Image offset={:#x} CodeType={} length={:#x}",
		pci_rom_offset, std::to_underlying(pcir.code_type), pcir.image_length * 512);

	auto rom = image.subspan(pci_rom_offset, image.size_bytes() - pci_rom_offset);
	auto npde_offset = (hdr.pcir_offset + pcir.pcir_length + 0x0F) & ~0x0F;

	auto npde = biosRead<pci::rom::npde>(rom, npde_offset);
	assert(npde.signature == pci::rom::NPDE_SIGNATURE);

	assert(!is_efi_compressed(&hdr, &pcir));
	processImage(rom, expansion_rom);

	close(fd);

	return 0;
}
