/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.15 $"

/*
 * Test routine for the xlv tab routines that copy data in/out of
 * kernel mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/open.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_utils.h>
#include "xlv_procs.h"

extern void xlv_io_done (void);

void drv_test (void) {

#define MAX_VOLS 100
#define MAX_SUBVOLS 3*MAX_VOLS

    xlv_tab_vol_t       *k_xlv_tab_vol = NULL;
    xlv_tab_t           *k_xlv_tab = NULL;
    unsigned		max_top_bufs, max_low_bufs;
    int			status;

    status = xlv_tab_copyin (xlv_tab_vol, xlv_tab,
	&k_xlv_tab_vol, &k_xlv_tab, &max_top_bufs, &max_low_bufs, 0);

    fprintf (stdout, "\n");
    fprintf (stdout, "Copied xlvtab: \n");
    xlv_tab_print (k_xlv_tab_vol, k_xlv_tab);

    fprintf (stdout, " reserved %d top buffers, %d low buffers\n",
		    max_top_bufs, max_low_bufs);

    status = xlv_tab_set (k_xlv_tab_vol, k_xlv_tab, 0);
    fprintf (stdout, "\nCopied 2nd xlvtab: \n");
    xlv_tab_print (xlv_tab_vol, xlv_tab);

    fprintf (stdout, " reserved %d top buffers, %d low buffers\n",
		    max_top_bufs, max_low_bufs);

}


/* Stubs */

void *xlv_top_strat_mem = NULL;
void *xlv_low_strat_mem = NULL;
unsigned xlv_top_memsize;
unsigned xlv_low_memsize;
sema_t xlv_top_memsem, xlv_low_memsem;

#include <sys/types.h>
#include <bstring.h>
#include <sys/kmem.h>

int copyin (caddr_t userbuf, caddr_t driverbuf, int cnt) {

    bcopy (userbuf, driverbuf, cnt);
    return (0);

}

#ifdef kmem_free
#else

void kmem_free (void *cp, size_t size) {
    free (cp);
}
#endif

#ifdef kmem_alloc
#else
void *kmem_alloc (size_t size, int flag) {
    return (malloc (size));
}
#endif

#ifdef kmem_zalloc
#else
void *kmem_zalloc (size_t size, int flag) {
    void *p;

    p = malloc (size);
    bzero (p, size);
}
#endif

int kmem_do_fill = 0;

void    kmem_fill(char *c, int d, void *e, size_t f)
{
}
void    _kmem_free(void *p, size_t s)
{
        free (p);
}


int xlv_io_lock;
int xlv_config_lock;
int xlv_maxlocks = 50;
void mrlock() {
}
void mrunlock () {
}
int cmrlock () {
	return 1;
}

/* Stubs required by xlv_merge.c */
int MAJOR (int dev) {
	return (1);
}
void binval () {
}
void bdrv () {
}
void bdhold () {
}
void bdrele () {
}

/* Stubs required by xlv_ktab.o */
int ismrlocked (void *mrp, int type) {
	return (1);
}

int psema() {
	return 0;
}

int vsema() {
	return 0;
}

int cap_enabled () {
	return 0;
}

int cap_able () {
	return 1;
}

int suser () {
	return 1;
}

void xlv_start_daemons() {
}

void xlv_critical_region_init (xlv_tab_subvol_t *xlv_p) {
}
