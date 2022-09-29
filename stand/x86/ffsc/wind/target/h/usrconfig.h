/* usrConfig.h - header file for usrConfig.c */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02j,10oct95,dat	 removed #include "bspversion.h"
02i,09may95,ms	 prototype for WDB agent
02h,28mar95,kkk  changed edata and end to arrays (SPR #3917)
02g,25oct93,hdn  added prototype for floppy and IDE driver.
02m,22feb94,elh  added function defs for snmp. 
02m,25jul94,jmm  removed prototype of printConfig() (spr 3461)
02l,28jan94,dvs  changed INCLUDE_POSIX_MEM_MAN to INCLUDE_POSIX_MEM
02k,12jan94,kdl  changed aio includes.
02j,10dec93,smb  added include of seqDrvP.h  for windview
02i,07dec93,dvs  added #ifdef's around POSIX includes for component releases
02h,02dec93,dvs  added includes for POSIX modules
02g,22nov93,dvs  added include of sched.h
02f,23aug93,srh  added include of private/cplusLibP.h.
02e,20oct92,pme  added vxLib.h.
02d,22sep92,rrr  added support for c++
02c,21sep92,ajm  fixes for lack of #elif support on mips
02b,20sep92,kdl  added include of private/ftpLibP.h.
02a,18sep92,jcf  major clean up.  added prototypes and #includes.
01d,04jul92,jcf  cleaned up.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01a,05oct90,shl created.
*/

#ifndef __INCusrConfigh
#define __INCusrConfigh

#ifdef __cplusplus
extern "C" {
#endif

#include "bootlib.h"
#include "cachelib.h"
#include "ctype.h"
#include "dbglib.h"
#include "dosfslib.h"
#include "envlib.h"
#include "errno.h"
#include "esf.h"
#include "fcntl.h"
#include "fiolib.h"
#include "fpplib.h"
#include "ftplib.h"
#include "ftpdlib.h"
#include "hostlib.h"
#include "iflib.h"
#include "if_bp.h"
#include "if_sl.h"
#include "inetlib.h"
#include "intlib.h"
#include "iolib.h"
#include "ioslib.h"
#include "iv.h"
#include "kernellib.h"
#include "loadlib.h"
#include "loglib.h"
#include "loginlib.h"
#include "math.h"
#include "memlib.h"
#include "mmulib.h"
#include "msgqlib.h"
#include "netdrv.h"
#include "netlib.h"
#include "netshow.h"
#include "xdr_nfs.h"
#include "nfsdrv.h"
#include "nfslib.h"
#include "pipedrv.h"
#include "proxyarplib.h"
#include "qlib.h"
#include "qpribmaplib.h"
#include "ramdrv.h"
#include "rawfslib.h"
#include "rebootlib.h"
#include "remlib.h"
#include "rloglib.h"
#include "routelib.h"
#include "rpclib.h"
#include "rt11fslib.h"
#include "scsilib.h"
#include "selectlib.h"
#include "semlib.h"
#include "shelllib.h"
#include "smobjlib.h"
#include "spylib.h"
#include "stdio.h"
#include "string.h"
#include "syslib.h"
#include "syssymtbl.h"
#include "taskhooklib.h"
#include "tasklib.h"
#include "taskvarlib.h"
#include "telnetlib.h"
#include "tftplib.h"
#include "tftpdlib.h"
#include "ticklib.h"
#include "time.h"
#include "timers.h"
#include "timexlib.h"
#include "unistd.h"
#include "unldlib.h"
#include "usrlib.h"
#include "version.h"
#include "vxlib.h"
#include "wdlib.h"
#include "net/mbuf.h"
#include "drv/netif/smnetlib.h"
#include "rpc/rpcgbl.h"
#include "rpc/rpctypes.h"
#include "rpc/xdr.h"
#include "private/cpluslibp.h"
#include "private/ftplibp.h"
#include "private/kernellibp.h"
#include "private/siglibp.h"
#include "private/vmlibp.h"
#include "private/workqlibp.h"

#ifdef	INCLUDE_POSIX_AIO
#include "aio.h"
#ifdef	INCLUDE_POSIX_AIO_SYSDRV 
#include "aiosysdrv.h"
#endif  /* INCLUDE_POSIX_AIO_SYSDRV */
#endif	/* INCLUDE_POSIX_AIO */

#ifdef	INCLUDE_POSIX_MQ 
#include "private/mqpxlibp.h"
#ifdef	INCLUDE_SHOW_ROUTINES
#include "mqpxshow.h"
#endif  /* INCLUDE_SHOW_ROUTINES */
#endif  /* INCLUDE_POSIX_MQ */

#ifdef	INCLUDE_POSIX_SCHED
#include "sched.h"
#endif	/* INCLUDE_POSIX_SCHED */

#ifdef	INCLUDE_POSIX_SEM
#include "private/sempxlibp.h"
#ifdef	INCLUDE_SHOW_ROUTINES
#include "sempxshow.h"
#endif  /* INCLUDE_SHOW_ROUTINES */
#endif	/* INCLUDE_POSIX_SEM */

#ifdef	INCLUDE_POSIX_MEM
#include "sys/mman.h"
#endif  /* INCLUDE_POSIX_MEM */

#if 	defined(INCLUDE_INSTRUMENTATION)
#include "wvlib.h"
#include "wvtmrlib.h"   
#include "connlib.h"   
#include "private/seqdrvp.h"   
#include "private/evtsocklibp.h"   
#endif  /* INCLUDE_INSTRUMENTATION */

#if     defined(INCLUDE_AOUT)
#include "loadaoutlib.h"
#else   /* coff or ecoff */
#if     defined(INCLUDE_ECOFF)
#include "loadecofflib.h"
#else   /* coff */
#if     defined(INCLUDE_COFF)
#include "loadcofflib.h"
#endif                                          /* mips cpp no elif */
#endif
#endif

/* variable declarations */

extern char	edata [];			/* automatically defined by loader */
extern char	end [];			/* automatically defined by loader */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void	usrInit (int startType);
extern void	usrRoot (char *pMemPoolStart, unsigned memPoolSize);
extern void	usrClock (void);
extern STATUS	usrScsiConfig (void);
extern STATUS   usrFdConfig (int type, int drive, char *fileName);
extern STATUS   usrIdeConfig (int drive, char *fileName);
extern void     devSplit (char *fullFileName, char *devName);
extern STATUS	usrNetInit (char *bootString);
extern STATUS	checkInetAddrField (char *pInetAddr, BOOL subnetMaskOK);
extern STATUS	loadSymTbl (char *symTblName);
extern STATUS	netLoadSymTbl (void);
extern STATUS	usrBootLineCrack (char *bootString, BOOT_PARAMS *pParams);
extern void	usrBootLineInit (int startType);
extern STATUS	usrBpInit (char * devName, u_long startAddr);
extern void	usrDemo (void);
extern STATUS	usrNetIfAttach (char *devName, char *inetAdrs);
extern STATUS	usrSlipInit (char * pBootDev, char * localAddr, char *peerAddr);
extern STATUS	usrSmObjInit (char * bootString);
extern void	usrStartupScript (char *fileName);
extern STATUS	usrNetIfConfig (char *devName, char *inetAdrs, char *inetName,
				int netmask);
extern STATUS   usrSnmpdInit (void);
extern STATUS 	usrMib2Init (void);
extern STATUS 	usrMib2CleanUp (void);
extern STATUS	wdbConfig (void);

#else	/* __STDC__ */

extern void	usrInit ();
extern void	usrRoot ();
extern void	usrClock ();
extern STATUS	usrScsiConfig ();
extern STATUS   usrFdConfig ();
extern STATUS   usrIdeConfig ();
extern void     devSplit ();
extern STATUS	usrNetInit ();
extern STATUS	checkInetAddrField ();
extern STATUS	loadSymTbl ();
extern STATUS	netLoadSymTbl ();
extern STATUS	usrBootLineCrack ();
extern void	usrBootLineInit ();
extern STATUS	usrBpInit ();
extern void	usrDemo ();
extern STATUS	usrNetIfAttach ();
extern STATUS	usrSlipInit ();
extern STATUS	usrSmObjInit ();
extern void	usrStartupScript ();
extern STATUS	usrNetIfConfig ();
extern STATUS   usrSnmpdInit ();
extern STATUS 	usrMib2Init ();
extern STATUS 	usrMib2CleanUp ();
extern STATUS 	wdbConfig ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCusrConfigh */
