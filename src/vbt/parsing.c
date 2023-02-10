#include <stddef.h>
#include <stdint.h>

#include <lil/vbt.h>

int memcmp(const void *s1, const void *s2, size_t n);

const struct vbt_header *vbt_get_header(const void *vbt, size_t size) {
	for(size_t i = 0; i + 4 < size; i++) {
		const struct vbt_header *header = (vbt + i);

		if(memcmp(header->signature, VBT_HEADER_SIGNATURE, 4))
			continue;

		return header;
	}

	return NULL;
}

const struct bdb_header *vbt_get_bdb_header(const struct vbt_header *hdr) {
	struct bdb_header *bdb = (void *) ((uintptr_t) hdr + hdr->bdb_offset);

	if(memcmp(bdb->signature, BDB_HEADER_SIGNATURE, 16))
		return NULL;

	return bdb;
}

static size_t bdb_block_size(const void *hdr) {
	if(*(uint8_t *)hdr == BDB_MIPI_SEQUENCE && *(uint8_t *)(hdr + 3) >= 3)
		return *((const uint32_t *)(hdr + 4)) + 3;
	else
		return *((const uint16_t *)(hdr + 1)) + 3;
}

const struct bdb_block_header *vbt_get_bdb_block(const struct vbt_header *hdr, enum bdb_block_id id) {
	const struct bdb_header *bdb = vbt_get_bdb_header(hdr);
	const struct bdb_block_header *block = (const void *) ((uintptr_t) bdb + bdb->header_size);
	size_t index = 0;

	for(; index + 3 < bdb->bdb_size && index + bdb_block_size(block) <= bdb->bdb_size; block = (void *) ((uintptr_t) block + bdb_block_size(block))) {
		if(id == block->id)
			return block;
	}

	return NULL;
}
