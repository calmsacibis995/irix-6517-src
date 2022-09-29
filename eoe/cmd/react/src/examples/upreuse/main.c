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

#include <sys/types.h>
#include <sys/frs.h>
#include "pqueue.h"
#include "pdesc.h"
#include "sema.h"
#include "loop.h"
#include "work.h"

extern void test_hello(void);

main(int argc, char** argv)
{

        system_init();

        test_hello();

        printf("Single frs test 1.....\n");
        test_singlefrs(1, 200000, 4, 4, 100, WORKF_FIXEDLOOPS, 8, 0xFFFFFFFF);
        printf("Single frs test 2.....\n");
        test_singlefrs(1, 200000, 4, 4, 50000, WORKF_RANDLOOPS, 8, 0xFFFFFFFF);

        system_cleanup();

}

                
