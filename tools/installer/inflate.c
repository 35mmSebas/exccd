/* Raw DEFLATE decoder (RFC 1951) for the EXCCD installer.
 *
 * Small and slow on purpose: the files in an EXCCD ZIP are at most a few
 * hundred KB, so a bit-by-bit canonical Huffman decoder is fast enough and
 * keeps the installer free of external libraries. The structure follows the
 * well-known "puff" approach (canonical codes decoded one bit at a time).
 *
 * The whole output size is known in advance (from the ZIP directory), so we
 * decode into a fixed buffer and treat any overrun as an error.
 */
#include <setjmp.h>
#include <string.h>
#include "inflate.h"

#define MAXBITS 15
#define MAXLCODES 286
#define MAXDCODES 30
#define FIXLCODES 288

typedef struct {
    const uint8_t *in;
    size_t inlen, inpos;
    uint32_t bitbuf;
    int bitcnt;
    uint8_t *out;
    size_t outlen, outpos;
    jmp_buf env;
} state;

typedef struct {
    short count[MAXBITS + 1];
    short symbol[FIXLCODES];
} huffman;

static int bits(state *s, int need)
{
    uint32_t val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen)
            longjmp(s->env, 1);             /* input ended too early */
        val |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= need;
    return (int)(val & ((1UL << need) - 1));
}

static int stored(state *s)
{
    size_t len;
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->inpos + 4 > s->inlen)
        return -1;
    len = s->in[s->inpos] | (s->in[s->inpos + 1] << 8);
    if ((s->in[s->inpos + 2] ^ 0xff) != (len & 0xff) ||
        (s->in[s->inpos + 3] ^ 0xff) != (len >> 8))
        return -1;                          /* LEN and NLEN don't match */
    s->inpos += 4;
    if (s->inpos + len > s->inlen || s->outpos + len > s->outlen)
        return -1;
    memcpy(s->out + s->outpos, s->in + s->inpos, len);
    s->inpos += len;
    s->outpos += len;
    return 0;
}

static int decode(state *s, const huffman *h)
{
    int code = 0, first = 0, index = 0, len, count;
    for (len = 1; len <= MAXBITS; len++) {
        code |= bits(s, 1);
        count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;                              /* ran out of codes */
}

/* Returns 0 for a complete code, >0 for an incomplete one, <0 if the
 * lengths are over-subscribed. */
static int construct(huffman *h, const short *length, int n)
{
    int symbol, len, left;
    short offs[MAXBITS + 1];

    for (len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        h->count[length[symbol]]++;
    if (h->count[0] == n)
        return 0;
    left = 1;
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return left;
    }
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];
    for (symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = (short)symbol;
    return left;
}

static int codes(state *s, const huffman *lencode, const huffman *distcode)
{
    static const short lbase[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dbase[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
    static const short dext[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    int symbol;
    size_t len, dist;

    for (;;) {
        symbol = decode(s, lencode);
        if (symbol < 0)
            return -1;
        if (symbol < 256) {
            if (s->outpos >= s->outlen)
                return -1;
            s->out[s->outpos++] = (uint8_t)symbol;
        } else if (symbol == 256) {
            return 0;
        } else {
            symbol -= 257;
            if (symbol >= 29)
                return -1;
            len = lbase[symbol] + bits(s, lext[symbol]);
            symbol = decode(s, distcode);
            if (symbol < 0 || symbol >= 30)
                return -1;
            dist = dbase[symbol] + bits(s, dext[symbol]);
            if (dist > s->outpos || s->outpos + len > s->outlen)
                return -1;
            while (len--) {
                s->out[s->outpos] = s->out[s->outpos - dist];
                s->outpos++;
            }
        }
    }
}

static int fixed(state *s)
{
    static int built = 0;
    static huffman lencode, distcode;
    if (!built) {
        short lengths[FIXLCODES];
        int i;
        for (i = 0; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < FIXLCODES; i++) lengths[i] = 8;
        construct(&lencode, lengths, FIXLCODES);
        for (i = 0; i < MAXDCODES; i++) lengths[i] = 5;
        construct(&distcode, lengths, MAXDCODES);
        built = 1;
    }
    return codes(s, &lencode, &distcode);
}

static int dynamic(state *s)
{
    static const short order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int nlen, ndist, ncode, index, err;
    short lengths[MAXLCODES + MAXDCODES];
    huffman lencode, distcode;

    nlen = bits(s, 5) + 257;
    ndist = bits(s, 5) + 1;
    ncode = bits(s, 4) + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES)
        return -1;

    for (index = 0; index < ncode; index++)
        lengths[order[index]] = (short)bits(s, 3);
    for (; index < 19; index++)
        lengths[order[index]] = 0;
    if (construct(&lencode, lengths, 19) != 0)
        return -1;                          /* code-length code must be complete */

    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, &lencode), len, rep;
        if (symbol < 0)
            return -1;
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
            continue;
        }
        len = 0;
        if (symbol == 16) {
            if (index == 0)
                return -1;
            len = lengths[index - 1];
            rep = 3 + bits(s, 2);
        } else if (symbol == 17) {
            rep = 3 + bits(s, 3);
        } else {
            rep = 11 + bits(s, 7);
        }
        if (index + rep > nlen + ndist)
            return -1;
        while (rep--)
            lengths[index++] = (short)len;
    }
    if (lengths[256] == 0)
        return -1;                          /* no end-of-block code */

    err = construct(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        return -1;
    err = construct(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        return -1;
    return codes(s, &lencode, &distcode);
}

int inflate_raw(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    state s;
    int last, type, err;

    s.in = in;
    s.inlen = inlen;
    s.inpos = 0;
    s.bitbuf = 0;
    s.bitcnt = 0;
    s.out = out;
    s.outlen = outlen;
    s.outpos = 0;

    if (setjmp(s.env) != 0)
        return -1;
    do {
        last = bits(&s, 1);
        type = bits(&s, 2);
        if (type == 0)
            err = stored(&s);
        else if (type == 1)
            err = fixed(&s);
        else if (type == 2)
            err = dynamic(&s);
        else
            err = -1;
        if (err != 0)
            return -1;
    } while (!last);
    return s.outpos == outlen ? 0 : -1;
}
