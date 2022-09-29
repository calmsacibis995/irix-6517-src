/*
 * These routines presume inside knowledge of stdio FILE
 * structs to steal it blindly of temporarily buffered writes
 * to stdout and stderr.  The data is instead sent back to
 * the client in XDR packets via RPC svc_sendreply calls.
 * See also the comments in rpc_io.h.
 */

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "rpc_io.h"
#include "externs.h"

extern int hadsigpipe;		/* if sigpipe sets, must be client vanished */
extern SVCXPRT *transp;		/* port back to client: rpc.rfindd.c sets it */

struct io stderrbuf = {stderr, 2, {0, 2, 0}};
struct io stdoutbuf = {stdout, 2, {0, 1, 0}};

void rpcsetbuf(void) {
	stdoutbuf.io_rfh.bufp = calloc (1, RPCBUFSIZ);
    	setvbuf (stdout, stdoutbuf.io_rfh.bufp, _IOFBF, RPCBUFSIZ);
	/* adjust for _findbuf.c: iop->_base = buf + PUSHBACK; */
	stdoutbuf.io_rfh.bufp = (char *)(stdout->_base);

	/* heaven only knows, why on Sherwood, but not on 4.0.*, without
	 * the following, _cnt goes negative and first printf("%u")
	 * is dropped. -- pj */
	stdout->_cnt = _bufendtab[stdout->_file] - stdout->_base;

	stderrbuf.io_rfh.bufp = calloc (1,RPCBUFSIZ);
	setvbuf (stderr, stderrbuf.io_rfh.bufp, _IOFBF, RPCBUFSIZ);
	/* adjust for _findbuf.c: iop->_base = buf + PUSHBACK; */
	stderrbuf.io_rfh.bufp = (char *)(stderr->_base);

	/* heaven only knows, why on Sherwood, but not on 4.0.*, without
	 * the following, _cnt goes negative and first printf("%u")
	 * is dropped. -- pj */
	stderr->_cnt = _bufendtab[stderr->_file] - stderr->_base;

}

void rpcresetminlen(void) {
	stdoutbuf.io_minlen = 0;
	stderrbuf.io_minlen = 0;
}

void rpcflushall(void) {
	rpcflush (&stderrbuf);
	rpcflush (&stdoutbuf);
}

void rpcflush (struct io *bp) {
	FILE *iop = bp->io_ptr;
	long len = iop->_ptr - iop->_base;

	if (len == 0) return;
	if (len < bp->io_minlen) return;
	if (bp->io_minlen < 700)
		bp->io_minlen *= 2;
	else
		bp->io_minlen = 1200;


	if (len > RPCBUFSIZ) {
		if (bp == &stderrbuf)
			_exit (0);	/* can't say much - my stderr is hosed */
		else {
			(void) fprintf(stderr, "internal buffer overflow botch");
			rpcexit (1);
			/* NOTREACHED */
		}
	}
	bp->io_rfh.sz = (u_int) len;
	if (!svc_sendreply(transp,xdr_rfind,&bp->io_rfh))
		_exit(0);
	if (hadsigpipe) _exit(0);		/* no client ==> no reason to live */
	iop->_cnt = _bufendtab[iop->_file] - iop->_base;
	iop->_ptr = iop->_base;
	iop->_flag &= ~(_IOERR | _IOEOF);
	if(iop->_flag & _IORW)
		iop->_flag &= ~(_IOREAD | _IOWRT);
}

void rpcexit (int rval) {
	rpcresetminlen();
	rpcflushall();
	stderrbuf.io_rfh.fd = 3;		/* use fd 3 for exit status */
	fprintf (stderr, "exit status %d\n", rval);
	rpcflush (&stderrbuf);
	svc_sendreply(transp,xdr_rfind,&stderrbuf.io_rfh);
	_exit (0);				/* no one really waits for exit */
	/* NOTREACHED */
}
