#ident "$Revision: 1.4 $"

#include <sys/param.h>
#include <sys/fs/xfs_types.h>
#include "bit.h"

static int	getbit(char *ptr, int bit);
static void	setbit(char *ptr, int bit, int val);

static int
getbit(
	char	*ptr,
	int	bit)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = 1 << shift;
	return (*ptr & mask) >> shift;
}

static void
setbit(
	char *ptr,
	int  bit,
	int  val)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = (1 << shift);
	if (val) {
		*ptr |= mask;
	} else {
		mask = ~mask;
		*ptr &= mask;
	}
}

__int64_t
getbitval(
	void		*obj,
	int		bitoff,
	int		nbits,
	int		flags)
{
	int		bit;
	int		i;
	char		*p;
	__int64_t	rval;
	int		signext;
	int		z1, z2, z3, z4;

	p = (char *)obj + byteize(bitoff);
	bit = bitoffs(bitoff);
	signext = (flags & BVSIGNED) != 0;
	z4 = ((__psint_t)p & 0xf) == 0 && bit == 0;
	if (nbits == 64 && z4) {
		if (signext)
			return (__int64_t)*(__int64_t *)p;
		else
			return (__int64_t)*(__uint64_t *)p;
	}
	z3 = ((__psint_t)p & 0x7) == 0 && bit == 0;
	if (nbits == 32 && z3) {
		if (signext)
			return (__int64_t)*(__int32_t *)p;
		else
			return (__int64_t)*(__uint32_t *)p;
	}
	z2 = ((__psint_t)p & 0x3) == 0 && bit == 0;
	if (nbits == 16 && z2) {
		if (signext)
			return (__int64_t)*(__int16_t *)p;
		else
			return (__int64_t)*(__uint16_t *)p;
	}
	z1 = ((__psint_t)p & 0x1) == 0 && bit == 0;
	if (nbits == 8 && z1) {
		if (signext)
			return (__int64_t)*(__int8_t *)p;
		else
			return (__int64_t)*(__uint8_t *)p;
	}
	for (i = 0, rval = 0LL; i < nbits; i++) {
		if (getbit(p, bit + i)) {
			if (i == 0 && signext && nbits < 64)
				rval = -1LL << nbits;
			rval |= 1LL << (nbits - i - 1);
		}
	}
	return rval;
}

void
setbitval(
	void *obuf,      /* buffer to write into */
	int bitoff,      /* bit offset of where to write */
	int nbits,       /* number of bits to write */
	void *ibuf)      /* source bits */
{
	int i;
	int num_bytes      = byteize(nbits);
	int copy_offset;
	char *in           = (char *)ibuf;
	char *out          = (char *)obuf;

	if (!(bitoff&0x07) && !(nbits&0x07)) {

		/* byte aligned */
		out += byteize(bitoff);

		for (i = 0; i < num_bytes; i++)
			*out++ = *in++;

	} else {

		/* not byte aligned (big endian hack):
		 *
		 * we assume the only non byte aligned items are integers
		 * and handle big endianess by starting the copy from the
                 * lsb.
		 */

		if (nbits <= 8)
			copy_offset = 8 - nbits;
		else if (nbits <= 16)
			copy_offset = 16 - nbits;
		else if (nbits <= 32)
			copy_offset = 32 - nbits;
		else
			copy_offset = 64 - nbits;

		for (i = 0; i < nbits; i++)
			setbit(out, bitoff + i, getbit(in, i + copy_offset));
	}
}


