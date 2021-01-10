#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct {
	u32	type;
	u32	size;
	u8	data[0];
} KTSR_ENTRY;

typedef struct {
	u32	magic;
	u32	type;
	u16	version;
	u8	zero1;
	u8	platform;
	u32	game_id;
	u64	zero2;
	u32	file_size;
	u32	file_size2;
	u8	reserved[32];
	u8	data[0];
} KTSR;

typedef struct {
	u32	magic;
	u32	zero1;
	u32	file_size;
	u32	zero2;

	KTSR	audio;
} SRST;

const char* get_platform(int platform)
{
	switch(platform) {
		case 0x01:
			return "PC";
		case 0x03:
			return "PS Vita";
		case 0x04:
			return "Switch";
		default:
			return "unknown";
	}
}

int main(int argc, char** argv)
{
	char magic[4];

	if(argc != 2 && argc != 3) {
		printf("Usage: %s in.file [outprefix]\n"
			"\n"
			"Example: %s romfs/asset/data/0x272c6efb.file "
					"music/0x272c6efb\n", *argv, *argv);
		return 1;
	}

	FILE* in = fopen(argv[1], "rb");
	fseek(in, 0, SEEK_END);
	size_t size = ftell(in);
	fseek(in, 0x38, SEEK_SET);

	if(size <= 0x38) {
		fclose(in);
		printf("Error: file too small\n");
		return 0;
	}

	fread(magic, 4, 1, in);
	if(memcmp("TSRS", magic, 4)) {
		fclose(in);
		printf("Error: not a SRST file!\n");
		return 0;
	}

	fseek(in, 0x38, SEEK_SET);
	size_t data_size = size - 0x38;
	char* buf = (char*) malloc(data_size);
	fread(buf, data_size, 1, in);
	fclose(in);

	SRST* srst = (SRST*) buf;

	if(memcmp("KTSR", &srst->audio.magic, 4)) {
		printf("Error: SRST doesn't contain KTSR chunk!\n");
		return 0;
	}

	printf("platform: %u [%s]\n", srst->audio.platform,
			get_platform(srst->audio.platform));
	printf("SRST size: %lu bytes\n", srst->file_size);

	u32 offset = 0;
	u32 end = srst->audio.file_size - offsetof(KTSR, data);
	unsigned int i = 0;
	while(offset < end) {
		char filename[256];

		KTSR_ENTRY* entry = (KTSR_ENTRY*) &srst->audio.data[offset];
		offset += entry->size;

		/* KTSS starts at offset 56 in the data area, not sure what the
		 * "header" is about */
		void* ktss = entry->data + 56;
		if(memcmp("KTSS", ktss, 4)) {
			printf("no KTSS found in entry %u\n", i);
			return 0;
		}

		/* extract audio file if destination prefix is given */
		if(argc == 3) {
			size_t entry_size = entry->size - 56 - 8;
			sprintf(filename, "%s.%u.kns", argv[2], i);
			printf("writing [%03u] to \"%s\" (%u bytes)...\n", i,
					filename, entry_size);
			FILE* out = fopen(filename, "wb");
			fwrite(ktss, entry_size, 1, out);
			fclose(out);
		} else {
			printf("entry %u type: %08X, size: %ld [0x%x]\n", i,
					entry->type, entry->size, offset);
		}
		i++;
	}

	free(buf);

	return 0;
}
