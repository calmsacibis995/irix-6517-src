/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifdef __STDC__
	#pragma weak cache_synch_handler = _cache_synch_handler
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/cachectl.h>
#include <sys/syssgi.h>

#define CACHE_SYNCH_BUFFER_SIZE (40)
cache_synch_handler_t _cache_synch_routine = NULL;
inst_t _cache_synch_buffer[CACHE_SYNCH_BUFFER_SIZE];

static int cache_synch_default(void *a, int b)
{
    return(cacheflush(a,b,BCACHE));
}

cache_synch_handler_t
cache_synch_handler(void)
{
    if (_cache_synch_routine == NULL) {
        if (syssgi(SGI_SYNCH_CACHE_HANDLER,
                _cache_synch_buffer,
                sizeof(_cache_synch_buffer)) != 0) {
            /* error of some sort */
            _cache_synch_routine = cache_synch_default;
        } else
            _cache_synch_routine =
                ((cache_synch_handler_t)
                     _cache_synch_buffer);
            /*
             * assumes that the system call above
             * synchronizes the buffer
             */
    }
    return(_cache_synch_routine);
}
