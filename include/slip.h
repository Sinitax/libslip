#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct slip {
	int rx_state;

	size_t rx_index;
	size_t rx_buflen;
	uint8_t *rx_buf;

	void (*rx_restart)(struct slip *slip, uint8_t err_byte);
	void (*rx_packet)(struct slip *slip, void *user,
		uint8_t *data, size_t size);
	void *rx_packet_userdata;

	int (*realloc)(void **buffer, size_t size);

	uint8_t esc, start, end;
	bool esc_active[256];
	uint8_t esc_enc[256];
	uint8_t esc_dec[256];
};

int slip_init(struct slip *slip);
void slip_deinit(struct slip *slip);

int slip_decode(struct slip *slip, uint8_t *data, size_t size);
int slip_decode_store(struct slip *slip, uint8_t byte);

int slip_encode(struct slip *slip, uint8_t *out, size_t *outlen,
	size_t outcap, uint8_t *in, size_t inlen);
