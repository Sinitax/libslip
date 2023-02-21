#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef LIBSLIP_ASSERT_ENABLE

#include <stdio.h>

#define LIBSLIP_ASSERT(x) libslip_assert((x), __FILE__, __LINE__, #x)

static inline void libslip_assert(int cond,
	const char *file, int line, const char *condstr)
{
	if (cond) return;

	fprintf(stderr, "libslip: Assertion failed at %s:%i (%s)\n",
		file, line, condstr);
	abort();
}

#else
#define LIBSLIP_ASSERT(x)
#endif

struct slip {
	int rx_state;

	size_t rx_index;
	size_t rx_buflen;
	uint8_t *rx_buf;

	void (*rx_restart)(struct slip *slip, uint8_t err_byte);
	void (*rx_packet)(struct slip *slip, void *user,
		uint8_t *data, size_t size);
	void *rx_packet_userdata;

	void *(*realloc)(void *buffer, size_t size);

	uint8_t esc, start, end;
	bool esc_active[256];
	uint8_t esc_enc[256];
	uint8_t esc_dec[256];
};

int slip_init(struct slip *slip);
void slip_deinit(struct slip *slip);

void slip_decode(struct slip *slip, uint8_t *data, size_t size);
void slip_decode_store(struct slip *slip, uint8_t byte);

int slip_encode(struct slip *slip, uint8_t *out, size_t *outlen, size_t outcap,
	uint8_t *in, size_t inlen);
