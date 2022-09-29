/*
 * xtp_cksum.c --
 *
 *	XTP checksum routines.
 *
 * Copyright (C) 1989 Protocol Engines, Incorporated.
 * All rights reserved.
 * ##NOTICE##
 *
 * $Revision: 1.2 $
 * Modified for Netsnoop
 */

#include <sys/types.h>
#include "protocols/byteorder.h"


#define ROTATE(x)       ((x<<1) | ((unsigned)x>>31))
#define ROTATE16(x)	((x<<1) | ((u_short)x>>15))


/*
 * Concatenated xor+rxor checksum routines.
 */


static u_long
xtp_short_catxor (register u_short *w, register int len)
{
	register u_short rot = 0;
	register u_long	xor = 0;
	register u_long sxor;

#define	R(n) (w[n] << (15-n))
#define X(n) (((u_long *)w)[n])

	while (len > 16 * sizeof *w)  {
		sxor = R(0) ^ R(1) ^ R(2) ^ R(3)
		     ^ R(4) ^ R(5) ^ R(6) ^ R(7)
		     ^ R(8) ^ R(9) ^ R(10) ^ R(11)
		     ^ R(12) ^ R(13) ^ R(14) ^ R(15);
		xor ^= X(0) ^ X(1) ^ X(2) ^ X(3)
		     ^ X(4) ^ X(5) ^ X(6) ^ X(7);
		len -= 16 * sizeof *w;
		w += 16;
		rot ^= sxor ^ (u_short) (sxor >> 16);
	}
	while (len >= sizeof *w) {
		xor ^= *w;
		rot = ROTATE16 (rot) ^ *w++;
		len -= sizeof *w;
	}
#if BYTE_ORDER == LITTLE_ENDIAN
	if (len == 1) {
		rot = ROTATE16 (rot) ^ (*w & 0x00ff);
		xor ^= (*w & 0x00ff);
	}
	xor = (u_short)xor ^ (xor >> 16);
	return (rot << 16) | xor;
#else
	if (len == 1) {
		rot = ROTATE16 (rot) ^ (*w & 0xff00);
		xor ^= (*w & 0xff00);
	}
	xor = (u_short)xor ^ (xor >> 16);
	return (xor << 16) | rot;
#endif
}
#undef X
#undef R



u_long
xtp_long_catxor (addr, len, oldresult)
	register u_long *addr;		  /* assumed to be int-aligned */
	register int	len;		  /* number of BYTES in addr */
	u_long		oldresult;
{
	register u_short resrot = 0;
	register short	i;
	register u_long	t, resxor = 0;

#define BLK(n) \
   (addr[(4*n)*8] ^ addr[((4*n)+1)*8] ^ addr[((4*n)+2)*8] ^ addr[((4*n)+3)*8])

	if (oldresult) {
#if BYTE_ORDER == LITTLE_ENDIAN
		resxor = oldresult & 0xffff;
		resrot = oldresult >> 16;
#else
		resxor = oldresult >> 16;
		resrot = oldresult;
#endif
	}
	while (len >= 256) {
		register u_long	trot = 0;
		for (i=8; i-- > 0; addr++) {
			t = 0;
			switch (len >> 8) {
			    default:
			    case 8: t = BLK(8) ^ BLK(9) ^ BLK(10) ^ BLK(11) ^
				        BLK(12) ^ BLK(13) ^ BLK(14) ^ BLK(15);
			    case 7: 
			    case 6:
			    case 5:
			    case 4: t ^= BLK(6) ^ BLK(7);
			    case 3: t ^= BLK(4) ^ BLK(5);
			    case 2: t ^= BLK(2) ^ BLK(3);
			    case 1: t ^= BLK(0) ^ BLK(1);
				break;
			}
			resxor ^= t;
			trot = (trot << 2) ^ (t & 0xffff) ^
				((t >> 15) & 0x1fffe);
		}
		resrot ^= (trot >> 16) ^ (trot & 0xffff);
		switch (len >> 8) {
		    case 0:
			break;
		    case 1:
			addr += 256/sizeof (u_long) - 8;
			len -= 256;
			break;
		    case 2:
			addr += 512/sizeof (u_long) - 8;
			len -= 512;
			break;
		    case 3:
			addr += 768/sizeof (u_long) - 8;
			len -= 768;
			break;
		    case 4:
		    case 5:
		    case 6:
		    case 7:
			addr += 1024/sizeof (u_long) - 8;
			len -= 1024;
			break;
		    case 8:
		    default:
			addr += 2048/sizeof (u_long) - 8;
			len -= 2048;
			break;
		}
	}
	if (len > 0) {
		short	rot;

		/*
		 *	Compute the amount to rotate resrot so that it
		 *	may be prepended to the checksum for the last bytes.
		 */

		if (rot = ((len + 1) >> 1) & 0x0f)
			resrot = (resrot << rot) | (resrot >> (16-rot));
#if BYTE_ORDER == LITTLE_ENDIAN
		return ((resxor ^ (resxor >> 16)) & 0xffff)
			^ ((u_long)resrot << 16)
			^ xtp_short_catxor (addr, len);
#else
		return (((resxor ^ (resxor << 16)) & 0xffff0000) ^ resrot)
			^ xtp_short_catxor ((u_short *)addr, len);
#endif
	}
#if BYTE_ORDER == LITTLE_ENDIAN
	return ((resxor ^ (resxor >> 16)) & 0xffff) ^ ((u_long)resrot << 16);
#else
	return ((resxor ^ (resxor << 16)) & 0xffff0000) ^ resrot;
#endif
}


/*
 * Compute the checksum for the header and trailer.
 *
 * N.B. The header route and the trailer htcheck fields are not included
 * in the checksum.
 */

u_long
xtp_htcheck (head, tail)
	register u_long *head;
	register u_long *tail;
{
	register u_long	resxor, resrot;

#if BYTE_ORDER == LITTLE_ENDIAN
#define TH(n)	(((u_short *)head)[n^1] << (15-n))
#define TT(n)	(((u_short *)tail)[n^1] << (5-n))
#define NOTTL	(((u_short *)tail)[5])
#else
#define TH(n)	(((u_short *)head)[n] << (15-n))
#define TT(n)	(((u_short *)tail)[n] << (5-n))
#define NOTTL	(((u_short *)tail)[4] << 16)
#endif
	resxor = head[0] ^ head[1] ^ head[2] ^ head[3] ^ head[4] ^
		 tail[0] ^ tail[1] ^ NOTTL;

	resrot = 
	  TH (0) ^ TH (1) ^ TH (2) ^ TH (3) ^ TH (4) ^ TH (5) ^ 
	  TH (6) ^ TH (7) ^ TH (8) ^ TH (9) ^
	  TT (0) ^ TT (1) ^ TT (2) ^ TT (3) ^ 
#if BYTE_ORDER == LITTLE_ENDIAN
	  TT (5);
#else
	  TT (4);
#endif
#undef TH
#undef TT
#undef NOTTL
	return (resxor << 16) ^ (resxor & 0xffff0000)
		^ (u_short)(resrot >> 16) ^ (u_short)resrot;
}
