/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libnet:netdir/netdir.c	1.7.5.1"

/*
 *+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's UNIX(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *  	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
 *	(c) 1990,1991  UNIX System Laboratories, Inc.
 *  	          All rights reserved.
 */

#define dlopen_works 1

/*
 * netdir.c
 *
 * This is the library routines that do the name to address
 * translation.
 */
#if defined(__STDC__) 
        #pragma weak netdir_free	= _netdir_free
#ifndef _LIBNSL_ABI
        #pragma weak netdir_perror	= _netdir_perror
        #pragma weak netdir_sperror	= _netdir_sperror
#endif /* _LIBNSL_ABI */
#if defined(netdir_getbyaddr)
        #pragma weak netdir_getbyaddr	= _netdir_getbyaddr
        #pragma weak netdir_getbyname	= _netdir_getbyname
        #pragma weak netdir_options	= _netdir_options
        #pragma weak taddr2uaddr	= _taddr2uaddr
        #pragma weak uaddr2taddr	= _uaddr2taddr
#endif
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

#include <stdio.h>
#include <sys/types.h>
#include <tiuser.h>
#include <netconfig.h>
#include <netdir.h>
#include <string.h>
#include <sys/file.h>
#include <dlfcn.h>

int	_nderror;
extern int errno;

static struct translator {
	struct nd_addrlist	*(*gbn)();	/* _netdir_getbyname	*/
	struct nd_hostservlist	*(*gba)();	/* _netdir_getbyaddr	*/
	int			(*opt)();	/* _netdir_options	*/
	char			*(*t2u)();	/* _taddr2uaddr		*/
	struct netbuf		*(*u2t)();	/* _uaddr2taddr		*/
	void *			tr_fd;		/* library descriptor	*/
	char			*tr_name;	/* Full path		*/
	struct translator	*next;
};

static struct translator *xlate_list = NULL;

static struct translator *load_xlate();
extern char *malloc();

#if ! dlopen_works
extern struct nd_addrlist 	* _netdir_getbyname();
extern struct nd_hostservlist 	* _netdir_getbyaddr(); 
extern int   			  _netdir_options();
extern char   			* _taddr2uaddr(); 
extern struct netbuf  		* _uaddr2taddr();
#endif

/*
 * This routine is the main routine it resolves host/service/xprt triples
 * into a bunch of netbufs that should connect you to that particular
 * service. RPC uses it to contact the binder service (rpcbind)
 */
int
netdir_getbyname(
	struct netconfig	*tp,	/* The network config entry	*/
	struct nd_hostserv	*serv,	/* the service, host pair	*/
	struct nd_addrlist	**addrs) /* the answer			*/
{
	struct translator	*t;	/* pointer to translator list	*/
	struct nd_addrlist	*nn;	/* the results			*/
	char			*lr;	/* routines to try		*/
	int			i;	/* counts the routines		*/

	for (i = 0; i < tp->nc_nlookups; i++) {
		lr = *((tp->nc_lookups) + i);
		for (t = xlate_list; t; t = t->next) {
			if (strcmp(lr, t->tr_name) == 0) {
				nn = (*(t->gbn))(tp, serv);
				if (nn) {
					*addrs = nn;
					return (0);
				} else {
					if (_nderror < 0)
						return (_nderror);
					break;
				}
			}
		}
		/* If we didn't find it try loading it */
		if (!t) {
			if ((t = load_xlate(lr)) != NULL) {
				/* add it to the list */
				t->next = xlate_list;
				xlate_list = t;
				nn = (*(t->gbn))(tp, serv);
				if (nn) {
					*addrs = nn;
					return (0);
				} else {
					if (_nderror < 0)
						return (_nderror);
				}
			}
		}
	}

	return (_nderror);	/* No one works */
}

/*
 * This routine is similar to the one above except that it trys to resolve
 * the name by the address passed.
 */
int
netdir_getbyaddr(
	struct netconfig	*tp,	/* The netconfig entry		*/
	struct nd_hostservlist	**serv,	/* the answer(s)		*/
	struct netbuf		*addr)	/* the address we have		*/
{
	struct translator	*t;	/* pointer to translator list	*/
	struct nd_hostservlist	*hs;	/* the results			*/
	char			*lr;	/* routines to try		*/
	int			i;	/* counts the routines		*/

	for (i = 0; i < tp->nc_nlookups; i++) {
		lr = *((tp->nc_lookups) + i);
		for (t = xlate_list; t; t = t->next) {
			if (strcmp(lr, t->tr_name) == 0) {
				hs = (*(t->gba))(tp, addr);
				if (hs) {
					*serv = hs;
					return (0);
				} else {
					if (_nderror < 0)
						return (_nderror);
					break;
				}
			}
		}
		/* If we didn't find it try loading it */
		if (!t) {
			if ((t = load_xlate(lr)) != NULL) {
				/* add it to the list */
				t->next = xlate_list;
				xlate_list = t;
				hs = (*(t->gba))(tp, addr);
				if (hs) {
					*serv = hs;
					return (0);
				} else {
					if (_nderror < 0)
						return (_nderror);
				}
			}
		}
	}

	return (_nderror);	/* No one works */
}

/*
 * This is the library routine for merging universal addresses with the
 * address of the client caller, so that a address which makes sense
 * can be returned to the client caller.
 * Again it uses the same code as below(uaddr2taddr)
 * to search for the appropriate translation routine. Only it doesn't
 * bother trying a whole bunch of routines since either the transport
 * can merge it or it can't.
 */
int
netdir_options(
	struct netconfig	*tp,	/* the netconfig entry		*/
	int			option,	/* option number		*/
	int			fd,	/* open file descriptor		*/
	char			*par)	/* parameters if any		*/
{
	struct translator	*t;	/* pointer to translator list	*/
	char			*lr,	/* routines to try		*/
				*x;	/* the answer			*/
	int			i;	/* counts the routines		*/

	for (i = 0; i < tp->nc_nlookups; i++) {
		lr = *((tp->nc_lookups) + i);
		for (t = xlate_list; t; t = t->next) {
			if (strcmp(lr, t->tr_name) == 0) {
				return ((*(t->opt))(tp, option, fd, par));
			}
		}
		/* If we didn't find it try loading it */
		if (!t) {
			if ((t = load_xlate(lr)) != NULL) {
				/* add it to the list */
				t->next = xlate_list;
				xlate_list = t;
				return ((*(t->opt))(tp, option, fd, par));
			}
		}
	}

	return (_nderror);	/* No one works */
}

/*
 * This is the library routine for translating universal addresses to
 * transport specific addresses. Again it uses the same code as above
 * to search for the appropriate translation routine. Only it doesn't
 * bother trying a whole bunch of routines since either the transport
 * can translate it or it can't.
 */
struct netbuf *
uaddr2taddr(
	struct netconfig	*tp,	/* the netconfig entry		*/
	char			*addr)	/* The address in question	*/
{
	struct translator	*t;	/* pointer to translator list	*/
	struct netbuf		*x;	/* the answer we want		*/
	char			*lr;	/* routines to try		*/
	int			i;	/* counts the routines		*/

	for (i = 0; i < tp->nc_nlookups; i++) {
		lr = *((tp->nc_lookups) + i);
		for (t = xlate_list; t; t = t->next) {
			if (strcmp(lr, t->tr_name) == 0) {
				x = (*(t->u2t))(tp, addr);
				if (x)
					return (x);
				if (_nderror < 0)
					return (0);
			}
		}
		/* If we didn't find it try loading it */
		if (!t) {
			if ((t = load_xlate(lr)) != NULL) {
				/* add it to the list */
				t->next = xlate_list;
				xlate_list = t;
				x = (*(t->u2t))(tp, addr);
				if (x)
					return (x);
				if (_nderror < 0)
					return (0);
			}
		}
	}

	return (0);	/* No one works */
}

/*
 * This is the library routine for translating transport specific
 * addresses to universal addresses. Again it uses the same code as above
 * to search for the appropriate translation routine. Only it doesn't
 * bother trying a whole bunch of routines since either the transport
 * can translate it or it can't.
 */
char *
taddr2uaddr(
	struct netconfig	*tp,	/* the netconfig entry		*/
	struct netbuf		*addr)	/* The address in question	*/
{
	struct translator	*t;	/* pointer to translator list	*/
	char			*lr;	/* routines to try		*/
	char			*x;	/* the answer			*/
	int			i;	/* counts the routines		*/

	for (i = 0; i < tp->nc_nlookups; i++) {
		lr = *((tp->nc_lookups) + i);
		for (t = xlate_list; t; t = t->next) {
			if (strcmp(lr, t->tr_name) == 0) {
				x = (*(t->t2u))(tp, addr);
				if (x)
					return (x);
				if (_nderror < 0)
					return (0);
			}
		}
		/* If we didn't find it try loading it */
		if (!t) {
			if ((t = load_xlate(lr)) != NULL) {
				/* add it to the list */
				t->next = xlate_list;
				xlate_list = t;
				x = (*(t->t2u))(tp, addr);
				if (x)
					return (x);
				if (_nderror < 0)
					return (0);
			}
		}
	}

	return (0);	/* No one works */
}

/*
 * This is the routine that frees the objects that these routines allocate.
 */
void
netdir_free(
	void	*ptr,	/* generic pointer	*/
	int	type)	/* thing we are freeing */
{
	struct netbuf		*na;
	struct nd_addrlist	*nas;
	struct nd_hostserv	*hs;
	struct nd_hostservlist	*hss;
	int			i;

	switch (type) {
	case ND_ADDR :
		na = (struct netbuf *) ptr;
		free(na->buf);
		free((char *)na);
		break;

	case ND_ADDRLIST :
		nas = (struct nd_addrlist *) ptr;
		for (na = nas->n_addrs, i = 0; i < nas->n_cnt; i++, na++) {
			free(na->buf);
		}
		free((char *)nas->n_addrs);
		free((char *)nas);
		break;

	case ND_HOSTSERV :
		hs = (struct nd_hostserv *) ptr;
		free(hs->h_host);
		free(hs->h_serv);
		free((char *)hs);
		break;

	case ND_HOSTSERVLIST :
		hss = (struct nd_hostservlist *) ptr;
		for (hs = hss->h_hostservs, i = 0; i < hss->h_cnt; i++, hs++) {
			free(hs->h_host);
			free(hs->h_serv);
		}
		free((char *)hss->h_hostservs);
		free((char *)hss);
		break;

	default :
		_nderror = ND_UKNWN;
		break;
	}
}

/*
 * load_xlate is a routine that will attempt to dynamically link in the
 * file specified by the network configuration structure.
 */
static struct translator *
load_xlate(
	char	*name)		/* file name to load */
{
	struct translator	*t;

	/* do a sanity check on the file ... */
	if (access(name, 00) != 0) {
		_nderror = ND_ACCESS;
		return (NULL);
	}
	t = (struct translator *) malloc(sizeof (struct translator));
	if (!t) {
		_nderror = ND_NOMEM;
		return (NULL);
	}
	t->tr_name = strdup(name);
	if (!t->tr_name) {
		_nderror = ND_NOMEM;
		free((char *)t);
		return (NULL);
	}

#if dlopen_works
	/* open for linking */
	t->tr_fd = _dlopen(name, RTLD_NOW);
	if (t->tr_fd == 0) {
		_nderror = ND_OPEN;
		goto error;
	}

	/* Resolve the getbyname symbol */
	t->gbn = (struct nd_addrlist *(*)())_dlsym(t->tr_fd,
				"_netdir_getbyname");
	if (!(t->gbn)) {
		_nderror = ND_NOSYM;
		goto error;
	}

	/* resolve the getbyaddr symbol */
	t->gba = (struct nd_hostservlist *(*)())_dlsym(t->tr_fd,
				"_netdir_getbyaddr");
	if (!(t->gba)) {
		_nderror = ND_NOSYM;
		goto error;
	}

	/* resolve the taddr2uaddr symbol */
	t->t2u = (char *(*)())_dlsym(t->tr_fd, "_taddr2uaddr");
	if (!(t->t2u)) {
		_nderror = ND_NOSYM;
		goto error;
	}

	/* resolve the uaddr2taddr symbol */
	t->u2t = (struct netbuf *(*)())_dlsym(t->tr_fd, "_uaddr2taddr");
	if (!(t->u2t)) {
		_nderror = ND_NOSYM;
		goto error;
	}

	/* resolve the netdir_options symbol */
	t->opt = (int (*)())_dlsym(t->tr_fd, "_netdir_options");
	if (!(t->opt)) {
		_nderror = ND_NOSYM;
		goto error;
	}
#else	/* dlopen doesn't work */
	t->tr_fd = 0;
	t->gbn =  _netdir_getbyname;
	t->gba =  _netdir_getbyaddr;
	t->t2u =  _taddr2uaddr;
	t->u2t =  _uaddr2taddr;
	t->opt =  _netdir_options;
#endif

	return (t);
error:
	free((char *)t);
	return (NULL);
}

static char	*errbuf;

static char *
_buf( void )
{
	if (errbuf == NULL)
		errbuf = (char *)malloc(128);
	return (errbuf);
}

/*
 * This is a routine that returns a string related to the current
 * error in _nderror.
 */
char *
netdir_sperror( void )
{
	char	*str = _buf();

	if (str == NULL)
		return (NULL);
	switch (_nderror) {
	case ND_NOMEM :
		(void) sprintf(str, "n2a: memory allocation failed");
		break;
	case ND_OK :
		(void) sprintf(str, "n2a: successful completion");
		break;
	case ND_NOHOST :
		(void) sprintf(str, "n2a: hostname not found");
		break;
	case ND_NOSERV :
		(void) sprintf(str, "n2a: service name not found");
		break;
	case ND_NOSYM :
		(void) sprintf(str, "n2a: symbol missing in shared object - %s",
			_dlerror());
		break;
	case ND_OPEN :
		(void) sprintf(str, "n2a: couldn't open shared object - %s",
			_dlerror());
		break;
	case ND_ACCESS :
		(void) sprintf(str, "n2a: access denied for shared object");
		break;
	case ND_UKNWN :
		(void) sprintf(str, "n2a: attempt to free unknown object");
		break;
	case ND_BADARG :
		(void) sprintf(str, "n2a: bad arguments passed to routine");
		break;
	case ND_NOCTRL:
		(void) sprintf(str, "n2a: unknown option passed");
		break;
	case ND_FAILCTRL:
		(void) sprintf(str, "n2a: control operation failed");
		break;
	case ND_SYSTEM:
		(void) sprintf(str, "n2a: system error: %s", strerror(errno));
	default :
		(void) sprintf(str, "n2a: unknown error #%d", _nderror);
		break;
	}
	return (str);
}

/*
 * This is a routine that prints out strings related to the current
 * error in _nderror. Like perror() it takes a string to print with a
 * colon first.
 */
void
netdir_perror(
	char	*s)
{
	char	*err;

	err = netdir_sperror();
	fprintf(stderr, "%s: %s\n", s, err ? err: "nd: error");
	return;
}
