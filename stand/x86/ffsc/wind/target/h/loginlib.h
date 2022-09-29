/* loginLib.h - user password/login subroutine library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01s,22sep92,rrr  added support for c++
01r,04jul92,jcf  cleaned up.
01q,26may92,rrr  the tree shuffle
01p,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01o,19aug91,rrr  fixed the ansi prototype for loginUserAdd
01d,01may91,jdi  added ifndef for DOC.
01c,21jan91,shl  fixed arguments type in loginUserAdd().
01b,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01a,18apr90,shl  written.
*/

#ifndef __INCloginLibh
#define __INCloginLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"

#ifndef DOC
#define MAX_LOGIN_NAME_LEN    80
#define MAX_LOGIN_ENTRY       80
#endif	/* DOC */

/* status messages */

#define S_loginLib_UNKNOWN_USER			(M_loginLib | 1)
#define S_loginLib_USER_ALREADY_EXISTS		(M_loginLib | 2)
#define S_loginLib_INVALID_PASSWORD		(M_loginLib | 3)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	loginDefaultEncrypt (char *in, char *out);
extern STATUS 	loginPrompt (char *userName);
extern STATUS 	loginUserAdd (char name [ 80 ], char passwd [ 80 ]);
extern STATUS 	loginUserDelete (char *name, char *passwd);
extern STATUS 	loginUserVerify (char *name, char *passwd);
extern void 	loginEncryptInstall (FUNCPTR rtn, int var);
extern void 	loginInit (void);
extern void 	loginStringSet (char *newString);
extern void 	loginUserShow (void);

#else	/* __STDC__ */

extern STATUS 	loginDefaultEncrypt ();
extern STATUS 	loginPrompt ();
extern STATUS 	loginUserAdd ();
extern STATUS 	loginUserDelete ();
extern STATUS 	loginUserVerify ();
extern void 	loginEncryptInstall ();
extern void 	loginInit ();
extern void 	loginStringSet ();
extern void 	loginUserShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloginLibh */
