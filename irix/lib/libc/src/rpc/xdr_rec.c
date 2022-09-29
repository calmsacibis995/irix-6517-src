#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)xdr_rec.c	1.5 90/07/19 4.1NFSSRC Copyr 1990 Sun Micro";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.22 88/02/08
 */


/*
 * xdr_rec.c, Implements TCP/IP based XDR streams with a "record marking"
 * layer above tcp (for rpc's use).
 *
 * These routines interface XDRSTREAMS to a tcp/ip connection.
 * There is a record marking layer between the xdr stream
 * and the tcp transport level.  A record is composed on one or more
 * record fragments.  A record fragment is a thirty-two bit header followed
 * by n bytes of data, where n is contained in the header.  The header
 * is represented as a htonl(u_long).  The high order bit encodes
 * whether or not the fragment is the last fragment of the record
 * (1 => fragment is last, 0 => more fragments to follow).
 * The other 31 bits encode the byte length of the fragment.
 */

#ifdef __STDC__
	#pragma weak xdrrec_create = _xdrrec_create
	#pragma weak xdrrec_skiprecord = _xdrrec_skiprecord
	#pragma weak xdrrec_eof = _xdrrec_eof
	#pragma weak xdrrec_endofrecord = _xdrrec_endofrecord
	#pragma weak xdrrec_readbytes = _xdrrec_readbytes
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/errorhandler.h>
#include <netinet/in.h>
#include <bstring.h>		/* prototype for bcopy() */

/*
 * A record is composed of one or more record fragments.
 * A record fragment is a two-byte header followed by zero to
 * 2**32-1 bytes.  The header is treated as a long unsigned and is
 * encode/decoded to the network via htonl/ntohl.  The low order 31 bits
 * are a byte count of the fragment.  The highest order bit is a boolean:
 * 1 => this fragment is the last fragment of the record,
 * 0 => this fragment is followed by more fragment(s).
 *
 * The fragment/record machinery is not general;  it is constructed to
 * meet the needs of xdr and rpc based on tcp.
 */

#define LAST_FRAG ((xdr_u_long_t)(1 << 31))

typedef struct rec_strm {
	caddr_t tcp_handle;
	caddr_t the_buffer;
	/*
	 * out-goung bits
	 */
	int (*writeit)(caddr_t, u_long, int);
	caddr_t out_base;	/* output buffer (points to frag header) */
	caddr_t out_finger;	/* next output position */
	caddr_t out_boundry;	/* data cannot up to this address */
	xdr_u_long_t *frag_header;	/* beginning of curren fragment */
	bool_t frag_sent;	/* true if buffer sent in middle of record */
	u_long out_partial;	/* partial write */
	long out_pos;		/* output buffer seek position */
	/*
	 * in-coming bits
	 */
	int (*readit)(caddr_t, caddr_t, int);
	u_long in_size;		/* fixed size of the input buffer */
	caddr_t in_base;	/* base of input buffer */
	caddr_t in_finger;	/* location of next byte to be had */
	caddr_t in_boundry;	/* can read up to this location */
	long in_pos;		/* input buffer seek position */
	long fbtbc;		/* fragment bytes to be consumed */
	bool_t last_frag;
	bool_t first_frag;
	u_int sendsize;
	u_int recvsize;
} RECSTREAM;

static bool_t	xdrrec_getlong(XDR *, long *);
static bool_t	xdrrec_putlong(XDR *, long *);
static bool_t	xdrrec_getbytes(XDR *, caddr_t, u_int);
static bool_t	xdrrec_putbytes(XDR *, caddr_t, u_int);
static u_int	xdrrec_getpos(XDR *);
static bool_t	xdrrec_setpos(XDR *, u_int);
static long *	xdrrec_inline(XDR *, int);
static void	xdrrec_destroy(XDR *);
static bool_t	flush_out(RECSTREAM *, bool_t);
static bool_t	fill_input_buf(RECSTREAM *);
static bool_t	get_input_bytes(RECSTREAM *, caddr_t, long);
static bool_t	set_input_fragment(RECSTREAM *);
static bool_t	skip_input_bytes(RECSTREAM *, long);
static u_int	fix_buf_size(u_int);
#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t	xdrrec_getint(XDR *, int *);
static bool_t	xdrrec_putint(XDR *, int *);
#endif

static struct  xdr_ops xdrrec_ops = {
	xdrrec_getlong,
	xdrrec_putlong,
#if (_MIPS_SZLONG != _MIPS_SZINT)
	xdrrec_getint,
	xdrrec_putint,
#endif
	(bool_t (*)(XDR *, void *, u_int)) xdrrec_getbytes,
	(bool_t (*)(XDR *, void *, u_int)) xdrrec_putbytes,
	xdrrec_getpos,
	xdrrec_setpos,
	xdrrec_inline,
	xdrrec_destroy
};


/*
 * Create an xdr handle for xdrrec
 * xdrrec_create fills in xdrs.  Sendsize and recvsize are
 * send and recv buffer sizes (0 => use default).
 * tcp_handle is an opaque handle that is passed as the first parameter to
 * the procedures readit and writeit.  Readit and writeit are read and
 * write respectively.   They are like the system
 * calls expect that they take an opaque handle rather than an fd.
 */
void
xdrrec_create(register XDR *xdrs,
	register u_int sendsize,
	register u_int recvsize,
	void  *tcp_handle,
	int (*readit)(),  /* like read, but pass it a tcp_handle, not sock */
	int (*writeit)())  /* like write, but pass it a tcp_handle, not sock */
{
	register RECSTREAM *rstrm =
		(RECSTREAM *)mem_alloc(sizeof(RECSTREAM));

	if (rstrm == NULL) {
		_rpc_errorhandler(LOG_ERR, "xdrrec_create: out of memory");
		/*
		 *  This is bad.  Should rework xdrrec_create to
		 *  return a handle, and in this case return NULL
		 */
		return;
	}
	/*
	 * adjust sizes and allocate buffer quad byte aligned
	 */
	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
	rstrm->the_buffer = mem_alloc(sendsize + recvsize + BYTES_PER_XDR_UNIT);
	if (rstrm->the_buffer == NULL) {
		_rpc_errorhandler(LOG_ERR, "xdrrec_create: out of memory");
		mem_free((char *) rstrm, sizeof(RECSTREAM));
		return;
	}
	for (rstrm->out_base = rstrm->the_buffer;
		(__psunsigned_t)rstrm->out_base % BYTES_PER_XDR_UNIT != 0;
		rstrm->out_base++);
	rstrm->in_base = rstrm->out_base + sendsize;
	/*
	 * now the rest ...
	 */
	xdrs->x_ops = &xdrrec_ops;
	xdrs->x_private = (caddr_t)rstrm;
	rstrm->tcp_handle = tcp_handle;
	rstrm->readit = readit;
	rstrm->writeit = writeit;
	rstrm->out_finger = rstrm->out_boundry = rstrm->out_base;
	rstrm->out_pos = 0;
	rstrm->frag_header = (xdr_u_long_t *)rstrm->out_base;
	rstrm->out_finger += sizeof(xdr_u_long_t);
	rstrm->out_boundry += sendsize;
	rstrm->frag_sent = FALSE;
	rstrm->in_size = recvsize;
	rstrm->in_boundry = rstrm->in_base;
	rstrm->in_finger = (rstrm->in_boundry += recvsize);
	rstrm->in_pos = 0;
	rstrm->fbtbc = 0;
	rstrm->last_frag = TRUE;
	rstrm->first_frag = FALSE;
	rstrm->out_partial = 0;
}


/*
 * The reoutines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */

static bool_t
xdrrec_getlong(XDR *xdrs, long *lp)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register xdr_long_t *buflp = (xdr_long_t *)(rstrm->in_finger);
	xdr_long_t mylong;

	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= sizeof(xdr_long_t)) &&
			(((__psint_t)rstrm->in_boundry - (__psint_t)buflp) >=
				sizeof(xdr_long_t))) {
		*lp = (long)ntohl((u_long)(*buflp));
		rstrm->fbtbc -= sizeof(xdr_long_t);
		rstrm->in_finger += sizeof(xdr_long_t);
	} else {
		if (! xdrrec_getbytes(xdrs, (caddr_t)&mylong, sizeof(xdr_long_t)))
			return (FALSE);
		*lp = (long)ntohl((u_long)mylong);
	}
	return (TRUE);
}

static bool_t
xdrrec_putlong(XDR *xdrs, long *lp)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register xdr_long_t *dest_lp = ((xdr_long_t *)(rstrm->out_finger));

	if ((rstrm->out_finger += sizeof(xdr_long_t)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof(xdr_long_t);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE))
			return (FALSE);
		dest_lp = ((xdr_long_t *)(rstrm->out_finger));
		rstrm->out_finger += sizeof(xdr_long_t);
	}
	*dest_lp = (xdr_long_t)htonl((u_long)(*lp));
	return (TRUE);
}

#if (_MIPS_SZLONG != _MIPS_SZINT)
static bool_t
xdrrec_getint(XDR *xdrs, int *ip)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int *bufip = (int *)(rstrm->in_finger);
	int myint;

	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= sizeof(int)) &&
	   (((__psint_t)rstrm->in_boundry - (__psint_t)bufip) >= sizeof(int))) {
		*ip = (int)ntohl((u_int)(*bufip));
		rstrm->fbtbc -= sizeof(int);
		rstrm->in_finger += sizeof(int);
	} else {
		if (! xdrrec_getbytes(xdrs, (caddr_t)&myint, sizeof(int)))
			return (FALSE);
		*ip = (int)ntohl((u_int)myint);
	}
	return (TRUE);
}

static bool_t
xdrrec_putint(XDR *xdrs, int *ip)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int *dest_ip = ((int *)(rstrm->out_finger));

	if ((rstrm->out_finger += sizeof(int)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof(int);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE))
			return (FALSE);
		dest_ip = ((int *)(rstrm->out_finger));
		rstrm->out_finger += sizeof(int);
	}
	*dest_ip = (int)htonl((u_int)(*ip));
	return (TRUE);
}
#endif	/* (_MIPS_SZLONG != _MIPS_SZINT) */

static bool_t  /* must manage buffers, fragments, and records */
xdrrec_getbytes(XDR *xdrs, register caddr_t addr, register u_int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register long current;

	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return (FALSE);
			if (! set_input_fragment(rstrm))
				return (FALSE);
			continue;
		}
		current = (long)(len < current ? len : current);
		if (! get_input_bytes(rstrm, addr, current))
			return (FALSE);
		addr += current;
		rstrm->fbtbc -= current;
		len -= (unsigned int)current;
	}
	return (TRUE);
}

static bool_t
xdrrec_putbytes(XDR *xdrs, register caddr_t addr, register u_int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register long current;

	while (len > 0) {
		current = (__psint_t)rstrm->out_boundry - (__psint_t)rstrm->out_finger;
		current = (int)(len < (unsigned int)current ? len : (unsigned int)current);
		bcopy(addr, rstrm->out_finger, (int)current);
		rstrm->out_finger += current;
		addr += current;
		len -= (unsigned int)current;
		if (rstrm->out_finger == rstrm->out_boundry) {
			rstrm->frag_sent = TRUE;
			if (! flush_out(rstrm, FALSE))
				return (FALSE);
		}
	}
	return (TRUE);
}

static u_int
xdrrec_getpos(register XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	register long pos;

	switch (xdrs->x_op) {
	case XDR_ENCODE:
		pos = rstrm->out_pos + (rstrm->out_finger - rstrm->out_base);
		break;

	case XDR_DECODE:
		pos = rstrm->in_pos - (rstrm->in_boundry - rstrm->in_finger);
		break;

	default:
		pos = -1;
	}
	return ((u_int) pos);
}

static bool_t
xdrrec_setpos(register XDR *xdrs, u_int pos)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	u_int currpos = xdrrec_getpos(xdrs);
	int delta = (int)(currpos - pos);
	caddr_t newpos;

	if ((int)currpos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			newpos = rstrm->out_finger - delta;
			if ((newpos > (caddr_t)(rstrm->frag_header)) &&
			    (newpos < rstrm->out_boundry)) {
				rstrm->out_finger = newpos;
				return (TRUE);
			}
			break;

		case XDR_DECODE:
			newpos = rstrm->in_finger - delta;
			if ((delta < (int)(rstrm->fbtbc)) &&
			    (newpos <= rstrm->in_boundry) &&
			    (newpos >= rstrm->in_base)) {
				rstrm->in_finger = newpos;
				rstrm->fbtbc -= delta;
				return (TRUE);
			}
			break;
		}
	return (FALSE);
}

static long *
xdrrec_inline(register XDR *xdrs, int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	long * buf = NULL;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		if ((rstrm->out_finger + len) <= rstrm->out_boundry) {
			buf = (long *) rstrm->out_finger;
			rstrm->out_finger += len;
		}
		break;

	case XDR_DECODE:
		if ((len <= rstrm->fbtbc) &&
			((rstrm->in_finger + len) <= rstrm->in_boundry)) {
			buf = (long *) rstrm->in_finger;
			rstrm->fbtbc -= len;
			rstrm->in_finger += len;
		}
		break;
	}
	return (buf);
}

static void
xdrrec_destroy(register XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;

	mem_free(rstrm->the_buffer,
		rstrm->sendsize + rstrm->recvsize + BYTES_PER_XDR_UNIT);
	mem_free((caddr_t)rstrm, sizeof(RECSTREAM));
}


/*
 * Exported routines to manage xdr records
 */

/*
 * Before reading (deserializing from the stream, one should always call
 * this procedure to guarantee proper record alignment.
 */
bool_t
xdrrec_skiprecord(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (!rstrm->first_frag && (rstrm->fbtbc > 0 || !rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (FALSE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (FALSE);
	}
	rstrm->last_frag = FALSE;
	rstrm->first_frag = TRUE;
	return (TRUE);
}

/*
 * Look ahead fuction.
 * Returns TRUE iff there is no more input in the buffer
 * after consuming the rest of the current record.
 */
bool_t
xdrrec_eof(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (TRUE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (TRUE);
	}
	if (rstrm->in_finger == rstrm->in_boundry)
		return (TRUE);
	return (FALSE);
}

/*
 * The client must tell the package when an end-of-record has occurred.
 * The second paraemters tells whether the record should be flushed to the
 * (output) tcp stream.  (This let's the package support batched or
 * pipelined procedure calls.)  TRUE => immmediate flush to tcp connection.
 */
bool_t
xdrrec_endofrecord(XDR *xdrs, bool_t sendnow)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register u_long len;  /* fragment length */

	if (sendnow || rstrm->frag_sent ||
		((u_long)rstrm->out_finger + sizeof(u_long) >=
		(u_long)rstrm->out_boundry)) {
		rstrm->frag_sent = FALSE;
		return (flush_out(rstrm, TRUE));
	}
	len = (u_long)(rstrm->out_finger) - (u_long)(rstrm->frag_header) -
	      sizeof(xdr_u_long_t);
	*(rstrm->frag_header) = (xdr_u_long_t)htonl(len | LAST_FRAG);
	rstrm->frag_header = (xdr_u_long_t *)rstrm->out_finger;
	rstrm->out_finger += sizeof(xdr_u_long_t);
	return (TRUE);
}

/*
 * This is just like the ops vector xdr_getbytes(), except that
 * instead of returning success or failure on getting a certain number
 * of bytes, it behaves much more like the read() system call against a
 * pipe -- it returns up to the number of bytes requested and a return of
 * zero indicates end-of-record.  A -1 means something very bad happened.
 */
int /* must manage buffers, fragments, and records */
xdrrec_readbytes(XDR *xdrs, caddr_t addr, u_int l)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register long current, len;

	len = (long)l;
	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return ((int)l - (int)len);
			if (! set_input_fragment(rstrm))
				return (-1);
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current))
			return (-1);
		addr += current;
		rstrm->fbtbc -= current;
		len -= current;
	}
	return ((int)l - (int)len);
}

/*
 * Internal useful routines
 */
static bool_t
flush_out(register RECSTREAM *rstrm, bool_t eor)
{
	register u_long eormask = (eor == TRUE) ? LAST_FRAG : 0;
	register u_long len = (u_long)(rstrm->out_finger) -
		(u_long)(rstrm->frag_header) - sizeof(xdr_u_long_t);
	u_long base = (u_long)(rstrm->out_base) + rstrm->out_partial;
	long written;

	*(rstrm->frag_header) = (xdr_u_long_t)htonl(len | eormask);
	len = (u_long)(rstrm->out_finger) - base;
	written = (*(rstrm->writeit))(rstrm->tcp_handle, base, (int)len);
	if (written < 0)
		return (FALSE);
	if ((unsigned long)written != len) {
		rstrm->out_partial += (unsigned long)written;
		return (FALSE);
	}
	rstrm->out_pos += rstrm->out_partial + (unsigned long)written;
	rstrm->out_partial = 0;
	rstrm->frag_header = (xdr_u_long_t *)rstrm->out_base;
	rstrm->out_finger = (caddr_t)rstrm->out_base + sizeof(xdr_u_long_t);
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
fill_input_buf(register RECSTREAM *rstrm)
{
	register caddr_t where = rstrm->in_base;
	register long len = (int)rstrm->in_size;
	long align = ((__psint_t)rstrm->in_boundry % BYTES_PER_XDR_UNIT);

	if (align != 0) {
		/*
		 * Keep stream non-congruent bytes aligned with memory
		 * non-congruent bytes.
		 */
		where += align;
		len -= align;
	}
	if ((len = (*(rstrm->readit))(rstrm->tcp_handle, where, (int)len)) < 0)
		return (FALSE);
	rstrm->in_finger = where;
	where += len;
	rstrm->in_boundry = where;
	rstrm->in_pos += len;
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
get_input_bytes(register RECSTREAM *rstrm, 
	register caddr_t addr, 
	register long len)
{
	register long current;

	while (len > 0) {
		current = (__psint_t)rstrm->in_boundry - (__psint_t)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		bcopy(rstrm->in_finger, addr, (int)current);
		rstrm->in_finger += current;
		addr += current;
		len -= current;
	}
	rstrm->first_frag = FALSE;
	return (TRUE);
}

static bool_t
set_input_fragment(register RECSTREAM *rstrm)
{
	xdr_u_long_t header;

	/* next four bytes of the input stream are treated as a header */
	if (! get_input_bytes(rstrm, (caddr_t)&header, sizeof(header)))
		return (FALSE);
	header = (xdr_u_long_t)ntohl(header);
	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
	rstrm->fbtbc = (long)(header & (~LAST_FRAG));
	return (TRUE);
}

static bool_t  /* consumes input bytes; knows nothing about records! */
skip_input_bytes(register RECSTREAM *rstrm, long cnt)
{
	register long current;

	while (cnt > 0) {
		current = (__psint_t)rstrm->in_boundry - (__psint_t)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (cnt < current) ? cnt : current;
		rstrm->in_finger += current;
		cnt -= current;
	}
	return (TRUE);
}

static u_int
fix_buf_size(register u_int s)
{

	if (s < 100)
		s = 4000;
	return (RNDUP(s));
}
