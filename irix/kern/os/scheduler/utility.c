/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.26 $"

#include "sys/types.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/pda.h"
#include "sys/schedctl.h"
#include "sys/runq.h"
#include "sys/space.h"
#include "sys/kmem.h"
#include "sys/rt.h"
#include "sys/cmn_err.h"

unsigned usecperclk;
unsigned clkperusec;
int	dispseq;

zone_t *job_zone_private;
zone_t *job_zone_public;

extern int fasthz;
extern int nproc;

/*
 * schedinit - initialize scheduling data structures
 *
 *	Called early in main() through the einit_tbl.
 */
void
schedinit(void)
{
	job_t *job;
	makeRunq();
	job_zone_private = kmem_zone_private(sizeof(job_t), "Job-Private");
	kmem_zone_private_mode_noalloc(job_zone_private);
	job_zone_public = kmem_zone_init(sizeof(job_t), "Job-Public");
	/* 
	 * Adding job for p0
	 */
	job = kmem_zone_alloc(job_zone_public, KM_NOSLEEP);
	if (job == 0)
		cmn_err_tag(1788,CE_PANIC,"Could not allocate job for proc 0");
	kmem_zone_free(job_zone_private, job);
	wtree_init();
	init_batchsys();
	init_cpus();
	init_rt();
	init_gang();
}

void
force_resched(void)
{
	private.p_runrun = 1;
}

extern int slice_size;

int
_disp_sanity(void)
{
	if (slice_size == 0)
#if !defined(MP) || defined(IP30)
		slice_size = 2;
#else
		slice_size = 10;
#endif
	return 0;
}
