#ifndef __RPC_XDR_H__
#define __RPC_XDR_H__
#ident "$Revision: 2.39 $"
/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*	@(#)xdr.h	1.2 90/07/19 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 *      @(#)xdr.h 1.22 88/02/08 SMI 
 */

/*
 * xdr.h, External Data Representation Serialization Routines.
 */


/*
 * XDR provides a conventional way for converting between C data
 * types and an external bit-string representation.  Library supplied
 * routines provide for the conversion on built-in C data types.  These
 * routines and utility routines defined here are used to help implement
 * a type encode/decode routine for each user-defined type.
 *
 * Each data type provides a single procedure which takes two arguments:
 *
 *	bool_t
 *	xdrproc(xdrs, argresp)
 *		XDR *xdrs;
 *		<type> *argresp;
 *
 * xdrs is an instance of a XDR handle, to which or from which the data
 * type is to be converted.  argresp is a pointer to the structure to be
 * converted.  The XDR handle contains an operation field which indicates
 * which of the operations (ENCODE, DECODE * or FREE) is to be performed.
 *
 * XDR_DECODE may allocate space if the pointer argresp is null.  This
 * data can be freed with the XDR_FREE operation.
 *
 * We write only one procedure per data type to make it easy
 * to keep the encode and decode procedures for a data type consistent.
 * In many cases the same code performs all operations on a user defined type,
 * because all the hard work is done in the component type routines.
 * decode as a series of calls on the nested data types.
 */

/*
 * NOTE that for 64 bit software, the xdr representation of a long
 * and a u_long, is 32 bits. The reasons for this:
 *	All existing protocols expect only 32-bit data. Obviously
 *	we cannot change what is going into rpc packets.
 *
 *	All existing code makes liberal use of xdr_long, XDR_PUTLONG, etc.
 *	To change the behavior of xdr_long to put out 64 bits would require
 *	that all code that is to be compiled for 64 bits would have to be
 *	changed to use xdr_int, XDR_PUTINT, etc.
 *
 *	Moreover, all the data types being used as arguments to those
 *	xdr calls would have to be changed from long/u_long to int/u_int.
 *	That is not desirable, and in many cases would violate our 64bit
 *	abi, or supported API's.
 *
 *	Any 64 bit xdr user that wants to interoperate with 32 bit programs
 *	will be able to without changing any current code.
 */

/*
 * Xdr operations.  XDR_ENCODE causes the type to be encoded into the
 * stream.  XDR_DECODE causes the type to be extracted from the stream.
 * XDR_FREE can be used to release the space allocated by an XDR_DECODE
 * request.
 */
enum xdr_op {
	XDR_ENCODE=0,
	XDR_DECODE=1,
	XDR_FREE=2
};

/*
 * This is the number of bytes per unit of external data.
 */
#define BYTES_PER_XDR_UNIT	(4)
#define RNDUP(x)  ((((x) + BYTES_PER_XDR_UNIT - 1) / BYTES_PER_XDR_UNIT) \
		    * BYTES_PER_XDR_UNIT)

#if (_MIPS_SZLONG == 32)
typedef	long	xdr_long_t;
typedef	u_long	xdr_u_long_t;
#endif
#if (_MIPS_SZLONG == 64)
#include <sys/types.h>
typedef	__int32_t	xdr_long_t;
typedef	__uint32_t	xdr_u_long_t;
#endif

struct __xdr_s;
struct xdr_ops {
	/* get an xdr_long from underlying stream */
	bool_t	(*x_getlong)(struct __xdr_s *, long *);

	/* put an xdr_long to the stream */
	bool_t	(*x_putlong)(struct __xdr_s *, long *);

#if (_MIPS_SZLONG != _MIPS_SZINT)
	/* get an xdr_int from underlying stream */
	bool_t	(*x_getint)(struct __xdr_s *, int *);

	/* put an xdr_int to the stream */
	bool_t	(*x_putint)(struct __xdr_s *, int *);
#endif

	/* get some bytes from the stream */
	bool_t	(*x_getbytes)(struct __xdr_s *, void *, u_int);

	/* put some bytes to " */
	bool_t	(*x_putbytes)(struct __xdr_s *, void *, u_int);

	/* returns bytes off from beginning */
	u_int	(*x_getpostn)(struct __xdr_s *);

	/* lets you reposition the stream */
	bool_t	(*x_setpostn)(struct __xdr_s *, u_int);

	/* buf quick ptr to buffered data */
	long *	(*x_inline)(struct __xdr_s *, int);

	/* free privates of this xdr_stream */
	void	(*x_destroy)(struct __xdr_s *);
};

/*
 * The XDR handle.
 * Contains operation which is being applied to the stream,
 * an operations vector for the paticular implementation (e.g. see xdr_mem.c),
 * and two private fields for the use of the particular impelementation.
 */
typedef struct __xdr_s {
    enum xdr_op	x_op;		/* operation; fast additional param */
    struct xdr_ops  *x_ops;
    caddr_t 	x_public;	/* users' data */
    caddr_t	x_private;	/* pointer to private data */
    caddr_t  	x_base;		/* private used for position info */
    int		x_handy;	/* extra private word */
} XDR;

/*
 * A xdrproc_t exists for each data type which is to be encoded or decoded.
 *
 * The second argument to the xdrproc_t is a pointer to an opaque pointer.
 * The opaque pointer generally points to a structure of the data type to be
 * decoded.  If the opaque pointer is NULL (0), then the type routines should
 * allocate dynamic storage of the appropriate size and return it.
 *
 * We can't really prototype this since most callers of routines that take
 * an xdrproc_t really (as described above) take an XDR* and an arbitrary
 * pointer. We provide a typedef so that user's who want to deref an xdrproc_t
 * have a prototype they can case to
 */
typedef	bool_t (*xdrproc_t)();
typedef	bool_t (*p_xdrproc_t)(XDR *, void *);

/*
 * there are a few xdrproc's that take a third 'cnt' argument -
 * this typedef can be used to coerce one into the other
 */
typedef bool_t (*xdrproc3_t)(XDR *, void *, u_int);

/*
 * Operations defined on a XDR handle
 *
 * XDR		*xdrs;
 * xdr_long_t	*longp;
 * void 	*addr;
 * u_int	 len;
 * u_int	 pos;
 */
#define XDR_GETLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)
#define xdr_getlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)

#define XDR_PUTLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)
#define xdr_putlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)

#if (_MIPS_SZLONG != _MIPS_SZINT)

#define XDR_GETINT(xdrs, intp)			\
	(*(xdrs)->x_ops->x_getint)(xdrs, intp)
#define xdr_getint(xdrs, intp)			\
	(*(xdrs)->x_ops->x_getint)(xdrs, intp)

#define XDR_PUTINT(xdrs, intp)			\
	(*(xdrs)->x_ops->x_putint)(xdrs, intp)
#define xdr_putint(xdrs, intp)			\
	(*(xdrs)->x_ops->x_putint)(xdrs, intp)
#else
#define XDR_GETINT(xdrs, intp) XDR_GETLONG(xdrs, (long *)intp)
#define XDR_PUTINT(xdrs, intp) XDR_PUTLONG(xdrs, (long *)intp)
#define xdr_getint(xdrs, intp) xdr_getlong(xdrs, (long *)intp)
#define xdr_putint(xdrs, intp) xdr_putlong(xdrs, (long *)intp)

#endif	/* _MIPS_SZLONG != _MIPS_SZINT */

#define XDR_GETBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)
#define xdr_getbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)

#define XDR_PUTBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)
#define xdr_putbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)

#define XDR_GETPOS(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)
#define xdr_getpos(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)

#define XDR_SETPOS(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)
#define xdr_setpos(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)

#define	XDR_INLINE(xdrs, len)				\
	(xdr_long_t *)(*(xdrs)->x_ops->x_inline)(xdrs, len)
#define	xdr_inline(xdrs, len)				\
	(xdr_long_t *)(*(xdrs)->x_ops->x_inline)(xdrs, len)

#define	XDR_DESTROY(xdrs)				\
	(*(xdrs)->x_ops->x_destroy)(xdrs)
#define	xdr_destroy(xdrs) XDR_DESTROY(xdrs)

/*
 * Support struct for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * a entry with a null procedure pointer.  The xdr_union routine gets
 * the discriminant value and then searches the array of structures
 * for a matching value.  If a match is found the associated xdr routine
 * is called to handle that part of the union.  If there is
 * no match, then a default routine may be called.
 * If there is no match and no default routine it is an error.
 */
#define NULL_xdrproc_t ((xdrproc_t)0)
struct xdr_discrim {
	int	value;
	xdrproc_t proc;
};

/*
 * In-line routines for fast encode/decode of primitve data types.
 * Caveat emptor: these use single memory cycles to get the
 * data from the underlying buffer, and will fail to operate
 * properly if the data is not aligned.  The standard way to use these
 * is to say:
 *	if ((buf = XDR_INLINE(xdrs, count)) == NULL)
 *		return (FALSE);
 *	<<< macro calls >>>
 * where ``count'' is the number of bytes of data occupied
 * by the primitive data types.
 *
 * N.B. and frozen for all time: each data type here uses 4 bytes
 * of external representation.
 */
#define IXDR_GET_LONG(buf)	\
		(ntohl(*(xdr_long_t *)(buf)++))
#define IXDR_PUT_LONG(buf, v)	\
		(*(xdr_long_t *)(buf)++ = (xdr_long_t)htonl((u_long)v))

#define IXDR_GET_BOOL(buf)		((bool_t)IXDR_GET_LONG(buf))
#define IXDR_GET_ENUM(buf, t)		((t)IXDR_GET_LONG(buf))
#define IXDR_GET_U_LONG(buf)		(ntohl(*(xdr_u_long_t *)(buf)++))
#define IXDR_GET_SHORT(buf)		((short)IXDR_GET_LONG(buf))
#define IXDR_GET_U_SHORT(buf)		((u_short)IXDR_GET_LONG(buf))

#define IXDR_PUT_BOOL(buf, v)		IXDR_PUT_LONG((buf), ((xdr_long_t)(v)))
#define IXDR_PUT_ENUM(buf, v)		IXDR_PUT_LONG((buf), ((xdr_long_t)(v)))
#define IXDR_PUT_U_LONG(buf, v)		IXDR_PUT_LONG((buf), ((xdr_long_t)(v)))
#define IXDR_PUT_SHORT(buf, v)		IXDR_PUT_LONG((buf), ((xdr_long_t)(v)))
#define IXDR_PUT_U_SHORT(buf, v)	IXDR_PUT_LONG((buf), ((xdr_long_t)(v)))

/*
 * These are the "generic" xdr routines.
 */
extern void	xdr_free(xdrproc_t, void *);
extern bool_t	xdr_void(XDR *, void *);
extern bool_t	xdr_int(XDR *, int *);
extern bool_t	xdr_u_int(XDR *, u_int *);
extern bool_t	xdr_long(XDR *, long *);
extern bool_t	xdr_u_long(XDR *, u_long *);
extern bool_t	xdr_short(XDR *, short *);
extern bool_t	xdr_u_short(XDR *, u_short *);
extern bool_t	xdr_char(XDR *, char *);
extern bool_t	xdr_u_char(XDR *, u_char *);
extern bool_t	xdr_bool(XDR *, bool_t *);
extern bool_t	xdr_enum(XDR *, enum_t *);
extern bool_t	xdr_array(XDR *, caddr_t *, u_int *, u_int, u_int, xdrproc_t);
extern bool_t	xdr_bytes(XDR *, char **, u_int *, u_int);
extern bool_t	xdr_opaque(XDR *, void *, u_int);
extern bool_t	xdr_string(XDR *, char **, u_int);
extern bool_t	xdr_union(XDR *, enum_t *, void *, struct xdr_discrim *, 
			xdrproc_t);
extern bool_t	xdr_reference(XDR *, caddr_t *, u_int, xdrproc_t);
extern bool_t	xdr_pointer(XDR *, caddr_t *, u_int, xdrproc_t);
extern bool_t	xdr_time_t(XDR *, time_t *);
extern bool_t	xdr_int32(XDR *, int *);
extern bool_t	xdr_int64(XDR *, __int64_t *);
extern bool_t	xdr_uint32(XDR *, u_int *);
extern bool_t	xdr_uint64(XDR *, __uint64_t *);
extern bool_t	xdr_hyper(XDR *, __int64_t *);
extern bool_t	xdr_longlong_t(XDR *, __int64_t *);
extern bool_t	xdr_u_hyper(XDR *, __uint64_t *);
extern bool_t	xdr_u_longlong_t(XDR *, __uint64_t *);
#if _ABIAPI
extern bool_t	xdr_longlong(XDR *, __int64_t *);
extern bool_t	xdr_u_longlong(XDR *, __uint64_t *);
#endif
#ifndef _KERNEL
extern bool_t	xdr_vector(XDR *, char *, u_int, u_int, xdrproc_t);
extern bool_t	xdr_float(XDR *, float *);
extern bool_t	xdr_double(XDR *, double *);
extern bool_t	xdr_wrapstring(XDR *, char **);
#endif /* !_KERNEL */

/*
 * Common opaque bytes objects used by many rpc protocols;
 * declared here due to commonality.
 */
#define MAX_NETOBJ_SZ 1024 
struct netobj {
	u_int	n_len;
	char	*n_bytes;
};
typedef struct netobj netobj;
extern bool_t   xdr_netobj(XDR *, netobj *);

extern int xdr_authkern(XDR *);

/*
 * These are the public routines for the various implementations of
 * xdr streams.
 */
/* XDR using memory buffers */
extern void   xdrmem_create(XDR *, void *, u_int, enum xdr_op);	

#ifndef _KERNEL 
#include <stdio.h>
extern void   xdrstdio_create(XDR *, FILE *, enum xdr_op);

/* XDR pseudo records for TCP */
extern void   xdrrec_create(XDR *, u_int, u_int, void *, 
			    int (*)(void *, void *, u_int), 
			    int (*)(void *, void *, u_int));
/* Make end of xdr record */
extern bool_t xdrrec_endofrecord(XDR *, bool_t);	

/* Move to beginning of next record */
extern bool_t xdrrec_skiprecord(XDR *);	

/* True if no more input */
extern bool_t xdrrec_eof(XDR *);	

/* 
 * Returns up to the number of bytes requested,
 * 0 indicates end-of-record, -1 means something very bad happened.
 */
extern int xdrrec_readbytes(XDR *, caddr_t, u_int);

#else
/* XDR using kernel mbufs */
struct mbuf;
extern void xdrmbuf_init(XDR *, struct mbuf *, enum xdr_op);
extern bool_t   xdrmbuf_getmbuf(XDR *, struct mbuf **, u_int *);
extern bool_t   xdrmbuf_putbuf(XDR *, caddr_t, u_int);
extern bool_t	xdrmbuf_getpfdchain(XDR *, void *);
extern struct mbuf     *mclgetx(void (*dfun)(), long, void (*ffun)(),
			long, caddr_t, int, int);
#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* !__RPC_XDR_H__ */
