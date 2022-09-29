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

#ifndef __PDESC_H__
#define __PDESC_H__

/*
 * Pdesc definition
 */

#include <sys/types.h>
#include "assert.h"
#include "pqueue.h"
#include "sema.h"
#include "pdescdef.h"



/*
 * Opcodes
 */

#define PDESCOP_EXIT   0      /* terminate the sproc */
#define PDESCOP_RUN    1      /* call the function pointed to by `function' */
#define PDESCOP_INIT   2      /* initialize pdesc */


/*
 * Flags
 */

#define PDESCFLAGS_SIGNALDONE 0x0001    /* Do a v on the done sema after action */


pdesc_t* pdesc_create(void);
void pdesc_syncrun_function(pdesc_t* pdesc, void (*function)(void*), void* args);
void pdesc_asyncrun_function(pdesc_t* pdesc, void (*function)(void*), void* args);

queue_t* pdesc_pool_create();
int pdesc_pool_put(queue_t* q, pdesc_t* pdesc);
pdesc_t* pdesc_pool_get(queue_t* q);
void pdesc_pool_destroy(queue_t* q);


#endif /* __PDESC_H__ */
