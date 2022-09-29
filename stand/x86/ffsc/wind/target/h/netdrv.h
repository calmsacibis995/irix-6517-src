/* netDrv.h - netDrv header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01l,13mar95,dzb  added param to netLsByName() prototype (SPR #3662).
01k,22jul93,jmm  added declaration for netLsByName() (spr 2305)
01j,22sep92,rrr  added support for c++
01i,04jul92,jcf  cleaned up.
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01f,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01e,08aut90,shl  added INCnetDrvh to #endif.
01d,08jan87,jlf  added ifndef's to keep from being included twice.
01c,24dec86,gae  changed stsLib.h to vwModNum.h.
01b,27jul86,llk  added S_netDrv_NO_SUCH_FILE_OR_DIR, S_netDrv_PERMISSION_DENIED,
		 S_netDrv_IS_A_DIRECTORY, S_netDrv_UNIX_FILE_ERROR
01a,30apr86,llk  written.
*/

#ifndef __INCnetDrvh
#define __INCnetDrvh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"

/* netDrv status codes */

#define S_netDrv_INVALID_NUMBER_OF_BYTES	(M_netDrv | 1)
#define S_netDrv_SEND_ERROR			(M_netDrv | 2)
#define S_netDrv_RECV_ERROR			(M_netDrv | 3)
#define S_netDrv_UNKNOWN_REQUEST		(M_netDrv | 4)
#define S_netDrv_BAD_SEEK			(M_netDrv | 5)
#define S_netDrv_SEEK_PAST_EOF_ERROR		(M_netDrv | 6)
#define S_netDrv_BAD_EOF_POSITION		(M_netDrv | 7)
#define S_netDrv_SEEK_FATAL_ERROR		(M_netDrv | 8)
#define	S_netDrv_WRITE_ONLY_CANNOT_READ		(M_netDrv | 9)
#define	S_netDrv_READ_ONLY_CANNOT_WRITE		(M_netDrv | 10)
#define	S_netDrv_READ_ERROR			(M_netDrv | 11)
#define S_netDrv_WRITE_ERROR			(M_netDrv | 12)
#define S_netDrv_NO_SUCH_FILE_OR_DIR		(M_netDrv | 13)
#define S_netDrv_PERMISSION_DENIED		(M_netDrv | 14)
#define S_netDrv_IS_A_DIRECTORY			(M_netDrv | 15)
#define S_netDrv_UNIX_FILE_ERROR		(M_netDrv | 16)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	netDevCreate (char *devName, char *host, int protocol);
extern STATUS 	netDrv (void);
extern STATUS   netLsByName (char *dirName);

#else	/* __STDC__ */

extern STATUS 	netDevCreate ();
extern STATUS 	netDrv ();
extern STATUS   netLsByName ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCnetDrvh */
