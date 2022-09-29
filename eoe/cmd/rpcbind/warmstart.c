/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)rpcbind:warmstart.c	1.1.1.2"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 

/*
 * warmstart.c
 * Allows for gathering of registrations from a earlier dumped file.
 *
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <sys/stat.h>
#ifdef PORTMAP
#include <netinet/in.h>
#include <rpc/pmap_prot.h>
#endif
#include "rpcbind.h"
#ifdef SYSLOG
#include <sys/syslog.h>
#else
#define	LOG_DAEMON (3<<3)
#define	LOG_CONS 0x02
#define	LOG_ERR 3
#endif /* SYSLOG */

/* These files keep the pmap_list and rpcb_list in XDR format */
#define	RPCBFILE	"/tmp/rpcbind.file"
#ifdef PORTMAP
#define	PMAPFILE	"/tmp/portmap.file"
#endif

static bool_t write_struct();
static bool_t read_struct();

static bool_t
write_struct(filename, structproc, list)
	char *filename;
	xdrproc_t structproc;
	void *list;
{
	FILE *fp;
	XDR xdrs;

	fp = fopen(filename, "w");
	if (fp == NULL) {
		syslog(LOG_ERR, "cannot open file = %s for writing", filename);
		syslog(LOG_ERR, "cannot save any registration");
		return (FALSE);
	}
	xdrstdio_create(&xdrs, fp, XDR_ENCODE);

	if (structproc(&xdrs, list) == FALSE) {
		syslog(LOG_ERR, "rpcbind: xdr_%s: failed", filename);
		fclose(fp);
		return (FALSE);
	}
	XDR_DESTROY(&xdrs);
	fclose(fp);
	chmod(filename, S_IREAD|S_IWRITE);
	return (TRUE);
}

static bool_t
read_struct(filename, structproc, list)
	char *filename;
	xdrproc_t structproc;
	void *list;
{
	FILE *fp;
	XDR xdrs;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr,
		"rpcbind: cannot open file = %s for reading\n", filename);
		fprintf(stderr, "rpcbind: Will start from scratch\n");
		return (FALSE);
	}
	xdrstdio_create(&xdrs, fp, XDR_DECODE);

	if (structproc(&xdrs, list) == FALSE) {
		fprintf(stderr, "rpcbind: xdr_%s: failed\n", filename);
		fclose(fp);
		return (FALSE);
	}
	XDR_DESTROY(&xdrs);
	fclose(fp);
	return (TRUE);
}

void
write_warmstart()
{
	(void) write_struct(RPCBFILE, xdr_rpcblist, &list_rbl);
#ifdef PORTMAP
	(void) write_struct(PMAPFILE, xdr_pmaplist, &list_pml);
#endif

}

void
read_warmstart()
{
	RPCBLIST *tmp_rpcbl = NULL;
#ifdef PORTMAP
	PMAPLIST *tmp_pmapl = NULL;
#endif
	int ok1, ok2 = TRUE;

	ok1 = read_struct(RPCBFILE, xdr_rpcblist, &tmp_rpcbl);
	if (ok1 == FALSE)
		return;
#ifdef PORTMAP
	ok2 = read_struct(PMAPFILE, xdr_pmaplist, &tmp_pmapl);
#endif
	if (ok2 == FALSE) {
		xdr_free(xdr_rpcblist, &tmp_rpcbl);
		return;
	}
	xdr_free(xdr_rpcblist, &list_rbl);
	list_rbl = tmp_rpcbl;
#ifdef PORTMAP
	xdr_free(xdr_pmaplist, &list_pml);
	list_pml = tmp_pmapl;
#endif
}
