/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __NET_EXTERN_H__
#define __NET_EXTERN_H__

/*
	Externs only in the net directory and are not
	"public".
*/

#include <sys/types.h>

/* inet_addr.c */
extern int inet_isaddr(const char *, __uint32_t *);

/* gethostnam.c */
extern struct hostent *_gethtbyname(const char *);
extern struct hostent *_gethtbyaddr(const char *, int, int);
extern struct hostent *_inet_atohtent(const char *);
extern struct hostent *_gethostbyname_named(const char *);
extern struct hostent *_gethostbyaddr_named(const char *, int, int);

/* gethostent.c */
extern struct hostent *_gethostbyname_yp(const char *);
extern struct hostent *_gethostbyaddr_yp(const char *, int, int);

/* gethostent.c */
extern int _setresordsubr(const char *);

/* ruserpass.c */
extern void ruserpass(char *, char **, char **);

/*
 * A structure for many of the formerly static data items
 */
extern struct netstuff {
	char res_abuf[256];
	char res_type[16];
	char res_class[16];
	char res_ttl[40];
 } *__netstuff;

#define GET_NETSTUFF (__netstuff ? __netstuff : (__netstuff = malloc(sizeof (struct netstuff))))

#endif
