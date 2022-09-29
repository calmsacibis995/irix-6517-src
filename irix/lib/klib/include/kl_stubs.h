#ident "$Header: "

/* The following stub functions are provided to address the fact that one
 * KLIB library included in an application may contain calls to another
 * KLIB library that is not included. For example, the mlinfo portion of
 * the libsym library makes a number of calls to kl_kaddr() to look up 
 * kernel addresses in a vmcore image. If libsym is loaded without libkern, 
 * then there will not be a vmcore image to look at (at least from a 
 * kernel point of view). These alternate functions are provided so that 
 * application builds will not fail.
 *
 * Certain macros determine whether or not stub functions for a particular
 * KLIB library will be included in an application. Currently, there
 * are two defines controlling blocks of stub functions:
 *
 * LIBALLOC_STUBS 
 *
 *   When set, stub functions for the liballoc library will be included. 
 *   To be used when an application does not use the liballoc library 
 *   (functions such as kl_alloc_block() will still work, only they will 
 *   use malloc() etc. instead of alloc_block()).
 *
 * LIBKERN_STUBS 
 *
 *   When set, stub functions for the libkern library will be included. 
 *   To be used when an application uses libsym library but not the 
 *   libkern library.
 *
 * For example, if an application is using some of the KLIB libraries,
 * but not the liballoc library, then the following lines should be
 * included in an application source file:
 *
 * #include <klib/klib.h>
 * #define LIBALLOC_STUBS
 * #include <klib/kl_stubs.h>
 * #undef LIBALLOC_STUBS
 *
 * One BIG Note: Any application making use of these functions must ensure
 * that they NEVER get called when the dependent library is not linked
 * in (or they must deal with the obvious errors that will result).
 */

/**
 ** Stubs for functions in the liballoc library
 **
 **/
#ifdef LIBALLOC_STUBS
k_ptr_t
#ifdef ALLOC_DEBUG
_alloc_block(int size, int flag, void *ra)
#else
alloc_block(int size, int flag)
#endif
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((k_ptr_t)NULL);
}

k_ptr_t
#ifdef ALLOC_DEBUG
_realloc_block(k_ptr_t blk, int size, int flag, void *ra)
#else
realloc_block(k_ptr_t blk, int size, int flag)
#endif
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((k_ptr_t)NULL);
}

k_ptr_t
#ifdef ALLOC_DEBUG
_dup_block(k_ptr_t blk, int flag, void *ra)
#else
dup_block(k_ptr_t blk, int flag)
#endif
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((k_ptr_t)NULL);
}

k_ptr_t
#ifdef ALLOC_DEBUG
_str_to_block(char * s, int flag, void *ra)
#else
str_to_block(char * s, int flag)
#endif
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((k_ptr_t)NULL);
}

void
free_block(void *block)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return;
}

int
init_mempool(int pghdr_cnt, int blkhdr_cnt, int page_cnt)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return(0);
}
#endif


#ifdef LIBKERN_STUBS
/**
 ** Stubs for functions in the libkern library
 **
 **/
kaddr_t
kl_kaddr(k_ptr_t p, char *s, char *m)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((kaddr_t)NULL);
}

k_int_t
kl_int(k_ptr_t p, char *s, char *m, unsigned offset)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((k_int_t)NULL);
}

kaddr_t
kl_kaddr_to_ptr(kaddr_t k)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((kaddr_t)NULL);
}

kaddr_t
kl_kaddr_val(k_ptr_t p)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return((kaddr_t)NULL);
}

k_uint_t
kl_get_block(kaddr_t addr, unsigned size, k_ptr_t bp, char *name)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return(KLE_NO_FUNCTION);
}

k_uint_t
kl_get_struct(kaddr_t addr, int size, k_ptr_t bp, char *name)
{
	KL_SET_ERROR(KLE_NO_FUNCTION);
	return(KLE_NO_FUNCTION);
}
#endif
