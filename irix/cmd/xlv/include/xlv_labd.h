#ifndef __XLV_LABD_H_
#define __XLV_LABD_H_

/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1996, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#define	PATH_XLV_LABD	"/sbin/xlv_labd"

/*
 * FIFO used for determining existence.
 */
#define XLV_LABD_DIR	"/etc/"

#define XLV_LABD_FIFO	XLV_LABD_DIR ".xlv_labd_fifo"
#define XLV_LABD_LOG	XLV_LABD_DIR "xlv_labd.log"

#endif /* __XLV_LABD_H_ */
