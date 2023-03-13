#include "slip.h"

#include <errno.h>

enum {
	SLIPDEC_START_STATE,
	SLIPDEC_IN_FRAME_STATE,
	SLIPDEC_ESC_STATE
};

static int slip_decode_start(struct slip *slip, uint8_t rx_byte);
static int slip_decode_inframe(struct slip *slip, uint8_t rx_byte);
static int slip_decode_escseq(struct slip *slip, uint8_t rx_byte);

static inline void
store(uint8_t **cur, uint8_t *end, uint8_t byte)
{
	if (*cur < end) *(*cur)++ = byte;
}

int
slip_decode_start(struct slip *slip, uint8_t rx_byte)
{
	/* wait for packet start */
	if (rx_byte == slip->start) {
		slip->rx_index = 0;
		slip->rx_state = SLIPDEC_IN_FRAME_STATE;
	}

	return 0;
}

int
slip_decode_inframe(struct slip *slip, uint8_t rx_byte)
{
	int rc;

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
		rc = slip_decode_store(slip, rx_byte);
		if (rc) return rc;
	}

	return 0;
}

int
slip_decode_escseq(struct slip *slip, uint8_t rx_byte)
{
	uint8_t dec;
	int rc;

	dec = slip->esc_dec[rx_byte];
	if (!slip->esc_active[dec]) {
		/* invalid contents, restart */
		if (slip->rx_restart)
			slip->rx_restart(slip, rx_byte);
		slip->rx_state = SLIPDEC_START_STATE;
		slip->rx_index = 0;
	} else {
		rc = slip_decode_store(slip, dec);
		if (rc) return rc;
		slip->rx_state = SLIPDEC_IN_FRAME_STATE;
	}

	return 0;
}

int
slip_init(struct slip *slip)
{
	int i;

	slip->rx_state = SLIPDEC_START_STATE;

	slip->rx_index = 0;
	slip->rx_buflen = 0;
	slip->rx_buf = NULL;

	if (slip->rx_packet) return -EINVAL;
	if (slip->realloc) return -EINVAL;
	if (slip->esc != slip->start) return -EINVAL;
	if (slip->esc != slip->end) return -EINVAL;

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

int
slip_decode_store(struct slip *slip, uint8_t byte)
{
	int rc;

	if (slip->rx_index >= slip->rx_buflen) {
		if (!slip->rx_buflen)
			slip->rx_buflen = 256;
		else
			slip->rx_buflen *= 2;
		rc = slip->realloc((void **)&slip->rx_buf, slip->rx_buflen);
		if (rc) return rc;
	}
	slip->rx_buf[slip->rx_index++] = byte;

	return 0;
}

int
slip_decode(struct slip *slip, uint8_t* data, size_t size)
{
	uint8_t rx_byte;
	size_t i;
	int rc;

	for (i = 0; i < size; i++) {
		rx_byte = data[i];
		switch (slip->rx_state) {
		case SLIPDEC_START_STATE:
			rc = slip_decode_start(slip, rx_byte);
			break;
		case SLIPDEC_IN_FRAME_STATE:
			rc = slip_decode_inframe(slip, rx_byte);
			break;
		case SLIPDEC_ESC_STATE:
			rc = slip_decode_escseq(slip, rx_byte);
			break;
		default:
			rc = -EINVAL;
			break;
		}
		if (rc) return rc;
	}

	return 0;
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

	if (pos >= end) return -ENOBUFS;

	*outlen = pos - out;

	return 0;
}
