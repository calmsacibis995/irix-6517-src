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

#define NUMSPROCS 10

static void
print_hello(void *args)
{
        char *s = (char*)args;
        printf("[%s]: Hello World\n", s);
}

void
test_hello(void)
{
        queue_t* pdesc_pool;
        pdesc_t* pdesc;
        int i;
        
        /*
         * Create a pool of sprocs
         */

        if ((pdesc_pool = pdesc_pool_create()) == NULL) {
                perror("pdesc_pool_create");
                exit(1);
        }
        
        for (i = 0; i < NUMSPROCS; i++) {
                pdesc = pdesc_create();
                if (pdesc_pool_put(pdesc_pool, pdesc) < 0) {
                        perror("pdesc_pool_put");
                        exit(1);
                }
        }

        
        /*
         * Make each one print hello world 
         */

        for (i = 0; i < NUMSPROCS; i++) {
                char* s;
                if ((pdesc = pdesc_pool_get(pdesc_pool)) == NULL) {
                        perror("pdesc_pool_get");
                        exit(1);
                }
                s = (char*)malloc(64);
                sprintf(s, "SPROC(%d)", i);
                pdesc_syncrun_function(pdesc, print_hello, (void*)s);
                free(s);
                if (pdesc_pool_put(pdesc_pool, pdesc) < 0) {
                        perror("pdesc_pool_put");
                        exit(1);
                }
                
        }

        
        /*
         * Destroy them
         */

        pdesc_pool_destroy(pdesc_pool);
}

                
