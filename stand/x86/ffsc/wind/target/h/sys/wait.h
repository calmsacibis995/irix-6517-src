/* wait.h - Unix compatible wait structures for remote debugging support */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,17nov87,dnw  removed unnecessary stuff.
*/

#ifndef __INCwaith
#define __INCwaith

#ifdef __cplusplus
extern "C" {
#endif

/*
 * If w_stopval==WSTOPPED, then the second structure
 * describes the information returned, else the first.
 */
union wait
    {
    int	w_status;

    /* Terminated process status. */

    struct
	{
	unsigned short	w_Fill1:16;	/* high 16 bits unused */
	unsigned short	w_Retcode:8;	/* exit code if w_termsig==0 */
	unsigned short	w_Coredump:1;	/* core dump indicator */
	unsigned short	w_Termsig:7;	/* termination signal */
	} w_T;

    /* Stopped process status. */

    struct
	{
	unsigned short	w_Fill2:16;	/* high 16 bits unused */
	unsigned short	w_Stopsig:8;	/* signal that stopped us */
	unsigned short	w_Stopval:8;	/* == W_STOPPED if stopped */
	} w_S;
    };
#define	w_termsig	w_T.w_Termsig
#define w_coredump	w_T.w_Coredump
#define w_retcode	w_T.w_Retcode
#define w_stopval	w_S.w_Stopval
#define w_stopsig	w_S.w_Stopsig

#define	WSTOPPED	0177	/* value of s.stopval if process is stopped */

#ifdef __cplusplus
}
#endif

#endif /* __INCwaith */
