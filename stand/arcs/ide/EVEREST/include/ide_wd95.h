/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/************************************************************************
 *                                                                      *
 *      wd95a_regdef.c - wd95a defs for testing		*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>
#include <sys/wd95a.h>

/* from libsk/io/wd95.c */
#define GET_WD_REG(_sbp, _reg) \
                (*(uint *)(_sbp + (_reg << ACC_SHIFT) + 4) & 0xff)

#define SET_WD_REG(_sbp, _reg, _val) \
                *(uint *)(_sbp + (_reg << ACC_SHIFT) + 4) = (_val)

#define ACC_SHIFT       3               /* if you prefer shifting */

/* SCSI command arrays */
#define SCSI_INQ_CMD    0x12
#define SCSI_SEND_DIAG  0x1d

typedef struct reglist {
        char            *rname;
        int             rnum;
        unchar          rval[2];
	int		read_only;
} reglist_t;
	
