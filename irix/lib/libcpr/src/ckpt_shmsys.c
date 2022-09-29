/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.        	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#include <sys.s>
#include <sys/types.h>
#include <stddef.h>

#define SHMSYS SYS_shmsys
#define SHMGETID 5

extern ptrdiff_t syscall(int , ...);
int
shmget_id(key_t key, size_t size, int shmflg, int id)
{
        return((int)syscall(SHMSYS, SHMGETID, key, size, shmflg, id));
}

