#ifndef __RPCSVC_YPCLNT_H__
#define __RPCSVC_YPCLNT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.15 $"
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
/*	@(#)ypclnt.h	1.1 88/02/17 4.0NFSSRC SMI	*/

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *	 1.12 86/12/19
 */


/*
 * ypclnt.h
 * This defines the symbols used in the C language interface to 
 * the network information service (NIS) client functions.  
 * A description of this interface can be read in ypclnt(3N).
 */

/*
 * Failure reason codes.  The success condition is indicated by a functional
 * value of "0".
 */
#define YPERR_BADARGS 1			/* Args to function are bad */
#define YPERR_RPC 2			/* RPC failure */
#define YPERR_DOMAIN 3			/* Can't bind to a server which serves
					 *   this domain. */
#define YPERR_MAP 4			/* No such map in server's domain */
#define YPERR_KEY 5			/* No such key in map */
#define YPERR_YPERR 6			/* Internal NIS server or client
					 *   interface error */
#define YPERR_RESRC 7			/* Local resource allocation failure */
#define YPERR_NOMORE 8			/* No more records in map database */
#define YPERR_PMAP 9			/* Can't communicate with portmapper */
#define YPERR_YPBIND 10			/* Can't communicate with ypbind */
#define YPERR_YPSERV 11			/* Can't communicate with ypserv */
#define YPERR_NODOM 12			/* Local domain name not set */
#define YPERR_BADDB 13			/* NIS data base is bad */
#define YPERR_VERS 14			/* NIS version mismatch */
#define YPERR_ACCESS 15			/* Access violation */
#define YPERR_BUSY 16			/* Database is busy */

/*
 * Types of update operations
 */
#define YPOP_CHANGE 1			/* change, do not add */
#define YPOP_INSERT 2			/* add, do not change */
#define YPOP_DELETE 3			/* delete this entry */
#define YPOP_STORE  4			/* add, or change */



/*
 * Data definitions
 */

/*
 * struct ypall_callback * is the arg which must be passed to yp_all
 */

struct ypall_callback {
	/* Return non-0 to stop getting called */
	int (*foreach)(int, char *, int, char *, int, char *);		
	/* Opaque pointer for use of callback function */
	void *data;			
};

/*
 * External NIS client function references.
 */
struct dom_binding;

extern int yp_bind(const char *);
extern int _yp_dobind(const char *, struct dom_binding **);
extern void yp_unbind(const char *);
extern int yp_match (const char *, const char *, const char *, int,
			char **, int *);
extern int yp_first (const char *, const char *, char **, int *,
			char **, int *);
extern int yp_next(const char *, const char *, const char *, int,
			char **, int *, char **, int *);
extern int yp_master(const char *, const char *, char **);
extern int yp_order(const char *, const char *, int *);
extern int yp_all(const char *, const char *, struct ypall_callback *);
extern char *yperr_string(int);
extern int ypprot_err(unsigned int);
extern int yp_get_default_domain(char **);
extern void _yp_disable();
extern void _yp_enable();

/*
 * Global NIS data structures
 */
extern int	_yellowup(int);
extern int	_yp_disabled;
extern int	_yp_is_bound;
extern char	_yp_domain[];
extern int	_yp_domainsize;
#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_YPCLNT_H__ */
