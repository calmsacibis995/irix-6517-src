/* remLib.h - structures for remLib.c */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,15sep92,jcf  added variable declaration.
02a,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01g,11jan91,shl  inlcuded "in.h" for sockaddr_in.
01f,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01e,28may88,dnw  removed some status values to hostLib.h.
01d,01nov87,llk  added S_remLib_HOST_ALREADY_ENTERED.
01c,10oct86,gae  deleted S_remLib_DIRECTORY_NAME_TOO_BIG, not used anymore;
		 spelled S_remLib_ILLEAGAL_INTERNET_ADDRESS correctly.
		 moved HOST and HOSTNAME structs to remLib.c for better hiding.
		 added conditional INCremLibh.  Defined MAX_IDENTITY_LEN.
01b,21may86,llk  deleted S_remLib_WRITE_ERROR, not used anymore.
01a,19sep85,rdc  written.
*/

#ifndef __INCremLibh
#define __INCremLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "netinet/in.h"

#define MAX_IDENTITY_LEN	100

/* status messages */

#define S_remLib_ALL_PORTS_IN_USE		(M_remLib | 1)
#define S_remLib_RSH_ERROR			(M_remLib | 2)
#define S_remLib_IDENTITY_TOO_BIG		(M_remLib | 3)

/* variable declarations */

extern int	remLastResvPort;        /* last port num used (from bootroms) */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	bindresvport (int sd, struct sockaddr_in *sin);
extern STATUS 	iam (char *newUser, char *newPasswd);
extern STATUS 	remCurIdSet (char *newUser, char *newPasswd);
extern int 	rcmd (char *host, int remotePort, char *localUser,
		      char *remoteUser, char *cmd, int *fd2p);
extern int 	rresvport (int *alport);
extern void 	remCurIdGet (char *user, char *passwd);
extern void 	whoami (void);

#else	/* __STDC__ */

extern STATUS 	bindresvport ();
extern STATUS 	iam ();
extern STATUS 	remCurIdSet ();
extern int 	rcmd ();
extern int 	rresvport ();
extern void 	remCurIdGet ();
extern void 	whoami ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCremLibh */
