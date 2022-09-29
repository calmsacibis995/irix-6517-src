/* hp64700.h - Hewlett Packard HP64700 emulator SIM I/O header */

/* Copyright 1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,26jan94,dzb	 written for Hewlett Packard HP64783.
*/

/*
This file contains constants for the Hewlett Packard HP64700 emulator SIM I/O
interface.
*/

#ifndef __INChp64700h
#define __INChp64700h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE

#include "tylib.h"

#define	CAST (char *)

#define	SIMIO_BUF_MAX	260	/* maximum buffer size including control word */
#define	SIMIO_DATA_MAX	256	/* maximum buffer size - data only */
#define SIMIO_CA_MAX	6

/*
 * The following command codes are loaded into the simio
 * buffer's control address after the buffer has been loaded.
 */

#define	S_OPEN          0x90	/* open file */
#define	S_CLOSE         0x91	/* close file */
#define	S_READ          0x92	/* read from file */
#define	S_WRITE         0x93	/* write to file */
#define	S_DELETE_FILE   0x94	/* delete file */
#define	S_POS_FILE      0x95	/* position file pointer */
#define	S_POS_CURSOR    0x96	/* position cursor (display sim I/O only) */
#define	S_CLEAR_DISP    0x97	/* clear display (display sim I/O only) */
#define	S_EXEC_CMD      0x98	/* UNIX command */
#define	S_KILL          0x99	/* Kill simulated I/O process */
#define	S_RESET         0x9a	/* Reset simulated I/O */

#define	HP64700_KEYBOARD	"/dev/simio/keyboard"
#define	HP64700_DISPLAY		"/dev/simio/display"

#define ENO_ERROR               0
#define EFILE_NOT_FOUND         3
#define EFILE_EXISTS            4
#define ECANNOT_READ_MEMORY     8
#define EINVALID_CMD            9
#define EINVALID_ROW_OR_COLUMN  11
#define EINVALID_FILE_NAME      15
#define ENO_FREE_DESC           17
#define EINVALID_DESC           18
#define ENO_PERMISSION          22
#define EINVALID_OPTIONS        23
#define ETOO_MANY_FILES         24
#define ENO_FREE_PROC_ID        25
#define ETOO_MANY_PROCESSES     26
#define EINVALID_CMD_NAME       27
#define EINVALID_PROC_ID        28
#define EINVALID_SIGNAL         29
#define ENO_SUCH_PROCESS        30
#define ENO_SEEK_ON_PIPE        31
#define EUNIX_ERROR             126
#define ECONTINUE_ERROR         127

/* device descriptors */

typedef struct			/* SIMIO_PORT */
    {
    int		fd;
    int		options;
    UINT8 *	pName;
    UINT8 *	pBuf;
    BOOL	opened;
    } SIMIO_CA;

typedef struct			/* TY_CO_DEV */
    {
    DEV_HDR	devHdr;
    BOOL	created;	/* true if device has already been created */
    int		numChannels;	/* number of channels on device */
    SIMIO_CA	rxCA;
    SIMIO_CA	txCA;
    } TY_CO_DEV;

typedef struct
    {
    SEM_ID	semMId;
    UINT8 *	cAdrs;
    BOOL	active;
    } SIMIO_CA_PROTECT;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

#else	/* __STDC__ */

#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INChp64700h */
