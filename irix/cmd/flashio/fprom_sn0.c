/***********************************************************************\
*	File:		fprom.c						*
*									*
*	Support for Am29F080 and Am29LV800 1-megabyte flash PROMs	*
*									*
*	NOTE: Flash PROM cannot be programmed while running out of it.	*
*									*
*	The Hub Flash PROM may only be read via double-word or word	*
*	accesses.  The Hub translates these into 8 and 4 byte reads,	*
*	respectively.  Fortunately, this doesn't cause problems with	*
*	programming.  The 8-bit PROM command register may be written	*
*	via double-word writes only.  Only the LSByte is used, and	*
*	the PROM address offset must be multiplied by 8.		*
*									*
*	The IO6 Flash PROM may only be read and written via half-word	*
*	accesses.  In each case, the upper byte of each half-word is	*
*	unused, and the PROM address offset must be multiplied by 2.	*
*									*
*	Programming is doubly verified.  The PROM does its own		*
*	hardware programming verification, plus this module reads	*
*	back and verifies flashed or programmed data.			*
*									*
\***********************************************************************/

#ident	"$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/SN/fprom.h>

#define ABORT			(f->afn ? (*f->afn)(f) : 0)

#define AMD29F080_MANU_CODE1	0x01
#define AMD29F080_DEV_CODE1	0xd5

#define AMD29F080_MANU_CODE2	0x01
#define AMD29F080_DEV_CODE2	0xad

#define HY29F080_MANU_CODE	0xad
#define HY29F080_DEV_CODE	0xd5

#define M29F080A_MANU_CODE	0x20
#define M29F080A_DEV_CODE	0xf1

#define AMD29LV800_MANU_CODE	0x01
#define AMD29LV800_DEV_CODE	0x22DA

#define LBYTEU(caddr)							  \
	(uchar_t) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) <<	  \
		    ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define LBYTEUV(caddr)							  \
	(uchar_t) ((*(volatile uint *) ((__psunsigned_t) (caddr) & ~3) << \
		    ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define LB_HUB(_base, _offset) \
	LBYTEUV((uchar_t *) (_base) + (_offset))
#define SB_HUB(_base, _offset, _byte) \
	(*((volatile __uint64_t *) (_base) + (_offset)) = (_byte))
#define LD_HUB(_base, _offset) \
	(*(volatile __uint64_t *) ((__psunsigned_t) (_base) + (_offset)))
#define LB_IO6_P0(_base, _offset) \
	(*((volatile ushort_t *) (_base) + (_offset)))
#define SB_IO6_P0(_base, _offset, _byte) \
	(*((volatile ushort_t *) (_base) + (_offset)) = (_byte))

/* base + offset should be aligned. Checkit. */

#define LB_IO6_P1(_base, _offset) \
	(*((volatile uchar_t *) (_base) + (_offset)))
#define LH_IO6_P1(_base, _offset) \
	(*((volatile ushort_t *) (((uchar_t *)_base + _offset))))
#define SHCMD_IO6_P1(_base, _offset, _short) \
	(*((volatile ushort_t *) (_base) + (_offset)) = (_short))
#define SHDATA_IO6_P1(_base, _offset, _short) \
	(*((volatile ushort_t *) ((uchar_t *)_base + _offset)) = (_short))


#define SN00_ADDR_SWIZZLE(A)	(	\
	(((A) & 0xfff00000))	 |	 \
	(((A) & 0x0000f))	|	\
	(((A) & 0x00010) << 11) |	\
	(((A) & 0x00020) <<  9) |	\
	(((A) & 0x00040) <<  7) |	\
	(((A) & 0x00080) <<  5) |	\
	(((A) & 0x00100) <<  3) |	\
	(((A) & 0x00200) <<  1) |	\
	(((A) & 0x00400) >>  1) |	\
	(((A) & 0x00800) >>  3) |	\
	(((A) & 0x01000) >>  5) |	\
	(((A) & 0x02000) >>  7) |	\
	(((A) & 0x04000) >>  9) |	\
	(((A) & 0x08000) >> 11) |	\
	(((A) & 0xf0000)))

#define SN00_DATA_SWIZZLE(D)	(	\
	(((D) & 0x01) << 7) |		\
	(((D) & 0x02) << 5) |		\
	(((D) & 0x04) << 3) |		\
	(((D) & 0x08) << 1) |		\
	(((D) & 0x10) >> 1) |		\
	(((D) & 0x20) >> 3) |		\
	(((D) & 0x40) >> 5) |		\
	(((D) & 0x80) >> 7))
/*
 * cmd: Device-specific command write
 */
static
void
cmd(fprom_t *f, fprom_off_t offset, uchar_t byte)
{
#ifndef EMULATE
    switch (f->dev) {
    case FPROM_DEV_HUB:
	if (f->swizzle)
	    SB_HUB(f->base, SN00_ADDR_SWIZZLE(offset),
		   SN00_DATA_SWIZZLE(byte));
	else
	    SB_HUB(f->base, offset, byte);
	break;
    case FPROM_DEV_IO6_P0:
	SB_IO6_P0(f->base, offset, byte);
	break;
    case FPROM_DEV_IO6_P1:
	SHCMD_IO6_P1(f->base, offset, (short)byte);
	break ;
    }
#endif /* !defined(EMULATE) */
}

/*
 * fprom_wait
 *
 *   Wait for the PROM to finish programming or time out.
 *   We allow aborting via the abort function.
 */

static int fprom_wait(fprom_t *f, fprom_off_t offset, uchar_t byte)
{
    uchar_t		status;
    int			i;
    uchar_t		timeout_mask;

    if (f->swizzle)
	timeout_mask = SN00_DATA_SWIZZLE(0x20);
    else
	timeout_mask = 0x20;

    switch (f->dev) {
    case FPROM_DEV_HUB:
	while (! ABORT) {
#ifdef EMULATE
	    status = byte;
#else /* EMULATE */
	    status = LB_HUB(f->base, offset);
#endif /* EMULATE */
	    if (status == byte)
		return 0;		/* Programming complete */
	    if (status & timeout_mask) {
		status = LB_HUB(f->base, offset);
		if (status == byte)
		    return 0;		/* Programming complete */
		return FPROM_ERROR_TIMEOUT;
	    }
	}
	break;
    case FPROM_DEV_IO6_P0:
	while (! ABORT) {
#ifdef EMULATE
	    status = byte;
#else /* EMULATE */
	    status = LB_IO6_P0(f->base, offset);
#endif /* EMULATE */
	    if (status == byte)
		return 0;		/* Programming complete */
	    if (status & timeout_mask) {
		status = LB_IO6_P0(f->base, offset);
		if (status == byte)
		    return 0;		/* Programming complete */
		return FPROM_ERROR_TIMEOUT;
	    }
	}
	break;
    case FPROM_DEV_IO6_P1:
	/* XXX - Bring this back in, as flash does not seem to work. */

	for (i=0; i<64; i++)	/* P1 flash needs lots of delay */
		LB_IO6_P1(f->base, 0);

	while (! ABORT) {
#ifdef EMULATE
	    status = byte;
#else /* EMULATE */
	    status = LB_IO6_P1(f->base, offset);
#endif /* EMULATE */
	    if (status == byte)
		return 0;		/* Programming complete */
	    if (status & timeout_mask) {
		status = LB_IO6_P1(f->base, offset);
		if (status == byte)
		    return 0;		/* Programming complete */
		return FPROM_ERROR_TIMEOUT;
	    }
	}
	break;
    }

    return FPROM_ERROR_ABORT;
}

/*
 * fprom_reset
 *
 *   Puts a PROM into regular read mode, in case it's not already, which
 *   often happens if a PROM programming or probing operation takes an
 *   exception.
 */

void fprom_reset(fprom_t *f)
{
    cmd(f, 0x0000, 0xf0);		/* Reset to synchronize */
}

/*
 * fprom_probe
 *
 *   Physically resets the PROM to get it into the read state.  Verifies
 *   the PROM is connected and well-behaved by reading its manufacturer and
 *   device codes.  Places it into normal read mode.
 *
 *   *** NOTE ***
 *
 *   This call MUST be used before any call that attempts to WRITE to
 *   the flash PROM (fprom_flash_sectors, fprom_write, etc).  It sets
 *   the appropriate swizzle state in the fprom_t structure.
 *
 *   Returns FPROM_ERROR_code.
 */

static int do_probe(fprom_t *f, int *manu_code, int *dev_code)
{
    fprom_reset(f);			/* Reset to synchronize */

    cmd(f, 0x5555, 0xaa);		/* Enter autoselect mode */
    cmd(f, 0x2aaa, 0x55);
    cmd(f, 0x5555, 0x90);

    /* Retrieve manufacturer and device codes */

    switch (f->dev) {
    case FPROM_DEV_HUB:
	*manu_code = LB_HUB(f->base, 0);
	if (f->swizzle) {
	    *manu_code = SN00_DATA_SWIZZLE(*manu_code);
	    *dev_code  = LB_HUB(f->base, SN00_ADDR_SWIZZLE(1));
	    *dev_code = SN00_DATA_SWIZZLE(*dev_code);
	} else
	    *dev_code  = LB_HUB(f->base, 1);
	break;
    case FPROM_DEV_IO6_P0:
	*manu_code = LB_IO6_P0(f->base, 0);
	*dev_code  = LB_IO6_P0(f->base, 1);
	break;
    case FPROM_DEV_IO6_P1:
	*manu_code = LH_IO6_P1(f->base, 0);
	*dev_code  = LH_IO6_P1(f->base, 2);
	break;
    }

#ifdef EMULATE
    *manu_code = AMD29F080_MANU_CODE1;
    *dev_code  = AMD29F080_DEV_CODE1;
#endif /* EMULATE */

    fprom_reset(f);		/* Reset to exit autoselect mode */

    if (*manu_code == AMD29LV800_MANU_CODE &&
	*dev_code  == AMD29LV800_DEV_CODE) {
	f->dev = FPROM_DEV_IO6_P1 ;
	return FPROM_ERROR_NONE;
    }

    if (*manu_code == HY29F080_MANU_CODE &&
	*dev_code  == HY29F080_DEV_CODE)
	return FPROM_ERROR_NONE;

    if (*manu_code == M29F080A_MANU_CODE &&
	*dev_code  == M29F080A_DEV_CODE)
	return FPROM_ERROR_NONE;

    if (*manu_code == AMD29F080_MANU_CODE1 &&
	(*dev_code == AMD29F080_DEV_CODE1 ||
	 *dev_code == AMD29F080_DEV_CODE2))
	return FPROM_ERROR_NONE;

    return FPROM_ERROR_DEVICE;
}

int
fprom_probe(fprom_t *f, int *manu_code, int *dev_code)
{
    int			r1, r2;

    f->swizzle = 0;

    r1 = do_probe(f, manu_code, dev_code);

    /*
     * P0 SN00 have a problem with swizzled address and data
     * Here we try and figure out if we have a problem.
     */

    if (r1 != FPROM_ERROR_NONE) {
	f->swizzle = 1;
	r2 = do_probe(f, manu_code, dev_code);
	if (r2 != FPROM_ERROR_NONE) {
	    f->swizzle = 0;
	    return(r1);
	}
	return (r2);
    }

    return r1;
}

/*
 * fprom_flash_verify
 *
 *   Verifies that a specified sector of the PROM is all 1's.
 *   Returns FPROM_ERROR_code.
 */

#ifdef SABLE
#define INCR	16
#else /* SABLE */
#define INCR	1
#endif

int fprom_flash_verify(fprom_t *f, int sector)
{
    fprom_off_t		istart, iend, i;

    if (ABORT)
	return FPROM_ERROR_ABORT;

    istart = (sector + 0) * FPROM_SECTOR_SIZE;
    iend   = (sector + 1) * FPROM_SECTOR_SIZE;

    switch (f->dev) {
    case FPROM_DEV_HUB:
	for (i = istart; i < iend; i += 8)
	    if (LD_HUB(f->base, i) != ~0ULL)	/* Fast native read size */
		return FPROM_ERROR_VERIFY;
	break;
    case FPROM_DEV_IO6_P0:
	for (i = istart; i < iend; i++)
	    if (LB_IO6_P0(f->base, i) != 0xff)	/* Oh well */
		return FPROM_ERROR_VERIFY;
	break;
    case FPROM_DEV_IO6_P1:
	for (i = istart; i < iend; i++)
	    if (LB_IO6_P1(f->base, i) != 0xff)	/* Oh well */
		return FPROM_ERROR_VERIFY;
	break;
    }

    return FPROM_ERROR_NONE;
}

/*
 * fprom_flash_all
 *
 *   Flashes the entire PROM.  Verifies that every byte of the PROM is
 *   0xff.  Returns FPROM_ERROR_code.
 */

int fprom_flash_all(fprom_t *f)
{
    int			i, r;

    cmd(f, 0x5555, 0xaa);		/* Chip Erase */
    cmd(f, 0x2aaa, 0x55);
    cmd(f, 0x5555, 0x80);
    cmd(f, 0x5555, 0xaa);
    cmd(f, 0x2aaa, 0x55);
    cmd(f, 0x5555, 0x10);

#ifdef EMULATE
    memset(f->base, 0xff, FPROM_SIZE);
#endif /* EMULATE */

    if ((r = fprom_wait(f, 0, (uchar_t) 0xff)) < 0)
	return r;

    for (i = 0; i < FPROM_SECTOR_COUNT; i++)
	if ((r = fprom_flash_verify(f, i)) < 0)
	    return r;

    return FPROM_ERROR_NONE;
}


/*
 * do_flash
 *
 *   Erase a single (sub-)sector, and wait for the chip to finish.
 */

static int do_flash(fprom_t *f, fprom_off_t cmdaddr, fprom_off_t waitaddr)
{
    cmd(f, 0x5555, 0xaa);		/* Sector Erase prefix */
    cmd(f, 0x2aaa, 0x55);
    cmd(f, 0x5555, 0x80);
    cmd(f, 0x5555, 0xaa);
    cmd(f, 0x2aaa, 0x55);

    cmd(f, cmdaddr, 0x30);		/* Sector Erase command */

    return fprom_wait(f, waitaddr, (uchar_t) 0xff);
}


/*
 * fprom_flash_sectors
 *
 *   *** NOTE *** fprom_probe must be called first; see comment there.
 *
 *   Flashes any combination of 64K PROM sectors.
 *   Sector_mask contains a mask of 64K sectors to erased: bit 0 erases
 *   sector 0, etc.  Waits for the operation to complete, then verifies that
 *   the erased sectors contain 0xff.  Returns FPROM_ERROR_code.
 *   Sectors are flashed one at a time, because we can't guarantee
 *   that we will meet the PROM chip's multi-sector setup timeout when
 *   running in UNCAC or DEX mode.
 */

int fprom_flash_sectors(fprom_t *f, int sector_mask)
{
    int			i, r;

    fprom_reset(f);			/* Reset */

    if ((sector_mask &= 0xffff) == 0)
	return FPROM_ERROR_NONE;	/* Handle empty sector_mask */

    /*
     * Write erase commands to target sectors
     */

    for (i = 0; i < FPROM_SECTOR_COUNT; i++)
	if (sector_mask & 1 << i) {
#ifdef EMULATE
	    memset((char *) f->base + i * FPROM_SECTOR_SIZE,
		   0xff,
		   FPROM_SECTOR_SIZE);
#endif /* EMULATE */

	    switch (f->dev) {
	    case FPROM_DEV_HUB:
		do_flash(f, i * FPROM_SECTOR_SIZE, i * FPROM_SECTOR_SIZE);
		break;
	    case FPROM_DEV_IO6_P0:
		if (i < 15)
		    do_flash(f, i * FPROM_SECTOR_SIZE, i * FPROM_SECTOR_SIZE);
		else {
		    do_flash(f, 0xf0000, 0xf0000);
		    do_flash(f, 0xf8000, 0xf8000);
		    do_flash(f, 0xfa000, 0xfa000);
		    do_flash(f, 0xfc000, 0xfc000);
		}
		break ;
	    case FPROM_DEV_IO6_P1:
		if (i < 15)
		    do_flash(f, i * (FPROM_SECTOR_SIZE / 2),
			     i * FPROM_SECTOR_SIZE);
		else {
		    do_flash(f, 0xf0000 / 2, 0xf0000);
		    do_flash(f, 0xf8000 / 2, 0xf8000);
		    do_flash(f, 0xfa000 / 2, 0xfa000);
		    do_flash(f, 0xfc000 / 2, 0xfc000);
		}
		break ;
	    }
	}

    for (i = 0; i < FPROM_SECTOR_COUNT; i++)
	if (sector_mask & 1 << i)
	    if ((r = fprom_flash_verify(f, i)) < 0)
		return r;

    return FPROM_ERROR_NONE;
}

/*
 * fprom_read
 *
 *   Copy characters from the PROM into a buffer, using only the kind
 *   of reads that are allowed from the PROM.  Returns FPROM_ERROR_code.
 */

int fprom_read(fprom_t *f, fprom_off_t offset, char *buf, int len)
{
    if (len == 0)
	return FPROM_ERROR_NONE;

    switch (f->dev) {
    case FPROM_DEV_HUB:
	{
	    __uint64_t	       *p, t, s;

	    p = (__uint64_t *) (((__psunsigned_t) f->base + offset) & ~7);
	    t = (__uint64_t  ) (((__psunsigned_t) f->base + offset) &  7);
	    s = (*(__uint64_t *) p) << 8 * t;

	    for (; t < 8 && len-- > 0; t++) {
		*buf++ = (char) (s >> 56);
		s <<= 8;
	    }

	    while (len > 0) {
		s = *(__uint64_t *) ++p;
		for (t = 0; t < 8 && len-- > 0; t++) {
		    *buf++ = (char) (s >> 56);
		    s <<= 8;
		}
	    }
	}
	break;
    case FPROM_DEV_IO6_P0:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++)
		*buf++ = LB_IO6_P0(f->base, off);
	}
	break;
    case FPROM_DEV_IO6_P1:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++)
		*buf++ = LB_IO6_P1(f->base, off);
	}
	break;
    }

    return FPROM_ERROR_NONE;
}

/*
 * fprom_validate
 *
 *   Compare part of the PROM with the contents of the buffer.  If copying
 *   the buffer to that part of the PROM would be illegal (because 0s would
 *   have to be programmed into 1s) returns FPROM_ERROR_CONFLICT.
 */

int fprom_validate(fprom_t *f, fprom_off_t offset, char *buf, int len)
{
    if (ABORT)
	return FPROM_ERROR_ABORT;

    if (len == 0)
	return FPROM_ERROR_NONE;

    switch (f->dev) {
    case FPROM_DEV_HUB:
	{
	    __uint64_t	       *p, t, s;

	    p = (__uint64_t *) (((__psunsigned_t) f->base + offset) & ~7);
	    t = (__uint64_t  ) (((__psunsigned_t) f->base + offset) &  7);
	    s = (*(__uint64_t *) p) << 8 * t;

	    for (; t < 8 && len-- > 0; t++, buf++) {
		if (~s >> 56 & LBYTEU(buf))
		    return FPROM_ERROR_CONFLICT;
		s <<= 8;
	    }

	    while (len > 0) {
		s = *(__uint64_t *) ++p;
		for (t = 0; t < 8 && len-- > 0; t++, buf++) {
		    if (~s >> 56 & LBYTEU(buf))
			return FPROM_ERROR_CONFLICT;
		    s <<= 8;
		}
	    }
	}
	break;
    case FPROM_DEV_IO6_P0:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++, buf++)
		if (~(LB_IO6_P0(f->base, off)) & LBYTEU(buf))
		    return FPROM_ERROR_CONFLICT;
	}
	break;
    case FPROM_DEV_IO6_P1:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++, buf++)
		if (~(LB_IO6_P1(f->base, off)) & LBYTEU(buf))
		    return FPROM_ERROR_CONFLICT;
	}
	break;
    }

    return FPROM_ERROR_NONE;
}

/*
 * fprom_verify
 *
 *   Compare part of the PROM with the contents of the buffer.
 *   If they differ, returns FPROM_ERROR_VERIFY.
 */

int fprom_verify(fprom_t *f, fprom_off_t offset, char *buf, int len)
{
    if (len == 0)
	return FPROM_ERROR_NONE;

    switch (f->dev) {
    case FPROM_DEV_HUB:
	{
	    __uint64_t	       *p, t, s;

	    p = (__uint64_t *) (((__psunsigned_t) f->base + offset) & ~7);
	    t = (__uint64_t  ) (((__psunsigned_t) f->base + offset) &  7);
	    s = (*(__uint64_t *) p) << 8 * t;

	    for (; t < 8 && len-- > 0; t++, buf++) {
		if (s >> 56 != LBYTEU(buf))
		    return FPROM_ERROR_VERIFY;
		s <<= 8;
	    }

	    while (len > 0) {
		s = *(__uint64_t *) ++p;
		for (t = 0; t < 8 && len-- > 0; t++, buf++) {
		    if (s >> 56 != LBYTEU(buf))
			return FPROM_ERROR_VERIFY;
		    s <<= 8;
		}
	    }
	}
	break;
    case FPROM_DEV_IO6_P0:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++, buf++)
		if (LB_IO6_P0(f->base, off) != LBYTEU(buf))
		    return FPROM_ERROR_VERIFY;
	}
	break;
    case FPROM_DEV_IO6_P1:
	{
	    fprom_off_t		soff	= offset;
	    fprom_off_t		eoff	= offset + (fprom_off_t) len;
	    fprom_off_t		off;

	    for (off = soff; off < eoff; off++, buf++)
		if (LB_IO6_P1(f->base, off) != LBYTEU(buf))
		    return FPROM_ERROR_VERIFY;
	}
	break;
    }

    return FPROM_ERROR_NONE;
}

/*
 * do_write (INTERNAL)
 *
 *   Programs characters from a buffer into the PROM.
 *   Does not validate or verify.  Returns FPROM_ERROR_code.
 */

static int do_write(fprom_t *f, fprom_off_t offset, char *buf, int len)
{
    int			r;
    uchar_t		old, new;

#ifdef EMULATE
    for (r = 0; r < len; r++)
	if (~*((char *) f->base + offset + r) & LBYTEU(&buf[r]))
	    printf("Conflict, offset 0x%x\n", offset + r);
	else
	    *((char *) f->base + offset + r) = LBYTEU(&buf[r]);

    return 0;
#else /* EMULATE */

    switch (f->dev) {
    case FPROM_DEV_HUB:
	for (; len-- > 0; buf++, offset++) {
	    old = LB_HUB(f->base, offset);
	    new = LBYTEU(buf);

	    if (~old & new)
		return FPROM_ERROR_CONFLICT;
	    else if (old != new) {
		    /*    printf("Trying to store byte, old is %d new is %d \n",old,new); */
		cmd(f, 0x5555, 0xaa);
		cmd(f, 0x2aaa, 0x55);
		cmd(f, 0x5555, 0xa0);
		SB_HUB(f->base, offset, new);

		if ((r = fprom_wait(f, offset, new)) < 0)
		    return r;
	    }
	}
	break;
    case FPROM_DEV_IO6_P0:
	for (; len-- > 0; buf++, offset++) {
	    old = LB_IO6_P0(f->base, offset);
	    new = LBYTEU(buf);

	    if (~old & new)
		return FPROM_ERROR_CONFLICT;
	    else if (old != new) {
		SB_IO6_P0(f->base, 0x5555, 0xaa);
		SB_IO6_P0(f->base, 0x2aaa, 0x55);
		SB_IO6_P0(f->base, 0x5555, 0xa0);
		SB_IO6_P0(f->base, offset, new);

		if ((r = fprom_wait(f, offset, new)) < 0)
		    return r;
	    }
	}
	break;
    case FPROM_DEV_IO6_P1:
    {
	ushort_t	*sp = (ushort_t	*) buf, old_s;
	int		length = len;

	if (((__psunsigned_t) buf | (__psunsigned_t) length) & 1)
	    return FPROM_ERROR_ODDIO6;

	for (; length > 0; sp++, (offset += 2), (length-=2)) {
	    old_s = LH_IO6_P1(f->base, offset);

	    if (~old_s & (ushort_t) *sp)
		return FPROM_ERROR_CONFLICT;
	    else if (old_s != (ushort_t) *sp) {
		SHCMD_IO6_P1(f->base, 0x5555, 0xaa);
		SHCMD_IO6_P1(f->base, 0x2aaa, 0x55);
		SHCMD_IO6_P1(f->base, 0x5555, 0xa0);
		SHDATA_IO6_P1(f->base, offset, (ushort_t) *sp);

		if ((r = fprom_wait(f, offset, (uchar_t) (*sp >> 8))) < 0)
		    return r;
	    }
	}
    }
	break;
    }

    return FPROM_ERROR_NONE;

#endif /* EMULATE */
}

/*
 * fprom_write
 *
 *   *** NOTE *** fprom_probe must be called first; see comment there.
 *
 *   Programs characters from a buffer into the PROM, then verifies.
 *   Returns FPROM_ERROR_code.
 */

int fprom_write(fprom_t *f, fprom_off_t offset, char *buf, int len)
{
    int			r;

    if ((r = do_write(f, offset, buf, len)) < 0)
	return r;

    if ((r = fprom_verify(f, offset, buf, len)) < 0)
	return r;

    return FPROM_ERROR_NONE;
}

/*
 * fprom_errmsg
 *
 *   Translates an FPROM_ERROR_code into an appropriate message string.
 */

char *fprom_errmsg(int rc)
{
    switch (rc) {
    case FPROM_ERROR_NONE:
	return "No error";
    case FPROM_ERROR_RESPOND:
	return "Chip not responding";
    case FPROM_ERROR_TIMEOUT:
	return "Programming operation timed out";
    case FPROM_ERROR_CONFLICT:
	return "Cannot program a 0 into a 1";
    case FPROM_ERROR_VERIFY:
	return "Data verify failed";
    case FPROM_ERROR_ABORT:
	return "Operation externally aborted";
    case FPROM_ERROR_DEVICE:
	return "Unknown manufacturer/device ID";
    case FPROM_ERROR_ODDIO6:
	return "Odd buffer or length not allowed with IO6 P1";
    default:
	return "Undefined error code";
    }
}
