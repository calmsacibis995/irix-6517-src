/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_func.c	8.4 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <db.h>
#include "hash.h"
#include "page.h"
#include "extern.h"

extern u_int32_t db_hash1 __P((const void *, size_t));
extern u_int32_t db_hash2 __P((const void *, size_t));
extern u_int32_t db_hash3 __P((const void *, size_t));
extern u_int32_t db_hash4 __P((const void *, size_t));
extern u_int32_t db_hash5 __P((const void *, size_t));

/* Default hash function. */
/* Ariel: switched to hash5 (FNV hash).
 * hash4 distribution performance is poor on dictionary words and binary data
 */
u_int32_t (*__default_hash) __P((const void *, size_t)) = db_hash5;

/*
 * Assume that we've already split the bucket to which this key hashes,
 * calculate that bucket, and check that in fact we did already split it.
 *
 * EJB's original hsearch hash.
 */
#define PRIME1		37
#define PRIME2		1048583

u_int32_t
db_hash1(key, len)
	const void *key;
	size_t len;
{
	u_int32_t h;
	u_int8_t *k;

	h = 0;
	k = (u_int8_t *)key;
	/* Convert string to integer */
	while (len--)
		h = h * PRIME1 ^ (*k++ - ' ');
	h %= PRIME2;
	return (h);
}

/*
 * Phong Vo's linear congruential hash
 */
#define dcharhash(h, c)	((h) = 0x63c63cd9*(h) + 0x9c39c33d + (c))

u_int32_t
db_hash2(key, len)
	const void *key;
	size_t len;
{
	u_int32_t h;
	u_int8_t *e, c, *k;

	k = (u_int8_t *)key;
	e = k + len;
	for (h = 0; k != e;) {
		c = *k++;
		if (!c && k > e)
			break;
		dcharhash(h, c);
	}
	return (h);
}

/*
 * This is INCREDIBLY ugly, but fast.  We break the string up into 8 byte
 * units.  On the first time through the loop we get the "leftover bytes"
 * (strlen % 8).  On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch instructions.  If
 * this routine is heavily used enough, it's worth the ugly coding.
 *
 * Ozan Yigit's original sdbm hash.
 */
u_int32_t
db_hash3(key, len)
	const void *key;
	size_t len;
{
	u_int32_t n, loop;
	u_int8_t *k;

#define HASHC   n = *k++ + 65599 * n

	n = 0;
	k = (u_int8_t *)key;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASHC;
		case 7:
				HASHC;
		case 6:
				HASHC;
		case 5:
				HASHC;
		case 4:
				HASHC;
		case 3:
				HASHC;
		case 2:
				HASHC;
		case 1:
				HASHC;
			} while (--loop);
		}

	}
	return (n);
}

/* Chris Torek's hash function. */
u_int32_t
db_hash4(key, len)
	const void *key;
	size_t len;
{
	u_int32_t h, loop;
	u_int8_t *k;

#define HASH4a   h = (h << 5) - h + *k++;
#define HASH4b   h = (h << 5) + h + *k++;
#define HASH4 HASH4b

	h = 0;
	k = (u_int8_t *)key;
	if (len > 0) {
		loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASH4;
		case 7:
				HASH4;
		case 6:
				HASH4;
		case 5:
				HASH4;
		case 4:
				HASH4;
		case 3:
				HASH4;
		case 2:
				HASH4;
		case 1:
				HASH4;
			} while (--loop);
		}

	}
	return (h);
}

/*
 * Fowler/Noll/Vo hash
 *
 * The magic is in the interesting relationship between the special
 * prime 16777619 (2^24 + 403) and 2^32 and 2^8.  If you need a hash to
 * base 2^64 instead of 2^32, let me know and I'll search for a similar
 * magic prime.
 *
 * This hash was able to process 234936 words from the web2 dictionary
 * without any 32 bit collisions.  It also hashes 4 byte integers
 * really well (no collisions for the range 1..10000000)
 */
u_int32_t
db_hash5(const void * key, size_t len)
{
	u_int32_t	val;		/* current hash value */
	uchar_t		*k = (uchar_t *) key;
	uchar_t		*key_end = k + len;

	/*
	 * Fowler/Noll/Vo hash - hash each character in the string
	 *
	 * The basis of the hash algorithm was taken from an idea
	 * sent by Email to the IEEE Posix P1003.2 mailing list from
	 * Phong Vo (kpv@research.att.com) and Glenn Fowler
	 * (gsf@research.att.com).
	 * Landon Curt Noll (chongo@toad.com) later improved on their
	 * algorithm to come up with Fowler/Noll/Vo hash.
	 *
	 */
	for (val = 0; k < key_end; ++k) {
		val *= 16777619;
		val ^= *k;
	}

	/* our hash value, (was: mod the hash size) */
	return val;
}

