#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/RCS/callbacks.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <filehdr.h>
#include "icrash.h"
#include "extern.h"

/* This module contains callback functions for libklib. They are added 
 * to the klib_t struct at startup. They allow the libklib library 
 * routines to get symbol addresses, determine member offsets, sizes, 
 * etc. They are essentially wrappers for the icrash routines that 
 * actually provides access to symbol related information.
 */

/* sym_addr_func()
 */
int
sym_addr_func(void *ptr, char *name, kaddr_t *addr)
{
	struct syment *sp;

	if (sp = get_sym(name, 0)) {
		*addr = sp->n_value;
		free_sym(sp);
		return(0);
	}
	return(-1);
}

/*
 * struct_len_func()
 */
int
struct_len_func(void *ptr, char *s, int *length)
{
	if (*length = STRUCT(s)) {
		return(0);
	}
	return(-1);
}

/*
 * member_offset_func()
 */
int
member_offset_func(void *ptr, char *s, char *m, int *offset)
{
	if ((*offset = FIELD(s, m)) == -1) {
		return(-1);
	}
	return(0);
}

/*
 * member_size_func()
 */
int
member_size_func(void *ptr, char *s, char *m, int *length)
{
	int nbytes;

	if (nbytes = FIELD_SZ(s, m)) {
		*length = nbytes;
		return(0);
	}
	return(-1);
}

/*
 * member_bitlen_func()
 */
int
member_bitlen_func(void *ptr, char *s, char *m, int *length)
{
	return(-1);
}

/*
 * member_base_value_func()
 */
int
member_base_value_func(void *ptr, char *s, char *m, k_uint_t *value)
{
	int ret;
	k_uint_t v;

	if (base_value(ptr, s, m, &v) == -1) {
		return(-1);
	}
	*value = v;
	return(0);
}

/*
 * block_alloc_func()
 */
k_ptr_t
block_alloc_func(void *ptr, int size, int flag)
{
	k_ptr_t b;

	b = alloc_block(size, flag);
	return(b);
}

/*
 * block_alloc_func()
 */
void
block_free_func(void *ptr, k_ptr_t blk)
{
	free_block(blk);
}

/*
 * add_callbacks()
 */
int
add_callbacks(klib_t *klp)
{
	int ret;

	ret = klib_add_callbacks(
			klp, 
			(k_ptr_t)NULL, 
			(klib_sym_addr_func) sym_addr_func,
			(klib_struct_len_func) struct_len_func, 
			(klib_member_offset_func) member_offset_func, 
			(klib_member_size_func) member_size_func,
			(klib_member_bitlen_func) member_bitlen_func,
			(klib_member_base_value_func) member_base_value_func,
			(klib_block_alloc_func) block_alloc_func,
			(klib_block_free_func) block_free_func,
			(klib_print_error_func) print_error);
	return(ret);
}

