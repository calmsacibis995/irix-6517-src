/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop filter set management.
 */
#include "debug.h"
#include "snooper.h"

SnoopFilter *
sfs_allocfilter(struct sfset *sfs)
{
	SnoopFilter *sf;

	if (sfs->sfs_elements >= SNOOP_MAXFILTERS)
		return 0;
	for (sf = &sfs->sfs_vec[0]; sf->sf_allocated; sf++)
		assert(sf < &sfs->sfs_vec[SNOOP_MAXFILTERS]);
	sfs->sfs_elements++;
	sf->sf_allocated = 1;
	return sf;
}

int
sfs_freefilter(struct sfset *sfs, SnoopFilter *sf)
{
	assert((unsigned)(sf - &sfs->sfs_vec[0]) < SNOOP_MAXFILTERS);
	if (sfs->sfs_elements == 0 || !sf->sf_allocated)
		return 0;
	bzero(sf, sizeof *sf);
	--sfs->sfs_elements;
	return 1;
}
