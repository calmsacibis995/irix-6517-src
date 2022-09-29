#ifndef __RPCSVC_RUSERS_H__
#define __RPCSVC_RUSERS_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.8 $"
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
/*	@(#)rusers.h	1.2 88/05/08 4.0NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.6
 */

#define RUSERSPROC_NUM 1
#define RUSERSPROC_NAMES 2
#define RUSERSPROC_ALLNAMES 3
#define RUSERSPROG 100002
#define RUSERSVERS_ORIG 1
#define RUSERSVERS_IDLE 2
#define RUSERSVERS 2

#define MAXUSERS 100

struct rusers_utmp {
	char    ut_line[8];             /* tty name */
	char    ut_name[8];             /* user id */
	char    ut_host[16];            /* host name, if remote */
	long    ut_time;                /* time on */
};

struct utmparr {
	struct rusers_utmp **uta_arr;
	int uta_cnt;
};

struct utmpidle {
	struct rusers_utmp ui_utmp;
	unsigned ui_idle;
};

struct utmpidlearr {
	struct utmpidle **uia_arr;
	int uia_cnt;
};

extern bool_t xdr_utmp(XDR *, struct rusers_utmp *);
extern bool_t xdr_utmpptr(XDR *, struct rusers_utmp **);
extern bool_t xdr_utmparr(XDR *, struct utmparr *);
extern bool_t xdr_utmpidle(XDR *, struct utmpidle *);
extern bool_t xdr_utmpidleptr(XDR *, struct utmpidle **);
extern bool_t xdr_utmpidlearr(XDR *, struct utmpidlearr *);

extern int rusers(const char *, struct utmpidlearr *);
extern int rnusers(const char *);
#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_RUSERS_H__ */
