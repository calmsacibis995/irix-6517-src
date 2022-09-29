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
#ident "$Revision: 1.12 $"

/*
 * Populate an xlv_tab and then invoke the XLV driver (in user mode).
 *
 * When run on bitbucket, input file: bit.conf.
 * Sample output is in drv_test.out.
 */

#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>

#define _KERNEL
#include <sys/types.h>
#undef _KERNEL

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
#include <sys/xlv_tab.h>

#include "drv_test.h"

void init_xlv_buffers(void);

void drv_test (void) {
    xlv_tab_vol_t	*xlv_tab_vol;
    xlv_tab_t		*xlv_tab;
    int			status;
    char		*subvol_name;
    struct stat		stat_buf;
    struct buf		*bp;
    dev_t		dev;

#define MAX_VOLS 100
#define MAX_SUBVOLS 3*MAX_VOLS

    /***************** NOTE: ******************
	mkxlv puts subvolumes starting at 0, 1, etc.
	xlv_assemble() makes them sparse, indexed by
	minor dev number.  To make it work out, we've
	made the 2 first subvolumes 0, 1.
     ******************************************/

    /* Get the device numbers for the logical volume */
    if (stat ("/dev/dsk/bit_data", &stat_buf)) {
        printf ("Sub-volume does not exist!\n");
        return ;
    }

    dev = stat_buf.st_rdev;

    status = xlvopen (&dev, 0, OTYP_BLK, NULL);
    if (status != 0) {
	printf ("xlvopen failed\n");
	return ;
    }

    init_xlv_buffers();

/***** striping test *****
 Write over entire stripe group.
 Start = 4128, end = 6879 */

    bp = (struct buf *) malloc (sizeof(struct buf));
    bzero (bp, sizeof(struct buf));
    bp->b_flags = B_READ;
    bp->b_edev = dev;
    bp->b_blkno = 4128;         /* starting at block
                                1000 from start of VE. */
    bp->b_bcount = (6879-4128+1) * 512;

    xlvstrategy (bp);

    xlv_io_done ();

/*****  striping test *****/
/* volume element stripe2 starts at block 6880 to 17,887.
   the stripe-width is 94 blocks.

   Each partition has 2752 blocks.

   We start off in the 2nd partition and transfer 6 stripe-width
   chunks of data, not aligned. 

   disk 0
   disk 1		4 disks in stripe
   disk 2
   disk 3

   The stripe (stripe2) begins at blockno 6880.
   So first transfer should be on disk 2. */

    bp = (struct buf *) malloc (sizeof(struct buf));
    bzero (bp, sizeof(struct buf));
    bp->b_flags = B_READ | B_ASYNC;
    bp->b_edev = dev;
    bp->b_blkno = 6880+1000;         /* starting at block 
				1000 from start of VE. */
    bp->b_bcount = 6*94* 512;

    xlvstrategy (bp);

#if 0
    xlv_io_done ();
#endif

/*****  concatenation test *****/
/* Each partition has 1376 blocks == 704,512 bytes.
   We want it to overlap.  So we'll start at block 1000,
   and ask to transfer 1376*2, this should result in going across
   3 disk parts (concatenated). 

   Should transfer 376, 1376, 1000 blocks into successive disks.

*/

    bp = (struct buf *) malloc (sizeof(struct buf));
    bzero (bp, sizeof(struct buf));
    bp->b_flags = B_WRITE | B_ASYNC;
    bp->b_edev = dev;
    bp->b_blkno = 1000;		/* starting at block 0 */
    bp->b_bcount = (2*1376) * 512;	/* bytes */

    xlvstrategy (bp);

    xlv_io_done ();

/**** Test of concatenation & striping combined ****/
/*
 * We will start in data3 and go across all of stripe1 and
 * extend into stripe2.
 */
    bp = (struct buf *) malloc (sizeof(struct buf));
    bzero (bp, sizeof(struct buf));
    bp->b_flags = B_WRITE | B_ASYNC;
    bp->b_edev = dev;
    bp->b_blkno = 4100; 	/* start towards end of data3 */
    bp->b_bcount = ((6879-4128) + 2000) * 512;      /* bytes */
		/* This puts some in data3, overlaps all of
		   stripe1, and extends into stripe2. */

    xlvstrategy (bp);
    xlv_io_done ();

    status = xlvclose (dev, 0, OTYP_BLK, NULL);
    if (status != 0) {
	printf ("xlvclose failed\n");
	return ;
    }
}



/*
 * We play games here because we don't export xlvmem_t to user space.
 */
extern void         *xlv_top_strat_mem, *xlv_low_strat_mem;
extern int          xlv_top_memsize, xlv_low_memsize;

buf_t		top_strat_mem[3000];
buf_t		low_strat_mem[3000];

/*
 * Note that there is only one buffer in the memory block.
 * So we pad out the remaining 99 elements separately.
 */
void init_xlv_buffers(void) {
	xlv_top_strat_mem = &top_strat_mem;
	xlv_low_strat_mem = &low_strat_mem;
	xlv_top_memsize = 2000;
	xlv_low_memsize = 2000;
}

/* disk map: in block numbers
 
data1:   [0      1375]
data2:   [1376   2751]
data3:   [2752   4127]
stripe1: [4128       ][       6879]
stripe2: [6880       ][           ][           ][      17887]

 */
