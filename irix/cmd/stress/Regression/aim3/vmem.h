/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	vmem_h
#	define vmem_h " @(#) vmem.h:3.2 5/30/92 20:19:03"
#endif

#ifndef SEGMENT
#define SEGMENT 268345456l
#endif
typedef struct  {
    char **seg_tbl;
    long n_elem;
    unsigned int el_siz;
} varray;

varray *vcalloc();
char *vaddr();
