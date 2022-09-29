#ifndef SUNRPC_H
#define SUNRPC_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Sun RPC/XDR utility routines.
 */
#include <rpc/rpc.h>
#include "protostack.h"

struct pmap;
struct datastream;

/*
 * Sun RPC protocol option
 */
enum sunrpc_propt { SUNRPC_PROPT_PORTMAP };

struct sunrpcframe {
	ProtoStackFrame	srf_frame;	/* base class state */
	u_long		srf_prog;	/* program number */
	u_long		srf_vers;	/* program version */
	u_long		srf_proc;	/* procedure number */
	enum msg_type	srf_direction;	/* CALL or REPLY */
	int		srf_procoff;	/* offset of proc field if call */
	u_int		srf_morefrags;	/* true if more fragments */
	XDR		*srf_xdr;	/* xdr datastream decoder */
};

/*
 * Get a host's list of mappings from programs to ports given its address.
 * Unlike Sun's pmap_getmaps function, the IP address is interpreted in host
 * byte order.  Sunrpc_addmap adds a portmapping to the list associated with
 * the host-ordered address.
 */
struct pmaplist *sunrpc_getmaps(struct in_addr);
void		sunrpc_addmap(struct in_addr, struct pmap *);

/*
 * Get the port mapped to a program name.  The program name may be prefixed
 * by 'hostname:' and suffixed by '.version'.
 */
int	sunrpc_getport(char *, u_int, u_short *);

/*
 * Test whether a transport port on an Internet host maps to an rpc program.
 * These next functions interpret IP addresses in host order.
 */
int	sunrpc_ismapped(u_short, struct in_addr, u_int);

/*
 * Get a program number given an IP address and protocol type.
 */
int	sunrpc_getprog(u_short, struct in_addr, u_int, u_long *);

/*
 * Create an XDR stream on top of a datastream.
 */
void	xdrdata_create(XDR *, struct datastream *);

/*
 * RPC protocol module name ("sunrpc").
 */
extern char sunrpcname[];

#endif
