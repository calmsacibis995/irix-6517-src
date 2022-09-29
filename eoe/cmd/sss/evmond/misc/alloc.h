#ifndef _ALLOC_H_
#define _ALLOC_H_

#define KLIB_LIBRARY 1

#include <pthread.h>

#if KLIB_LIBRARY
#define ALLOC_DEBUG
#include "klib/alloc.h"
#else
#define B_TEMP  1
#define B_PERM  2
#define alloc_block(X,Y)       calloc(X,1)
#define free_block(X)          free(X)
#define realloc_block(X,Y,Z)   realloc(X,Y)
#define init_mempool()
#endif

#ifdef ALLOC_DEBUG
#define MEM_ALLOC_PERM(X)            _alloc_block(X,B_PERM,get_ra())
#define MEM_ALLOC_TEMP(X)            _alloc_block(X,B_TEMP,get_ra())
#define MEM_REALLOC_TEMP(X,size)     _realloc_block(X,size,B_TEMP,get_ra())
#define MEM_REALLOC_PERM(X,size)     _realloc_block(X,size,B_PERM,get_ra())
#else
#define MEM_ALLOC_PERM(X)            alloc_block(X,B_PERM)
#define MEM_ALLOC_TEMP(X)            alloc_block(X,B_TEMP)
#define MEM_REALLOC_TEMP(X,size)     realloc_block(X,size,B_TEMP)
#define MEM_REALLOC_PERM(X,size)     realloc_block(X,size,B_PERM)
#endif
#define MEM_ALLOC_FREE(X)            free_block(X)


#endif /* _ALLOC_H_ */
