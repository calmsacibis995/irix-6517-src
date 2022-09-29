#ifndef __SYS_SCSIMSGS_H__
#define __SYS_SCSIMSGS_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"
/*
 * scsimsgs.h
 *
 * This file fixes some of the broken defines in scsi.h and
 * newscsi.h
 *
 * The defines SC_NUMSENSE and SC_NUMADDSENSE are in scsi.h but they
 * are overridden by new values in newscsi.h. These arrays are
 * defined in controller specific files like ql.c, wd95.c which
 * use values in newscsi.h. Including newscsi.h in dksc.c may cause other
 * problems. So only these controller file dependent information
 * is changed. If any of these change in newscsi.h it should
 * be reflected in include/sys/scsimsgs.h
 */

#ifdef SN0 /* || IP30 */
#undef  SC_NUMSENSE
#undef  SC_NUMADDSENSE
#define SC_NUMSENSE     0x10    /* # of messages in scsi_key_msgtab */
#define SC_NUMADDSENSE  0x4a    /* # of messages in scsi_addit_msgtab */
#endif

#endif /* !__SYS_SCSI_H__ */
