/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*
 * Maximum clients for semaphores
 */
#define MAXPROCS 64

/*
 * Maximum number of elements in a queue
 */
#define QMAXELEM   64

void system_init(void);
void system_cleanup(void);



#endif /* __CONFIG_H__ */
