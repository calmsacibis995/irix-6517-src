/*
 * biendian.h
 *
 * $Revision: 1.5 $
 */

#if EVEREST && PROM
/*
 * The address of something in the IP19 prom is 0xbfcxxxxx.
 * Lowprom data segment is 0xa08xxxxx.  Both of these had
 * better agree with ld's -T value in IP19prom/Makefile.
 */
#if _MIPS_SZPTR == 32
#define	INBOOTPROM(s)	((u_int)(s) >> 20 == 0xbfc || (u_int)(s) >> 20 == 0xa08)
#endif
#if _MIPS_SZPTR == 64
#define	INBOOTPROM(s)	((__scunsigned_t)(s) >> 20 == 0x900000001fc || (__scunsigned_t)(s) >> 20 == 0x90000000084)
#endif

/*
 * Prom byte arrays (e.g., strings) are _word_ flipped (xor 3)
 * in the opposite endianness.
 */

#define FLIPSET(do, s)	do = getendian() && INBOOTPROM(s)
#define FLIPINIT(do, s)	int FLIPSET(do, s)
#define	FLIP(s)		((u_char *)((__scunsigned_t)(s)^3))
#define	FLIPU(s)	((u_char *)((__scunsigned_t)(s)^3))
#define	EVCFLIP(do, s)	get_char(do ? FLIP(s) : (u_char *)(s))
#define	EVCFLIPU(do, s)	get_char(do ? FLIPU(s) : (u_char *)(s))

#else	/* !(EVEREST && PROM) */

#define FLIPSET(do,s)
#define	FLIPINIT(do, s)
#define	FLIP(s)		(s)
#define	FLIPU(s)	(s)
#define	EVCFLIP(do, s)	get_char(s)
#define	EVCFLIPU(do, s)	get_char(s)
#define	INBOOTPROM(s)	(s)

#endif	/* !(EVEREST && PROM) */
