/*
 * Taken from glibc:
 * https://www.gnu.org/software/libc/sources.html
 * resolv/inet_ntop.c
 *
 * Modified by Vacuumlabs (2021).
 */

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <string.h>

#include "assert.h"
#include "ipUtils.h"
#include "utils.h"

#define NS_IN6ADDRSZ 16 /*%< IPv6 T_AAAA */
#define NS_INT16SZ   2  /*%< #/bytes of data in a uint16_t */

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

/*
 *	format an IPv4 address
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no static
 *	(2) takes a uint8_t* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
void inet_ntop4(const uint8_t* src, char* dst, size_t dstSize) {
    ASSERT(dstSize >= MAX_IPV4_STR_LENGTH);

    static const char fmt[] = "%u.%u.%u.%u";

    snprintf(dst, dstSize, fmt, src[0], src[1], src[2], src[3]);

    ASSERT(strlen(dst) + 1 < dstSize);
}

/*
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
void inet_ntop6(const uint8_t* src, char* dst, size_t dstSize) {
    STATIC_ASSERT(sizeof(size_t) >= 4, "bad size_t size");
    STATIC_ASSERT(sizeof(unsigned int) >= 4, "bad unsigned int size");

    /*
     * Note that int32_t and int16_t need only be "at least" large enough
     * to contain a value of the specified size.  On some systems, like
     * Crays, there is no such thing as an integer variable with 16 bits.
     * Keep this in mind if you think this function should have been coded
     * to use pointer overlays.  All the world's not a VAX.
     */
    char tmp[MAX_IPV6_STR_LENGTH] = {0};
    struct {
        int base, len;
    } best, cur;
    unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];

    /*
     * Preprocess:
     *	Copy the input (bytewise) array into a wordwise array.
     *	Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    explicit_bzero(words, sizeof words);
    for (int i = 0; i < NS_IN6ADDRSZ; i += 2) {
        ASSERT((unsigned int) (i / 2) < SIZEOF(words));
        words[i / 2] = (src[i] << 8) | src[i + 1];
    }
    best.base = -1;
    cur.base = -1;
    best.len = 0;
    cur.len = 0;
    for (int i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
        ASSERT((unsigned int) i < SIZEOF(words));
        if (words[i] == 0) {
            if (cur.base == -1)
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        } else {
            if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len) best = cur;
                cur.base = -1;
            }
        }
    }
    if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len) best = cur;
    }
    if (best.base != -1 && best.len < 2) best.base = -1;

    /*
     * Format the result.
     */
    char* tp = tmp;
    for (int i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
        /* Are we inside the best run of 0x00's? */
        if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
            if (i == best.base) {
                ASSERT(tp < tmp + SIZEOF(tmp));
                *tp++ = ':';
            }
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if (i != 0) {
            ASSERT(tp < tmp + SIZEOF(tmp));
            *tp++ = ':';
        }
        /* Is this address an encapsulated IPv4? */
        if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
            inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp));
            tp += strlen(tp);
            break;
        }
        STATIC_ASSERT(sizeof(words[i]) <= sizeof(unsigned), "oversized type for %u");
        snprintf(tp, sizeof tmp - (tp - tmp), "%x", words[i]);

        tp += strlen(tp);
    }
    /* Was it a trailing run of 0x00's? */
    if (best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ)) {
        ASSERT(tp < tmp + SIZEOF(tmp));
        *tp++ = ':';
    }
    ASSERT(tp < tmp + SIZEOF(tmp));
    *tp++ = '\0';

    ASSERT(strlen(tmp) + 1 < dstSize);

    strncpy(dst, tmp, dstSize);
}
