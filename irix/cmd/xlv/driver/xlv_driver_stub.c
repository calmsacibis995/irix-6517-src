/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.21 $"

#include <stdlib.h>
#define _KERNEL
#include <sys/types.h>
#undef _KERNEL
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <bstring.h>
#include <stdio.h>
#include <sys/xlv_base.h>
#include <sys/major.h>


/* 
 Stubs needed to resolve undefine's from xlv.c (the driver).
 */

int xlvmajor = XLV_MAJOR;
int xlv_maxunits = 20;
int xlv_io_lock;	/* Actually should be an array of locks,
			   but this should be enough to fake out
			   the linker. */
int xlv_config_lock;	/* ditto. */
int xlv_maxlocks = 30;



int bdhold (int major_num) {
	return (0);
}


void bdrv (int major_num) {
	;
}

bdrele (int major_num) {
	;
}

binval (int dev) {
	;
}


void bp_mapin (char *buf) {
	;
}

extern void xlv_top_strat_done (buf_t *buf);

void iodone (buf_t *buf) {
	if (emajor(buf->b_edev) == XLV_LOWER_MAJOR) {
		printf ("Completing upper strat buffer: %x\n", buf);
		xlv_top_strat_done (buf);
	}
	else {
		printf ("Original buffer %x completed\n", buf);
	}
}

uiophysck (void) {
	;
}

uiophysio (void) {
	;
}

void bp_mapout (char *buf) {
	;
}

kmem_free (void *p, int i) {
	free (p);
}

void *kmem_zalloc (size_t size, int sleep) {
	void *p;
	p = malloc(size);
	bzero (p, size);
	return (p);
}

int kmem_do_fill = 0;

void kmem_fill () {}

_kmem_free (void *p, int i) {
	free (p);
}



/*
 * We store all the buf's that have been passed to strategy.
 * We will then simulate their completion later when the test
 * program calls xlv_io_done.
 */
int		issued_buf_count = 0;
struct buf	*issued_bufs [1000];

/*
 * Keep track of the dma transfer address so that we
 * can sanity check it against blockno's etc.
 *
 * Note that there will be differences every time we get a
 * new user I/O request.  (which presumably starts off at
 * a different address.
 */
unsigned long	expected_caddr = 0;

void xlv_bdstrat (int major, struct buf *buf) {

        /* Check to make sure that the dmaaddr is advancing as it should */
        if (((unsigned long) buf->b_dmaaddr) != expected_caddr) {
                printf ("---- dmaadr not going up consistently ----\n");
        }
        /* Assume that they will go up in sequence. */
        expected_caddr = ((unsigned long) buf->b_dmaaddr) +
                         buf->b_bcount;


	printf ("bdstrat:  b_flags: %x, b_edev: %x (maj: %d, minor: %d)\n",
		buf->b_flags, buf->b_edev, major(buf->b_edev),
		minor(buf->b_edev));
	printf ("          b_blkno: %d, b_bcount: %d (%d blocks), addr: %d\n",
		buf->b_blkno, buf->b_bcount, buf->b_bcount / 512,
		buf->b_dmaaddr);

	/* Save this buffer away so we can mark it completed later */
	issued_bufs [issued_buf_count++] = buf;

}

/* 
 * Test program should call this after all the I/O have been issued
 * to trigger completion.  This will call back the driver's xlv_done()
 * entrypint.
 */
extern void xlv_low_strat_done(register struct buf *buf);

void xlv_io_done (void) {
	int i;

	/* Complete the buffers in reverse order just for yucks. */
	while (--issued_buf_count >= 0) {
		xlv_low_strat_done(issued_bufs[issued_buf_count]);
	}

	issued_buf_count = 0;
}

int xlv_bdopen (int major, dev_t *dev, int flag, int type , char *cred) {
	printf ("bdopen:  dev: %x, (maj: %d, minor: %d)\n",
		*dev, major (*dev), minor (*dev));
	return (0);
}

int xlv_bdclose (int major, dev_t dev, int flag, int type, char *cred) {
	printf ("bdclose: dev: %x, (maj: %d, minor: %d)\n",
		dev, major(dev), minor(dev));
	return (0);
}


void initnlock (lock_t *lock)
{
}

int splockspl(lock_t lock, int (*func)(void)) {
	return (0);
}

int splhi (void) {
	return (16);
}

void spunlockspl(lock_t lock, int val) {
}

int copyout(void *src,void *dst, int sz) {
    return (-1);
}

int copyin(void *src,void *dst, int sz) {
    return (-1);
}

unsigned xlv_tab_subvol_trksize (void *sv) {
    return (-1);
}


void mrinit () {
}

void mrlock () {
}

void mrunlock () {
}

void mrfree () {
}

void splx(int i) {
}

void vsema () {
}

/*
 * Signal that the semaphore is busy.
 * This is currently used only by the xlv buffer memory allocation
 * routines.
 */
int cpsema () {
	return 0;
}

void psema () {
}

void initnsema () {
}

/*
 * We don't handle user mode errors. So we stub these out.
 */
int xlvd_work_queue;
void bp_enqueue () {
}

void xlvd_queue_request() {
}
