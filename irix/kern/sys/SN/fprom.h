/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#ifndef	__SYS_SN_FPROM_H__
#define	__SYS_SN_FPROM_H__

#define FPROM_SIZE		0x100000
#define FPROM_SECTOR_SIZE	0x10000
#define FPROM_SECTOR_COUNT	16

#if _LANGUAGE_C

/*
 * Some routines take a abort function parameter, afn.  If afn != NULL,
 * it is called periodically.  It should return 1 to abort the operation,
 * 0 if not.  A pointer to the fprom_t structure is passed to afn.  The
 * fprom_t structure contains a little space for use by the afn (aparm),
 * not used by the fprom package.
 */

typedef struct fprom_s		fprom_t;
typedef int			fprom_off_t;
typedef int		      (*fprom_afn)(fprom_t *f);

struct fprom_s {
    void		       *base;
    int				dev;
    fprom_afn			afn;
    __psunsigned_t		aparm[8];
    int				swizzle;
};

#define FPROM_DEV_HUB		0
#define FPROM_DEV_IO6_P0	1
#define FPROM_DEV_IO6_P1	2

void	fprom_reset(fprom_t *f);
int	fprom_probe(fprom_t *f, int *manu_code, int *dev_code);

int	fprom_flash_sectors(fprom_t *f, int sector_mask);
int	fprom_flash_verify(fprom_t *f, int sector);

int	fprom_validate(fprom_t *f, fprom_off_t offset, char *buf, int len);
int	fprom_verify(fprom_t *f, fprom_off_t offset, char *buf, int len);
int	fprom_read(fprom_t *f, fprom_off_t offset, char *buf, int len);
int	fprom_write(fprom_t *f, fprom_off_t offset, char *buf, int len);

char   *fprom_errmsg(int rc);

#endif /* _LANGUAGE_C */

#define FPROM_ERROR_NONE	0	/* No error			*/
#define FPROM_ERROR_RESPOND	-1	/* Chip not responding		*/
#define FPROM_ERROR_TIMEOUT	-2	/* Operation timed out		*/
#define FPROM_ERROR_CONFLICT	-3	/* Cannot program a 0 into a 1	*/
#define FPROM_ERROR_VERIFY	-4	/* Data verify failed		*/
#define FPROM_ERROR_ABORT	-5	/* Aborted by abort function	*/
#define FPROM_ERROR_DEVICE	-6	/* Unknown manu/device ID   	*/
#define FPROM_ERROR_ODDIO6	-7	/* Odd buffer or length w/IO6P1 */

#endif /* __SYS_SN_FPROM_H__ */
