#ifndef __FSD_H__
#define __FSD_H__
#ifdef __cplusplus
extern "C" {
#endif

/*
 * fsd.h
 *
 *  Constants used by System Manager and the file system daemons
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.3 $"

/*
 * Flags for Exit codes for cdromd (or dosd) -q <fsname>
 *
 * The exit code for cdromd -q is a bitwise or of these masks; e.g.
 *
 *      cdromd -q /dev/scsi/sc0d7l0
 *      echo $status
 *      6       # == FSDDQ_YESFSD | FSDDQ_NOFSD
 *
 * This means that cdromd is running for /dev/scsi/sc0d7l0, and the
 * system is configured so that it will start running for that device
 * each time the system is booted.
 */

#define FSDQ_NOFSD      0x0000  /* cdromd is not running for fsname */
#define FSDQ_ERROR      0x0001  /* An error has occurred */
#define FSDQ_YESFSD     0x0002  /* cdromd is running for fsname */
#define FSDQ_ALWAYS     0x0004  /* system is configured for cdromd at boot */
#define FSDQ_NOICON     0x0008  /* do not display an icon for fsname */

/*
 * Leave the old names for now so System Manager doesn't break
 */
#define CDQ_NOFSD       FSDQ_NOFSD
#define CDQ_ERROR       FSDQ_ERROR
#define CDQ_YESFSD      FSDQ_YESFSD
#define CDQ_ALWAYS      FSDQ_ALWAYS
#define CDQ_NOICON      FSDQ_NOICON

#ifdef __cplusplus
}
#endif
#endif /* !__FSD_H__ */
