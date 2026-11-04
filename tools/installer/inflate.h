#ifndef EXCCD_INFLATE_H
#define EXCCD_INFLATE_H
#include <stddef.h>
#include <stdint.h>

/* Decode a raw DEFLATE stream into out[0..outlen). Returns 0 only if the
 * stream is valid and produces exactly outlen bytes. */
int inflate_raw(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen);

#endif
