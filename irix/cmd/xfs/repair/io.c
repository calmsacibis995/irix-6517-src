#define _KERNEL 1
#include <sys/param.h>
#undef _KERNEL

#include <sys/sema.h>
#include <sys/uuid.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <diskinfo.h>

#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>

#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

void
io_init(void)
{
	int i;

	/* open up filesystem device */

	assert(fs_name != NULL && *fs_name != '\0');

	if (isa_file)
		rfs_name = fs_name;
	else if ((rfs_name = findrawpath(fs_name)) == NULL)
		do_error("couldn't find raw device for %s\n", fs_name);

	if ((fs_fd = open(rfs_name, O_RDWR)) < 0)  {
		do_error("couldn't open filesystem \"%s\"\n",
			rfs_name);
	}

	/* initialize i/o buffers */

	iobuf_size = 1000 * 1024;
	smallbuf_size = 4 * 4096;	/* enough for an ag */

	/*
	 * sbbuf_size must be < XFS_MIN_AG_BLOCKS (64) * smallest block size,
	 * otherwise you might get an EOF when reading in the sb/agf from
	 * the last ag if that ag is small
	 */
	sbbuf_size = 2 * 4096;		/* 2 * max sector size */

	if ((iobuf = malloc(iobuf_size)) == NULL)
		do_error("couldn't malloc io buffer\n");

	if ((smallbuf = malloc(smallbuf_size)) == NULL)
		do_error("couldn't malloc secondary io buffer\n");

	for (i = 0; i < NUM_SBS; i++)  {
		if ((sb_bufs[i] = malloc(sbbuf_size)) == NULL)
			do_error("couldn't malloc sb io buffers\n");
	}
}

/* ARGSUSED */
void
get_ag(char *buf, off64_t offset, int length)
{
}
