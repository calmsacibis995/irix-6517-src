/*
 * Copyright 1999, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Calculate physical memory present per node.  Walk the tree of /hw/module
 * looking for "memory" entries, and query their values.
 * 
 * Generates a "map" of the physical memory configured on each node:
 *
 *     nodemem_map[<nodenum>] == memory (in MB) configured on node <nodenum>
 *
 * max_nodemem is the largest node memory size encountered in the search.
 */
#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/dir.h>
#include <sys/iograph.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <paths.h>
#include <ftw.h>
#include <invent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "mem.h"

#define	MAX_WALK_DEPTH	32

#define MAX_MODULES	64
#define MAX_SLOTS	4

#define HW_MODULE_PATH	"/hw/module"
#define HW_NODENUM_PATH	"/hw/nodenum"

/* From main.c */
extern int nnodes;

/* EXPORTED VARIABLES */
int get_nodemem_map(void);
int nodemem_map[MAX_MODULES * MAX_SLOTS];
int max_nodemem = 0;			/* Largest node memory found */

/* STATIC VARIABLES */
static int nodemap[MAX_MODULES][MAX_SLOTS];
static int handle_entry(const char *path, 
			const struct stat *s,
			int type,
			struct FTW *f);
static int update_mem(void);

static unsigned         ninum = 0;
static nodeinfo_t       *ninfo = NULL;
static size_t           nisz = 0;

static u_char		*mem_data = NULL;

static int
update_mem(void)
{
    if (ninfo == NULL) {
	ninum = (int)sysmp(MP_NUMNODES);
	nisz  = sysmp(MP_SASZ, MPSA_NODE_INFO);
	ninfo = calloc(ninum, nisz);

	if (ninfo == NULL)
	    return 1;
    }

    if (sysmp(MP_SAGET, MPSA_NODE_INFO, (void *)ninfo, ninum * nisz))
	return 1;

    return 0;
}

/* ARGSUSED */
static int
handle_entry(const char *path, const struct stat *s, int type, struct FTW *f)
{
    invent_generic_t *invent;
    invent_meminfo_t *m;
    char *ptr;
    int len, node;
    char info[256];

    /* Ignore non-directories or directories that aren't named ".../memory". */
    if (type != FTW_D)
	return 0;

    ptr = strrchr(path, '/');
    if (!ptr || strcmp(ptr + 1, "memory"))
	return 0;

    /* Get the inventory details. */
    len = sizeof(info);
    if (attr_get((char *)path, INFO_LBL_DETAIL_INVENT, info, &len, 0))
	return 1;

    /* 
     * If it's a memory hunk, cast it to a meminfo and add it to the 
     * appropriate node's memory tally.
     */
    invent = (invent_generic_t *)info;
    if (invent->ig_invclass == INV_MEMORY) {
	m = (invent_meminfo_t *)info;
	node = nodemap[m->im_gen.ig_module][m->im_gen.ig_slot];
	nodemem_map[node] += m->im_size;
    }

    return 0;
}

int
get_nodemem_map(void)
{
	DIR *dirp;
	struct direct *entp;
	char *ptr;
	unsigned int module, slot;
	int nodenum, i, j;
	char real[1024];

	for (i = 0; i < MAX_MODULES; i++)
		for (j = 0; j < MAX_SLOTS; j++)
			nodemap[i][j] = 0;
	
	if (chdir(HW_NODENUM_PATH))
		return 1;

	if ((dirp = opendir(".")) == NULL)
		return 1;

	rewinddir(dirp);

	while ((entp = readdir(dirp)) != NULL) {
		if (*(entp->d_name) < '0' || *(entp->d_name) > '9')
			continue;
		if (readlink(entp->d_name, real, sizeof(real)) < 0)
			continue;

		nodenum = (int)strtol(entp->d_name, &ptr, 10);
		if (*ptr != '\0')
			continue;

		i = sscanf(real, "/hw/module/%u/slot/n%u/node", &module, &slot);
		if (i != 2)
			continue;

		nodemap[module][slot] = nodenum;
	}

	closedir(dirp);

	for (i = 0; i < MAX_MODULES * MAX_SLOTS; i++)
	    nodemem_map[i] = 0;

	nftw(HW_MODULE_PATH, handle_entry, MAX_WALK_DEPTH, FTW_PHYS);

	for (i = 0; i < nnodes; i++) {
	    if (nodemem_map[i] > max_nodemem)
		max_nodemem = nodemem_map[i];
	}

	return 0;
}

u_char *
mem_graph(void)
{
	int i;
	unsigned int phys, kern, user, free, total;
	u_char *p;

	if (mem_data == NULL) {
	    mem_data = (u_char *)malloc(MEM_DATA_SIZE(nnodes) + 16) + 16;
	    if (mem_data == NULL)
		fatal("can't allocated mem_data space");
	}

	p = mem_data;

	/* Update memory usage statistics. */
	update_mem();

	/* Compute the values to be graphed. */
	for (i = 0; i < nnodes; i++) {
		phys = nodemem_map[i];		/* Physical memory on node. */

		total = ninfo[i].totalmem / 1048576LL;
		free  = ninfo[i].freemem / 1048576LL;

		/* Space used by kernel (physmem - totalmem) */
		kern = phys - total;

		/* Space used by user application is total - free. */
		user = total - free;

		debug("mem_graph: node %d phys %d kern %d user %d free %d\n",
		    i, phys, kern, user, free);

		*p++ = (u_char)((255 * kern) / max_nodemem);
		*p++ = (u_char)((255 * (kern + user)) / max_nodemem);
		*p++ = (u_char)((255 * phys) / max_nodemem);

	}

	return mem_data;
}
