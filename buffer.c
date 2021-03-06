/*
 * buffer.c:
 * Circular buffers.
 *
 * Copyright (c) 2002 Chris Lightfoot.
 * Email: chris@ex-parrot.com; WWW: http://www.ex-parrot.com/~chris/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

static const char rcsid[] = "$Id$";

#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "util.h"

/* buffer_new LEN
 * Create a new buffer initially holding LEN bytes. */
buffer buffer_new(const size_t len) {
    buffer B;
    alloc_struct(_buffer, B);
    B->buf = xmalloc(B->len = len);
    return B;
}

/* buffer_delete BUFFER
 * Destroy BUFFER, deallocating memory used. */
void buffer_delete(buffer B) {
    assert(B);
    xfree(B->buf);
    xfree(B);
}

/* buffer_make_contiguous BUFFER
 * Makes the available data in BUFFER contiguous, so that it can be returned
 * by a single call to buffer_get_consume_ptr. */
void buffer_make_contiguous(buffer B) {
    size_t a;
    char *newbuf;

    assert(B);
    
    a = buffer_available(B);
    if (!a || (size_t)B->get + a <= B->len)
        /* Nothing to do. */
        return;
    
    /*
     *           v (get + a) % len
     * ,---------------------------------------------.
     * |XXXXXXXXXXXXXXXXX|               |XXXXXXXXXXX|
     * | (get + a) % len |               | len - get |
     * |XXXXXXXXXXXXXXXXX|               |XXXXXXXXXXX|
     * `---------------------------------------------'
     * ^ 0                               ^ get
     */
    newbuf = xmalloc(B->len);
    memcpy(newbuf, B->buf + B->get, B->len - B->get);
    memcpy(newbuf + B->len - B->get, B->buf, (B->get + a) % B->len);
    xfree(B->buf);
    B->buf = newbuf;
    B->get = 0;
    B->put = a;
}

/* buffer_get_consume_ptr BUFFER SLEN
 * Consume some available data in BUFFER, returning a pointer to the data and
 * recording the number of bytes consumed in SLEN. This may be less than the
 * total number of bytes available, in which case a further call to
 * buffer_consume will yield the remainder. The pointer returned may be
 * invalidated by a call to buffer_expand or buffer_push_data. The data
 * returned are not null-terminated. If no data are available, NULL is
 * returned. */
char *buffer_get_consume_ptr(buffer B, size_t *slen) {
    size_t a;
    char *p;
    assert(B);
    if (!(a = buffer_available(B))) {
        *slen = 0;
        return NULL;
    }
    if ((size_t)B->get + a > B->len)
        a = B->len - B->get;
    p = B->buf + B->get;
    *slen = a;
    return p;
}

/* buffer_consume_bytes BUFFER NUM
 * Indicate that NUM bytes have been consumed from a location in BUFFER
 * returned by buffer_get_consume_ptr. */
void buffer_consume_bytes(buffer B, const size_t num) {
    assert(B);
    B->get = (B->get + num) % B->len;
}

/* buffer_consume_all BUFFER STR SLEN
 * Consume all available data in BUFFER, recording the number of bytes
 * consumed in SLEN. If STR is not NULL it must point to a buffer of length at
 * least *SLEN allocated with malloc(3); this buffer will be used as is if the
 * returned string is small enough, or reallocated with realloc(3) otherwise.
 * The returned string is null-terminated. Returns NULL if no data are
 * available. */
char *buffer_consume_all(buffer B, char *str, size_t *slen) {
    size_t a, i;
    assert(B);
    if (!(a = buffer_available(B))) {
        *slen = 0;
        return NULL;
    }
    if (!str || *slen < a + 1)
        str = xrealloc(str, a + 1);
    *slen = a + 1;
    for (i = 0; i < a; ++i)
        str[i] = B->buf[(B->get + i) % B->len];
    str[i] = 0;
    B->get = B->put;
    return str;
}

/* buffer_consume_to_mark BUFFER MARK STR SLEN
 * Consume data from BUFFER up to and including single character MARK, returning
 * a pointer to a string allocated with malloc(3) or NULL if the mark was not
 * found. The number of bytes consumed is recorded in SLEN. If STR is not
 * NULL, it must point to a buffer of length at least *SLEN allocated with
 * malloc(3); this buffer will be used as is if the returned string is small
 * enough, or reallocated with realloc(3) otherwise. The returned string is
 * null-terminated. */
char *buffer_consume_to_mark(buffer B, const char *mark, char *str, size_t *slen) {
    size_t a;
    int k;

    assert(B);
    
    if ((a = buffer_available(B)) < 1) return NULL;

    assert(a <= (size_t)INT_MAX);

    for (k = 0; k < (int)a; k++) {
        if (B->buf[(B->get + k) % B->len] == mark[0]) {
            int j, len;
            /* Have found the mark at location k. */
            len = k + 1; /* string length */
            if (!str || *slen < (size_t)len + 1)
                str = xrealloc(str, (size_t)len + 1);
            *slen = (size_t)len + 1;
            for (j = 0; j < len; j++)
                str[j] = B->buf[(B->get + j) % B->len];
            str[j] = 0;
            B->get = (B->get + len) % B->len;
            return str;
        }
    }

    return NULL;
}

/* buffer_expand BUFFER NUM
 * Ensure that BUFFER can store at least an additional NUM bytes. */
void buffer_expand(buffer B, const size_t num) {
    size_t a;
    assert(B);
    if (B->len <= (a = buffer_available(B)) + num) { /* NB <= NOT < */
        size_t i;
        char *newbuf;
        size_t newlen;
        for (newlen = B->len * 2; newlen <= a + num; newlen *= 2);
        newbuf = xmalloc(newlen);
        for (i = 0; i < a; ++i)
            newbuf[i] = B->buf[(B->get + i) % B->len];
        xfree(B->buf);
        B->buf = newbuf;
        B->len = newlen;
        B->get = 0;
        B->put = (off_t)a;
    }
}

/* buffer_push_data BUFFER DATA DLEN
 * Add DLEN bytes of DATA to BUFFER, expanding the buffer if necessary. */
void buffer_push_data(buffer B, const char *data, const size_t dlen) {
    size_t i;

    assert(B);

    buffer_expand(B, dlen);

    for (i = 0; i < dlen; ++i)
        B->buf[(B->put + i) % B->len] = data[i];

    B->put = (B->put + dlen) % B->len;
}

/* buffer_get_push_ptr BUFFER LEN
 * Return a pointer to part of BUFFER to which new data can be written. On
 * return, LEN indicates how many contiguous bytes may be written. */
char *buffer_get_push_ptr(buffer B, size_t *len) {
    assert(B);
    *len = B->len - B->put;
    return B->buf + B->put;
}

/* buffer_push_bytes BUFFER NUM
 * Indicate that NUM bytes have been added to the buffer by writing them at
 * a location returned by buffer_get_push_ptr. */
void buffer_push_bytes(buffer B, const size_t num) {
    B->put = (B->put + num) % B->len;
}
