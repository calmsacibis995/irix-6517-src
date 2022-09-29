/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"

/*
 * Prototypes for user mode scaffolding for xlv driver.
 */
extern void xlv_io_done (void);
extern void drv_test (void);

struct buf;
extern void xlv_bdstrat (int major, struct buf *buf);
struct cred;
extern int xlv_bdopen (int major, dev_t *dev, int flag, int type ,
        struct cred *cred);
extern int xlv_bdclose (int major, dev_t dev, int flag, int type,
        struct cred *cred);
extern int xlvstrategy (struct buf *buf);
extern int xlvopen(dev_t *devp, int flag, int otype, struct cred *cred);
extern int xlvclose(dev_t dev, int flag, int otype, struct cred *cred);


