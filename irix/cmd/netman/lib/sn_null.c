/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Null snooper implementation.
 */
#include <sys/errno.h>
#include "heap.h"
#include "macros.h"
#include "snooper.h"

DefineSnooperOperations(null_snops,nsn)

Snooper *
nullsnooper(struct protocol *pr)
{
	Snooper *sn;

	sn = new(Snooper);
	(void) sn_init(sn, sn, &null_snops, "null", -1, pr);
	return sn;
}

/* ARGSUSED */
static int
nsn_add(Snooper *sn, struct expr **exp, struct exprerror *err)
{
	return 1;
}

/* ARGSUSED */
static int
nsn_delete(Snooper *sn)
{
	return 1;
}

/* ARGSUSED */
static int
nsn_read(Snooper *sn, SnoopPacket *sp, int len)
{
	return 0;
}

/* ARGSUSED */
static int
nsn_write(Snooper *sn, SnoopPacket *sp, int len)
{
	return len;
}

/* ARGSUSED */
static int
nsn_ioctl(Snooper *sn, int cmd, void *data)
{
	return 1;
}

/* ARGSUSED */
static int
nsn_getaddr(Snooper *sn, int cmd, struct sockaddr *sa)
{
	sn->sn_error = EOPNOTSUPP;
	return 0;
}

/* ARGSUSED */
static int
nsn_shutdown(Snooper *sn, enum snshutdownhow how)
{
	return 1;
}

static void
nsn_destroy(Snooper *sn)
{
	delete(sn);
}
