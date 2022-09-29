/*
 * The str5pack routine converts a nul-terminated string
 * into a single 32 bit word that encodes the first 5
 * ASCII characters.  The conversion is many to one,
 * but it preserves the ASCII sorting order.  That is,
 * if string A produces encoding e(A), and string B produces
 * encoding e(B), and if e(A) < e(B), then it must have
 * been the case that string A < string B.
 *
 * The encoding borrows its idea from another encoding,
 * of 3 characters into a 16 bit word, that is part of "my
 * oral tradition".  That encoding collapses the 256 possible
 * character values into 40 values in the range {0..39},
 * and encodes 3 characters using base-40 representation.
 * It works because 40**3 is less than 2**16 (64K).
 *
 * The present encoding, reflecting the steady march of
 * computer technology to larger word sizes, maps the
 * 256 possible characters to the range {0..83}, and
 * encodes 5 such collapsed values using base-84 representation.
 * This one works because 84**5 < 2**32.  Having 84 values
 * allows most printable ASCII characters to be mapped
 * one-to-one, including digits and upper and lower letters.
 */

#include <string.h>
#include "fsdump.h"

#define BASE 84
static unsigned scale[STR5LEN] = {BASE*BASE*BASE*BASE, BASE*BASE*BASE, BASE*BASE, BASE, 1};

str5_t str5pack (register char *s) {
	register int i;
	register str5_t v, val;
	register unsigned char c;

	val = 0;
	if (s == NULL)
		return val;

	for (i=0; i < STR5LEN && (c = *s); i++, s++) {
		v = (str5_t) (
			c <= 0x1F ? 1 :		/* SOH to US */
			c <= 0x2A ? 2 :		/* space to '*' */
			c <= 0x7A ? (3 + (c - 0x2B)) : /* '+' to 'z' */
				     83			/* '{' to 0xff */
		) ;
		val += v * scale[i];
	}
	return val;
}

/*
 * For debugging - show one possible inverse to an str5_t encoding.
 */

static char buf[STR5LEN+1];

char *str5unpack (str5_t val) {
	int i;
	str5_t v;

	for (i = STR5LEN - 1; i >= 0; i--) {
		v = val % BASE;
		val /= BASE;

		buf[i] = (
			v == 0 ? 0 :
			v == 1 ? 1 :
			v == 2 ? 0x20 :
			v <= 82 ? ( ( v - 3) + 0x2B ) :
				'{'
		) ;
	}
	if (val) buf[0] = '~';
	return buf;
}
