#include "slip.h"

#include <asm-generic/errno.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define ASSERT(x) assert((x), __FILE__, __LINE__, #x)

enum {
	SLIPDEC_START_STATE,
	SLIPDEC_IN_FRAME_STATE,
	SLIPDEC_ESC_STATE
};

static void slip_decode_start(struct slip *slip, uint8_t rx_byte);
static void slip_decode_inframe(struct slip *slip, uint8_t rx_byte);
static void slip_decode_escseq(struct slip *slip, uint8_t rx_byte);

static inline void assert(int cond,
	const char *file, int line, const char *condstr)
{
	if (cond) return;

	fprintf(stderr, "libslip: Assertion failed at %s:%i (%s)\n",
		file, line, condstr);
	abort();
}

static inline void
store(uint8_t **cur, uint8_t *end, uint8_t byte)
{
	if (*cur < end) *(*cur)++ = byte;
}

void
slip_decode_start(struct slip *slip, uint8_t rx_byte)
{
	/* wait for packet start */
	if (rx_byte == slip->start) {
		slip->rx_index = 0;
		slip->rx_state = SLIPDEC_IN_FRAME_STATE;
	}
}

void
slip_decode_inframe(struct slip *slip, uint8_t rx_byte)
{
	if (rx_byte == slip->end) {
		/* end of packet */
		slip->rx_packet(slip, slip->rx_packet_userdata,
			slip->rx_buf, slip->rx_index);
		slip->rx_state = SLIPDEC_START_STATE;
	} else if (rx_byte == slip->start) {
		/* malformed packet */
		if (slip->rx_restart)
			slip->rx_restart(slip, rx_byte);
		slip->rx_index = 0;
		slip->rx_state = SLIPDEC_START_STATE;
	} else if (rx_byte == slip->esc) {
		/* escape sequence */
		slip->rx_state = SLIPDEC_ESC_STATE;
	} else {
		/* data byte */
		slip_decode_store(slip, rx_byte);
	}
}

void
slip_decode_escseq(struct slip *slip, uint8_t rx_byte)
{
	uint8_t dec;

	dec = slip->esc_dec[rx_byte];
	if (!slip->esc_active[dec]) {
		/* invalid contents, restart */
		if (slip->rx_restart)
			slip->rx_restart(slip, rx_byte);
		slip->rx_state = SLIPDEC_START_STATE;
		slip->rx_index = 0;
	} else {
		slip_decode_store(slip, dec);
		slip->rx_state = SLIPDEC_IN_FRAME_STATE;
	}
}

int
slip_init(struct slip *slip)
{
	int i;

	slip->rx_state = SLIPDEC_START_STATE;

	slip->rx_index = 0;
	slip->rx_buflen = 0;
	slip->rx_buf = NULL;

	ASSERT(slip->rx_packet != NULL);
	ASSERT(slip->realloc != NULL);

	ASSERT(slip->esc != slip->start);
	ASSERT(slip->esc != slip->end);

	/* build decode table */
	for (i = 0; i < 256; i++)
		slip->esc_dec[slip->esc_enc[i]] = i;

	return 0;
}

void
slip_deinit(struct slip *slip)
{
	free(slip->rx_buf);
}

void
slip_decode_store(struct slip *slip, uint8_t byte)
{
	if (slip->rx_index >= slip->rx_buflen) {
		if (!slip->rx_buflen)
			slip->rx_buflen = 256;
		else
			slip->rx_buflen *= 2;
		slip->rx_buf = slip->realloc(slip->rx_buf, slip->rx_buflen);
		ASSERT(slip->rx_buf != NULL);
	}
	slip->rx_buf[slip->rx_index++] = byte;
}

void
slip_decode(struct slip *slip, uint8_t* data, size_t size)
{
	uint8_t rx_byte;
	size_t i;

	for (i = 0; i < size; i++) {
		rx_byte = data[i];
		switch (slip->rx_state) {
		case SLIPDEC_START_STATE:
			slip_decode_start(slip, rx_byte);
			break;
		case SLIPDEC_IN_FRAME_STATE:
			slip_decode_inframe(slip, rx_byte);
			break;
		case SLIPDEC_ESC_STATE:
			slip_decode_escseq(slip, rx_byte);
			break;
		default:
			abort();
		}
	}
}

int
slip_encode(struct slip *slip, uint8_t* out, size_t *outlen, size_t outcap,
	uint8_t* in, size_t inlen)
{
	uint8_t *pos, *end;
	size_t i;

	pos = out;
	end = out + outcap;

	store(&pos, end, slip->start);

	for (i = 0; i < inlen && pos < end; i++) {
		if (slip->esc_active[in[i]]) {
			store(&pos, end, slip->esc);
			store(&pos, end, slip->esc_enc[in[i]]);
		} else {
			store(&pos, end, in[i]);
		}
	}

	store(&pos, end, slip->end);

	if (pos >= end) return ENOBUFS;

	*outlen = pos - out;

	return 0;
}
