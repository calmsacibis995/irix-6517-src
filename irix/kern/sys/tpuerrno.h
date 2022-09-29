/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/********************************************************************************
 *										*
 * sys/tpuerrno.h - Mesa TPU error number definitions.				*
 *										*
 ********************************************************************************/

#ifndef __SYS_TPUERRNO_H__
#define __SYS_TPUERRNO_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Id: tpuerrno.h,v 1.3 1999/02/05 21:58:32 pww Exp $"

#include <sys/errno.h>

/*
 * TPU_RUN ending status values.  If any values are changed or added, be sure to
 * change the tpu_error table.
 */
#define	TPU_OK		0		/* no error */
#define	TPU_INTR	1		/* operation interrupted by a process signal */
#define	TPU_HIO		2		/* operation interrupted by halt-i/o */
#define	TPU_TIMEOUT	3		/* operation timed out */
#define	TPU_CERROR	4		/* channel error interrupt */
#define	TPU_PAGECOUNT	5		/* invalid page count */
#define	TPU_PAGESIZE	6		/* invalid page size */
#define	TPU_LDIOFFSET	7		/* invalid LDI offset */
#define	TPU_BUSY	8		/* device busy */
#define	TPU_PIN_ADDR	9		/* invalid page address */
#define	TPU_PIN_SYS	10		/* insufficient system memory to lock pages */
#define	TPU_PIN_UNK	11		/* undefined error encountered while locking pages */
#define	TPU_PHYSMAX	12		/* too many physical pages */
#define	TPU_PHYSCONTIG	13		/* discontiguous physical page */
#define	TPU_PHYSCOUNT	14		/* logical/physical page count mismatch */
#define TPU_OFFLINE	15		/* device offline */
#define TPU_DOWNED	16		/* operation interrupted by config-down  */
#define TPU_SIM_FAULT	17		/* simulated fault other than the above */
#define	TPU_LERROR	18		/* LDI error interrupt */

#define	TPU_ERR_MAX	18		/* maximum status code value */

/*
 * Error code table.  This table is indexed by the internal driver status code
 * to get the corresponding system errno value and text string.  Be sure to
 * update it if new status codes are added above.
 */
static const struct {
	int		status;		/* internal status code */
	int		errno;		/* system errno value */
	const char *	string;		/* descriptive string */

} _tpu_status[] = 
	{
	TPU_OK,		0,		"No error",
	TPU_INTR,	EINTR,		"Operation interrupted by a process signal",
	TPU_HIO,	EIO,		"Operation interrupted by halt-i/o",
	TPU_TIMEOUT,	EIO,		"Operation timed out",
	TPU_CERROR,	EIO,		"Channel error interrupt",
	TPU_PAGECOUNT,	EINVAL,		"Invalid page count",
	TPU_PAGESIZE,	EINVAL,		"Invalid page size",
	TPU_LDIOFFSET,	EINVAL,		"Invalid LDI offset",
	TPU_BUSY,	EBUSY,		"Device busy",
	TPU_PIN_ADDR,	EFAULT,		"Error while locking user pages",
	TPU_PIN_SYS,	EAGAIN,		"Insufficient system memory to lock pages",
	TPU_PIN_UNK,	EFAULT,		"Undefined error encountered while locking pages",
	TPU_PHYSMAX,	EFAULT,		"Too many physical pages",
	TPU_PHYSCONTIG,	EFAULT,		"Discontiguous physical page",
	TPU_PHYSCOUNT,	EFAULT,		"Logical/physical page count mismatch",
	TPU_OFFLINE,	EIO,		"Device off-line",
	TPU_DOWNED,	EIO,		"Operation interrupted by config-down",
	TPU_SIM_FAULT,	EIO,		"Simulated fault condition",
	TPU_LERROR,	EIO,		"LDI error interrupt"
	};

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TPUERRNO_H__ */
