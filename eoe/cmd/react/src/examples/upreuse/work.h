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

#ifndef __WORK_H__
#define __WORK_H__

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>

typedef struct workf_args {
        uint iloops;  /* internal loops */
        uint mcalls;  /* max number of calls */
        uint ccalls;  /* current number of calls */
        uint randloops; /* choose random number of internal loops */
} workf_args_t;

#define WORKF_CODE_MAXCALLS  1

#define WORKF_FIXEDLOOPS 0
#define WORKF_RANDLOOPS  1

int example_work_function(void* args) ;     
workf_args_t* workf_args_create(uint iloops, uint randloops, uint mcalls);
void workf_args_destroy(workf_args_t* workf_args);


#endif /* __WORK_H__ */
