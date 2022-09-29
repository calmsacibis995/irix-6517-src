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

#include "pdesc.h"

void
pdesc_dispatcher(void* args)
{
        pdesc_t* pdesc = (pdesc_t*)args;

        assert(pdesc != NULL);
        assert(pdesc->opcode == PDESCOP_INIT);
        
        while (1) {
#if DEBUG > 3                
                printf("[pdesc_dispatcher]: Waiting for go...\n");
#endif                
                sema_p(pdesc->ssema_go);
#if DEBUG > 3                
                printf("[pdesc_dispatcher]: Received go, code: %d\n", pdesc->opcode);
#endif                
                switch (pdesc->opcode) {
                    case PDESCOP_INIT:
                            pdesc->pid = getpid();
                            sema_v(pdesc->ssema_done);    
                            break;
                    case PDESCOP_EXIT:
                            sema_v(pdesc->ssema_done);    
                            exit(0);
                    case PDESCOP_RUN:
                            assert(pdesc->function != NULL);
                            (*pdesc->function)(pdesc->args);
                            if (pdesc->flags | PDESCFLAGS_SIGNALDONE) {
                                    sema_v(pdesc->ssema_done);
                            }   
                            break;
                    default:
                            fprintf(stderr, "[pdesc_dispatcher]: Invalid Opcode\n");
                            break;
                }
        }
}
                           
                            

pdesc_t*
pdesc_create(void)
{
        pdesc_t* pdesc;
        pid_t pid;

        if ((pdesc = (pdesc_t*)malloc(sizeof(pdesc_t))) == NULL) {
                return (NULL);
        }

        pdesc->opcode = PDESCOP_INIT;
        pdesc->flags = 0;
        pdesc->args = 0;
        pdesc->function = 0;

        if ((pdesc->ssema_go = sema_create()) == NULL) {
                free(pdesc);
                return (NULL);
        }

        if ((pdesc->ssema_done = sema_create()) == NULL) {
                free(pdesc);
                sema_destroy(pdesc->ssema_go);
                return (NULL);
        }

        if ((pid = sproc(pdesc_dispatcher, PR_SALL, (void*)pdesc)) < 0) {
                sema_destroy(pdesc->ssema_go);
                free(pdesc);
                return (NULL);
        }

        /*
         * Run once to execute initialization
         */

        sema_v(pdesc->ssema_go);
        sema_p(pdesc->ssema_done);

        return (pdesc);
}

void
pdesc_destroy(pdesc_t* pdesc)
{
        assert(pdesc != 0);

        
        pdesc->opcode = PDESCOP_EXIT;
        pdesc->flags = PDESCFLAGS_SIGNALDONE;
        sema_v(pdesc->ssema_go);
        sema_p(pdesc->ssema_done);

        /*
         * The process is gone;
         * we can safely get rid of the pdesc structure
         */
        
        sema_destroy(pdesc->ssema_go);
        sema_destroy(pdesc->ssema_done);
        pdesc->pid = 0;
        pdesc->opcode = 0;
        pdesc->flags = 0;
        pdesc->args = 0;
        pdesc->function = 0;
        free(pdesc);
}

void
pdesc_syncrun_function(pdesc_t* pdesc, void (*function)(void*), void* args)
{
        assert(pdesc != 0);
        assert(function != 0);

        pdesc->opcode = PDESCOP_RUN;
        pdesc->args = args;
        pdesc->function = function;
        pdesc->flags = PDESCFLAGS_SIGNALDONE;

        sema_v(pdesc->ssema_go);
        sema_p(pdesc->ssema_done);
}

void
pdesc_asyncrun_function(pdesc_t* pdesc, void (*function)(void*), void* args)
{
        assert(pdesc != 0);
        assert(function != 0);

        pdesc->opcode = PDESCOP_RUN;
        pdesc->args = args;
        pdesc->function = function;
        pdesc->flags = 0;

        sema_v(pdesc->ssema_go);
}
        


/*************************************************************
 *                  pdesc_pool                               *
 *************************************************************/

queue_t*
pdesc_pool_create()
{
        return (queue_create());
}

int
pdesc_pool_put(queue_t* q, pdesc_t* pdesc)
{
        qelem_t* qelem;

        if ((qelem = (qelem_t*)malloc(sizeof(qelem_t))) == NULL) {
                return (-1);
        }

        qelem->pdesc = pdesc;
        qelem->fp = 0;
        qelem->bp = 0;

        queue_enqueue(q, qelem);

        return (0);
}

pdesc_t*
pdesc_pool_get(queue_t* q)
{
        qelem_t* qelem;
        pdesc_t* pdesc;

        qelem = queue_dequeue(q);
        assert(qelem != 0);
        qelem->fp = 0;
        qelem->bp = 0;
        pdesc = qelem->pdesc;
        qelem->pdesc = 0;

        free(qelem);

        assert(pdesc != 0);
        return (pdesc);
}

void
pdesc_pool_destroy(queue_t* q)
{
        /*
         * calls pdesc_destroy, frees element holders,
         * and frees headers -- a bargain! (calls pdesc_destroy)
         */
        queue_destroy(q);
}
