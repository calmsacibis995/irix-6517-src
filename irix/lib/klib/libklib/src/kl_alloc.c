#ident "$Header: "

#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <klib/klib.h>

klib_functions_t klib_functions;

/* The block alloc functions below are wrappers around the actual
 * functions from liballoc. If liballoc is not linked into an
 * application, these functions will still work. All of the functions 
 * will work (via calls to malloc() and free()) with the exception 
 * of kl_dup_block(), because there is no way to determine the size 
 * of a malloc'd block. Note that it is also possible to "register" 
 * alturnate functions to perform these tasks. They have to conform
 * to the function prototypes in the kl_functions.h header file.
 */

/*
 * kl_alloc_block()
 */
k_ptr_t 
#ifdef ALLOC_DEBUG
_kl_alloc_block(int size, int flags, void *ra) 
#else
kl_alloc_block(int size, int flags) 
#endif
{
	k_ptr_t blk;

	kl_reset_error();

	if (size == 0) {
		KL_SET_ERROR(KLE_ZERO_BLOCK);
		return((k_ptr_t)NULL);
	}
	if (KL_BLOCK_ALLOC()) {
#ifdef ALLOC_DEBUG
		blk = (k_ptr_t)KL_BLOCK_ALLOC()(size, flags, ra);
#else
		blk = (k_ptr_t)KL_BLOCK_ALLOC()(size, flags);
#endif
	}
	else {
		if (blk = (k_ptr_t)malloc(size)) {
			bzero(blk, size);
		}
	}
	if (!blk) {
		KL_SET_ERROR(KLE_NO_MEMORY);
	}
	return(blk);
}

/*
 * kl_realloc_block()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
_kl_realloc_block(k_ptr_t b, int new_size, int flags, void *ra)
#else
kl_realloc_block(k_ptr_t b, int new_size, int flags)
#endif
{
	k_ptr_t blk;

	kl_reset_error();

	if (!b || new_size == 0) {
		KL_SET_ERROR(KLE_ZERO_BLOCK);
		return((k_ptr_t)NULL);
	}

	if (KL_BLOCK_REALLOC()) {
#ifdef ALLOC_DEBUG
		blk = (k_ptr_t)KL_BLOCK_REALLOC()(b, new_size, flags, ra);
#else
		blk = (void *)KL_BLOCK_REALLOC()(b, new_size, flags);
#endif
	}
	else {
		if (blk = (k_ptr_t)realloc(b, new_size)) {
			bzero(blk, new_size);
		}
	}
	if (!blk) {
		KL_SET_ERROR(KLE_NO_MEMORY);
	}
	return(blk);
}

/*
 * kl_dup_block()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
_kl_dup_block(k_ptr_t b, int flags, void *ra)
#else
kl_dup_block(k_ptr_t b, int flags)
#endif
{
	k_ptr_t blk;

	kl_reset_error();

	if (!b) {
		KL_SET_ERROR(KLE_ZERO_BLOCK);
		return((k_ptr_t)NULL);
	}

	if (KL_BLOCK_DUP()) {
#ifdef ALLOC_DEBUG
		blk = (k_ptr_t)KL_BLOCK_DUP()(b, flags, ra);
#else
		blk = (void *)KL_BLOCK_DUP()(b, flags);
#endif
	}
	else {
		/* There is no way to determine the size of a block if it has
		 * been allocated via malloc(). Therefore return a NULL pointer
		 * (and an error).
		 */
		KL_SET_ERROR(KLE_NOT_SUPPORTED);
		return((k_ptr_t)NULL);
	}
	if (!blk) {
		KL_SET_ERROR(KLE_NO_MEMORY);
	}
	return(blk);
}

/*
 * kl_str_to_block()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
_kl_str_to_block(char *s, int flags, void *ra)
#else
kl_str_to_block(char *s, int flags)
#endif
{
	k_ptr_t blk = (k_ptr_t)NULL;

	kl_reset_error();

	if (!s) {
		KL_SET_ERROR(KLE_ZERO_BLOCK);
		return((k_ptr_t)NULL);
	}

	if (KL_STR_TO_BLOCK()) {
#ifdef ALLOC_DEBUG
		blk = (k_ptr_t)KL_STR_TO_BLOCK()(s, flags, ra);
#else
		blk = (void *)KL_STR_TO_BLOCK()(s, flags);
#endif
	}
	else {
		int size;

		if (size = strlen(s) + 1) {
			blk = kl_alloc_block(size, flags);
			bcopy(s, blk, size);
		}
	}
	if (!blk) {
		KL_SET_ERROR(KLE_NO_MEMORY);
	}
	return(blk);
}

/*
 * kl_free_block()
 */
void
kl_free_block(k_ptr_t blk)
{
	if (!blk) {
		return;
	}
	if (KL_BLOCK_FREE()) {
		KL_BLOCK_FREE()(blk);
	}
	else {
		free(blk);
	}
}

/*
 * kl_block_alloc_func()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
kl_block_alloc_func(int size, int flag, caddr_t *ra)
#else
kl_block_alloc_func(int size, int flag)
#endif
{
    k_ptr_t b;

#ifdef ALLOC_DEBUG
    b = _alloc_block(size, flag, ra);
#else
    b = alloc_block(size, flag);
#endif
    return(b);
}

/*
 * kl_block_realloc_func()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
kl_block_realloc_func(k_ptr_t blk, int size, int flag, caddr_t *ra)
#else
kl_block_realloc_func(k_ptr_t blk, int size, int flag)
#endif
{
    k_ptr_t b;

#ifdef ALLOC_DEBUG
    b = _realloc_block(blk, size, flag, ra);
#else
    b = realloc_block(blk, size, flag);
#endif
    return(b);
}

/*
 * kl_block_dup_func()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
kl_block_dup_func(k_ptr_t blk, int flag, caddr_t *ra)
#else
kl_block_dup_func(k_ptr_t blk, int flag)
#endif
{
    k_ptr_t b;

#ifdef ALLOC_DEBUG
    b = _dup_block(blk, flag, ra);
#else
    b = dup_block(blk, flag);
#endif
    return(b);
}

/*
 * kl_str_to_block_func()
 */
k_ptr_t
#ifdef ALLOC_DEBUG
kl_str_to_block_func(char * s, int flag, void *ra)
#else
kl_str_to_block_func(char * s, int flag)
#endif
{
    k_ptr_t b;

#ifdef ALLOC_DEBUG
    b = _str_to_block(s, flag, ra);
#else
    b = str_to_block(s, flag);
#endif
    return(b);
}

/*
 * kl_block_free_func()
 */
void
kl_block_free_func(k_ptr_t blk)
{
    free_block(blk);
}

/*
 * kl_init_mempool()
 *
 * Provides a wrapper around the init_mempool() function from liballoc.
 */
void
kl_init_mempool(int pghdr_cnt, int blkhdr_cnt, int page_cnt)
{
	init_mempool(pghdr_cnt, blkhdr_cnt, page_cnt);
	KL_BLOCK_ALLOC() = kl_block_alloc_func;
	KL_BLOCK_REALLOC() = kl_block_realloc_func;
	KL_BLOCK_DUP() = kl_block_dup_func;
	KL_STR_TO_BLOCK() = kl_str_to_block_func;
	KL_BLOCK_FREE() = kl_block_free_func;
}

/*
 * kl_free_mempool()
 */
void
kl_free_mempool()
{
	free_mempool();
}
