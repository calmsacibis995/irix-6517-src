/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * zero.c - /dev/zero driver
 */

#ident	"$Revision: 3.28 $"

#include "sys/types.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/cred.h"
#include "sys/conf.h"
#include "sys/systm.h"
#include "sys/major.h"
#if MULTIKERNEL
#include "sys/EVEREST/everest.h"
#endif

int zerodevflag = D_MP;
static int zerorw(int rw, uio_t *uiop);

#define READ 	1
#define WRITE 	2

/*
 * mapping /dev/zero is really a std VM operation, it nothing do
 * with mapping a device.
 * In order to short circuit lots of special code - we export our
 * 'dev' and let mmap compare directly to it.
 */
#if CELL_IRIX
/* It looks like we can short circuit even more code by setting it here */
dev_t zeroesdev = makedev(ZERO_MAJOR, 0);
#else
dev_t zeroesdev;
#endif

int
zeroreg()
{
	dev_t zero_dev;

	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "zero", "zero", &zero_dev);
	hwgraph_chmod(zero_dev, 0666);

	return(0);
}

/* ARGSUSED */
int
zeroopen(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
#if CELL_IRIX
	ASSERT(emajor(*devp) == ZERO_MAJOR);
#else
	if (zeroesdev == 0) {
		zeroesdev = *devp;
	}
#endif
	return 0;
}

/* ARGSUSED */
int
zeroclose(dev_t devp, int oflag, int otyp, cred_t *crp)
{
	return 0;
}

/* ARGSUSED */
int
zeroread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	return(zerorw(READ,uiop));
}

/* ARGSUSED */
int
zerowrite(dev_t dev, uio_t *uiop, cred_t *crp)
{
	return(zerorw(WRITE,uiop));
}

static int
zerorw(int rw, uio_t *uiop)
{
	iovec_t *iovp;

	iovp = uiop->uio_iov;
	/*
	 * Access /dev/zero
	 */
	do {
		if (rw == READ) {
			if (uzero(iovp->iov_base, iovp->iov_len))
				return(EFAULT);
		}
		uiop->uio_offset += iovp->iov_len;
		iovp->iov_base = (char *)iovp->iov_base + iovp->iov_len;
		iovp->iov_len = 0;
		iovp++;
	} while (--(uiop->uio_iovcnt));
	uiop->uio_resid = 0;
	return(0);
}

int
zerounmap()
{
	return(0);
}
