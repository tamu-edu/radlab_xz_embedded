/* SPDX-License-Identifier: 0BSD */
/* Test caller ownership, invalid storage, independent decoders, and reuse. */
#include <stdio.h>
#include <string.h>
#include "xz.h"

#define DICTIONARY_BYTES (8U * 1024 * 1024)
#define STATE_BYTES (32U * 1024)
#define INPUT_BYTES (2U * 1024 * 1024)

static _Alignas(max_align_t) unsigned char first[DICTIONARY_BYTES + STATE_BYTES];
static _Alignas(max_align_t) unsigned char second[DICTIONARY_BYTES + STATE_BYTES];
static unsigned char input[INPUT_BYTES];
static unsigned char expected[INPUT_BYTES];

/* Read bounded test data; an oversized fixture is a test failure. */
static size_t read_fixture(const char *path, unsigned char *bytes)
{
	FILE *file = fopen(path, "rb");
	size_t count;
	if (file == NULL)
		return 0;
	count = fread(bytes, 1, INPUT_BYTES, file);
	if (ferror(file) || !feof(file))
		count = 0;
	fclose(file);
	return count;
}

int main(int argc, char **argv)
{
	struct xz_dec *decoders[2];
	size_t required = xz_dec_storage_size(DICTIONARY_BYTES);
	size_t input_size;
	size_t expected_size;
	unsigned round;
	if (argc != 3 || required == 0 || required > sizeof(first))
		return 100;
	memset(first, 0xA5, sizeof(first));
	if (xz_dec_storage_size(0) != 0
			|| xz_dec_init_preallocated(NULL, sizeof(first), DICTIONARY_BYTES)
			|| xz_dec_init_preallocated(first + 1, sizeof(first) - 1, DICTIONARY_BYTES)
			|| xz_dec_init_preallocated(first, required - 1, DICTIONARY_BYTES)
			|| xz_dec_init_preallocated(first, sizeof(first), 0)
			|| first[0] != 0xA5 || first[sizeof(first) - 1] != 0xA5)
		return 101;
	xz_dec_end(NULL);
	xz_crc32_init();
	xz_crc64_init();
	decoders[0] = xz_dec_init_preallocated(first, required, DICTIONARY_BYTES);
	decoders[1] = xz_dec_init_preallocated(second, sizeof(second), DICTIONARY_BYTES);
	if (decoders[0] == NULL || decoders[1] == NULL || decoders[0] == decoders[1])
		return 102;
	input_size = read_fixture(argv[1], input);
	expected_size = read_fixture(argv[2], expected);
	if (input_size == 0 || expected_size == 0)
		return 103;
	for (round = 0; round < 8; ++round) {
		struct xz_buf buffers[2] = {{0}, {0}};
		size_t offsets[2] = {0, 0};
		int done[2] = {0, 0};
		unsigned loops = 0;
		unsigned index;
		for (index = 0; index < 2; ++index) {
			xz_dec_reset(decoders[index]);
			buffers[index].in = input;
			buffers[index].in_size = input_size;
		}
		while (!done[0] || !done[1]) {
			if (++loops > expected_size + input_size + 100)
				return 104;
			for (index = 0; index < 2; ++index) {
				unsigned char output[257];
				enum xz_ret result;
				struct xz_buf *b = &buffers[index];
				if (done[index])
					continue;
				b->out = output;
				b->out_pos = 0;
				b->out_size = index ? sizeof(output) : 17;
				result = xz_dec_catrun(decoders[index], b, 1);
				if (result != XZ_OK && result != XZ_STREAM_END)
					return (int)result;
				if (b->out_pos > expected_size - offsets[index]
						|| memcmp(output, expected + offsets[index], b->out_pos))
					return 105;
				offsets[index] += b->out_pos;
				if (result == XZ_STREAM_END) {
					if (offsets[index] != expected_size || b->in_pos != input_size)
						return 106;
					done[index] = 1;
				}
			}
		}
	}
	xz_dec_end(decoders[0]);
	xz_dec_end(decoders[1]);
	/* Caller storage survives end() and can initialize another decoder. */
	if (xz_dec_init_preallocated(first, sizeof(first), DICTIONARY_BYTES) == NULL)
		return 107;
	return 0;
}
