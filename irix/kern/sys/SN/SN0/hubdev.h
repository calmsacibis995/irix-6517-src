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

#ifndef __SYS_SN_SN0_HUBDEV_H__
#define __SYS_SN_SN0_HUBDEV_H__

extern void hubdev_init(void);
extern void hubdev_register(int (*attach_method)(dev_t));
extern int hubdev_unregister(int (*attach_method)(dev_t));
extern int hubdev_docallouts(dev_t hub);

extern caddr_t hubdev_prombase_get(dev_t hub);
extern cnodeid_t hubdev_cnodeid_get(dev_t hub);

#endif /* __SYS_SN_SN0_HUBDEV_H__ */
