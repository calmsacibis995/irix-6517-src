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

#include "work.h"

int
example_work_function(void* args)
{
        workf_args_t* workf_args = (workf_args_t*)args;

        assert(workf_args != 0);

        if (++workf_args->ccalls > workf_args->mcalls) {
                return (WORKF_CODE_MAXCALLS);
        } else {
                volatile int i;
                volatile double res = 2;
                long workloops;
                if (workf_args->randloops == WORKF_RANDLOOPS) {
                        workloops = random() %  workf_args->iloops;
                } else {
                        workloops = workf_args->iloops;
                }
                for (i = 0; i < workloops; i++) {
                        res = res * log(res) - res * sqrt(res);
                }
                return (0);
        }
}

workf_args_t*
workf_args_create(uint iloops, uint randloops, uint mcalls)
{
    workf_args_t* workf_args;

    if ((workf_args = (workf_args_t*)malloc(sizeof(workf_args_t))) == NULL) {
            return (NULL);
    }

    workf_args->iloops = iloops;
    workf_args->randloops = randloops;
    workf_args->mcalls = mcalls;
    workf_args->ccalls = 0;
}

void
workf_args_destroy(workf_args_t* workf_args)
{
        assert(workf_args != 0);
        
        free(workf_args);
}


        
