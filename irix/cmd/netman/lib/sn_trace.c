/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * A trace file snooper implementation.
 */
#include <errno.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "exception.h"
#include "heap.h"
#include "macros.h"
#include "protocol.h"
#include "snooper.h"

#define	TRACEFILE_MAGIC	0x9e3779b9	/* magic version number */

DefineSnooperOperations(trace_snops,tsn)

typedef struct {
	FILE	*file;		/* stdio file pointer */
	int	fd;		/* file descriptor */
	XDR	xdr;		/* stdio xdr stream */
	Snooper	snooper;	/* base class state */
} TraceSnooper;

#define	TSN(sn)	((TraceSnooper *) (sn)->sn_private)

static int sock = -1;	/* socket to fake select until snooping is started */

Snooper *
tracesnooper(char *name, char *mode, Protocol *rawproto)
{
	TraceSnooper *tsn;
	int ronly, error;
	u_long magic;
	u_int rawprid;

	tsn = new(TraceSnooper);
	errno = 0;
	tsn->fd = -1;
	ronly = (mode[0] == 'r' && mode[1] != '+');
	if (!strcmp(name, "-"))
		tsn->file = ronly ? stdin : stdout;
	else
		tsn->file = fopen(name, mode);
	if (tsn->file == 0)
		goto sysfail;
	xdrstdio_create(&tsn->xdr, tsn->file, ronly ? XDR_DECODE : XDR_ENCODE);

	magic = TRACEFILE_MAGIC;
	if (!xdr_u_long(&tsn->xdr, &magic))
		goto sysfail;
	if (magic != TRACEFILE_MAGIC) {
		errno = EINVAL;
		goto sysfail;
	}

	if (tsn->xdr.x_op == XDR_ENCODE)
		rawprid = rawproto->pr_id;
	if (!xdr_u_int(&tsn->xdr, &rawprid))
		goto sysfail;
	if (tsn->xdr.x_op == XDR_DECODE) {
		rawproto = findprotobyid(rawprid);
		if (rawproto == 0) {
			errno = EPROTONOSUPPORT;
			goto sysfail;
		}
	}

	if (!sn_init(&tsn->snooper, tsn, &trace_snops, name, fileno(tsn->file),
		     rawproto)) {
		error = tsn->snooper.sn_error;
		goto fail;
	}
	if (ronly) {
		if (sock < 0)
			sock = socket(PF_INET, SOCK_DGRAM, 0);
		tsn->fd = dup(fileno(tsn->file));
		if (tsn->fd < 0)
			goto sysfail;
		if (dup2(sock, fileno(tsn->file)) < 0) {
			error = errno;
			close(tsn->fd);
			errno = error;
			goto sysfail;
		}
	}
	return &tsn->snooper;

sysfail:
	error = errno;
	exc_raise(error, "cannot snoop %s tracefile %s",
		  ronly ? "from" : "to", name);
fail:
	if (tsn->file)
		fclose(tsn->file);
	delete(tsn);
	errno = error;
	return 0;
}

/* ARGSUSED */
static int
tsn_add(Snooper *sn, struct expr **exp, struct exprerror *err)
{
	return 1;
}

/* ARGSUSED */
static int
tsn_delete(Snooper *sn)
{
	return 1;
}

static int
tsn_read(Snooper *sn, SnoopPacket *sp, int len)
{
	TraceSnooper *tsn;
	bool_t more;
	u_int cc;

	tsn = TSN(sn);
	if (tsn->xdr.x_op != XDR_DECODE) {
		sn->sn_error = EBADF;
		return -1;
	}
	if (!xdr_bool(&tsn->xdr, &more) || !more)
		return 0;
	cc = len;
	errno = 0;
	if (!xdr_snooppacket(&tsn->xdr, sp, &cc, (unsigned int) len)) {
		if (errno == 0)
			return 0;
		sn->sn_error = errno;
		return -1;
	}
	return cc;
}

static int
tsn_write(Snooper *sn, SnoopPacket *sp, int len)
{
	TraceSnooper *tsn;
	bool_t more;

	tsn = TSN(sn);
	if (tsn->xdr.x_op != XDR_ENCODE) {
		sn->sn_error = EBADF;
		return -1;
	}
	more = TRUE;
	errno = 0;
	if (!xdr_bool(&tsn->xdr, &more)
	    || !xdr_snooppacket(&tsn->xdr, sp, (u_int *) &len, (u_int) len)) {
		if (errno == 0)
			return 0;
		sn->sn_error = errno;
		return -1;
	}
	return len;
}

/* ARGSUSED */
static int
tsn_ioctl(Snooper *sn, int cmd, void *arg)
{
	switch (cmd) {
	    case SIOCSNOOPING:
	    {
		TraceSnooper *tsn = TSN(sn);
		if (*(int *)arg == 0)
			return dup2(sock, fileno(tsn->file));
		else
			return dup2(tsn->fd, fileno(tsn->file));
	    }

	    case SIOCRAWSTATS:
		bzero(arg, sizeof(struct snoopstats));
		break;
	}
	return 1;
}

/* ARGSUSED */
static int
tsn_getaddr(Snooper *sn, int cmd, struct sockaddr *sa)
{
	sn->sn_error = EOPNOTSUPP;
	return 0;
}

/* ARGSUSED */
static int
tsn_shutdown(Snooper *sn, enum snshutdownhow how)
{
	TraceSnooper *tsn;
	int rc;

	tsn = TSN(sn);
	if (tsn->fd >= 0)
		dup2(tsn->fd, fileno(tsn->file));
	fflush(tsn->file);
	rc = ferror(tsn->file);
	if (tsn->fd >= 0)
		dup2(sock, fileno(tsn->file));
	return rc;
}

static void
tsn_destroy(Snooper *sn)
{
	TraceSnooper *tsn;
	bool_t eof;

	tsn = TSN(sn);
	if (tsn->fd >= 0)
		dup2(tsn->fd, fileno(tsn->file));
	eof = FALSE;
	(void) xdr_bool(&tsn->xdr, &eof);
	xdr_destroy(&tsn->xdr);
	if (tsn->file != stdin && tsn->file != stdout)
		fclose(tsn->file);
	if (tsn->fd >= 0)
		close(tsn->fd);
	delete(tsn);
}
