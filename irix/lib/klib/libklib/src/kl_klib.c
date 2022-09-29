#ident "$Header: "

#include <sys/types.h>
#include <stdio.h>
#include <klib/klib.h>

/* Global variables that contain vmcore and symbol table information.
 * Initially, the value of MAX_KLIBS will be defined as 1. It should
 * be possible, however, to increase this value and allow the access
 * of more than one vmcore image/kernel by a single KLIB application.
 */
klib_t *klib_table[MAX_KLIBS];
int cur_klib = -1;

/*
 * init_klib()
 */
int
init_klib()
{
	if ((cur_klib + 1) >= MAX_KLIBS) {
		return(1);
	}
	cur_klib++;

	if (!(KLP = (klib_t *)malloc(sizeof(klib_t)))) {
		fprintf(stderr,"out of memory! malloc failed on %d bytes!",
			(int)sizeof(klib_t));
		return(-1);
	}
	bzero(KLP, sizeof(klib_t));
	return(cur_klib);
}

/*
 * set_klib_flags() 
 */
void
set_klib_flags(int flags)
{
    if (flags & K_IGNORE_FLAG) {
        K_FLAGS = (flags|K_IGNORE_MEMCHECK);
    }
    else {
        K_FLAGS = flags;
    }
}

/*
 * switch_klib()
 */
int
switch_klib(int index)
{
	if ((index < 0) || (index >= MAX_KLIBS) || !KLP) {
		return(1);
	}
	cur_klib = index;
	return(0);
}

/*
 * kl_free_klib()
 */
void
kl_free_klib(klib_t *klp)
{
	if (klp->k_libmem.k_data) {
		klp->k_libmem.k_free(klp->k_libmem.k_data);
	}
	if (klp->k_libsym.k_data) {
		klp->k_libsym.k_free(klp->k_libmem.k_data);
	}
	if (klp->k_libkern.k_data) {
		klp->k_libkern.k_free(klp->k_libkern.k_data);
	}
	free(klp);
}
