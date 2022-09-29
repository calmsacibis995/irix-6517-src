/* bootConfig.c - system configuration module for boot ROMs */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
08i,11oct95,dat	 new BSP revision id, modified signon banner printing
08h,26jun95,ms	 updated for new serial drivers, removed WDB support.
08g,21jun95,ms	 changed copywrite to 1995
08f,22may95,p_m  added WDB agent support.
          + ms
08e,05apr95,kkk  changed edata & end to char arrays (spr# 3917)
08d,28mar95,kkk  made baud rate a macro in configAll.h
08c,19mar95,dvs  removed TRON references.
08b,08dec94,hdn  swapped 1st and 2nd parameters of fdDevCreate().
		 fixed bootHelp message; fd=... and ide=...
08a,21nov94,hdn  fixed a problem by swapping 1st and 2nd parameters of sscanf.
07z,20nov94,kdl  added hashLibInit() call if using dosFs.
07y,11nov94,dzb  added QU network interface.
07x,17aug94,dzb  fixed setting the gateway for a slip connection (SPR #2353).
                 added INCLUDE_NETWORK macros for scalability (SPR #1147).
 		 added CSLIP support.
08c,09nov94,jds  additions for scsi backward compatability ; scsi[12]IfInit()
08b,20oct94,hdn  used ideRawio() instead of using raw file system.
		 swapped 1st and 2nd parameter of ideLoad() and fdLoad().
08a,14jun94,caf  updated copyright notices.
07z,25oct93,cd   added support for ELF loader.
07y,08oct93,caf  changed MIPS to call sysGpInit() instead of sysInitAlt(),
		 added attach support for "egl" driver.
07x,12sep93,caf  fixed parameter cast for kernelInit(), changed MIPS to clear
		 bss and call sysInitAlt(), updated copyright notice.
08a,29may94,hdn  fixed more FTP bootrom bug in netLoad().
		 updated the copyright year 93 to 94.
		 disabled cache for i486 and Pentium.
07z,10may94,hdn  fixed the FTP bootrom bug (John's patch)
07y,09feb94,hdn  added support for if_elt 3COM EtherLink III driver.
		 added support for if_ene Eagle NE2000 driver.
07x,15nov93,hdn  added support for Thin Ethernet (10Base2, BNC).
		 added support for SMC Ultra ethernet driver.
		 added support for floppy and IDE device.
		 added support for I80X86.
07z,26may94,kdl  changed netLoad() to properly close FTP connections (SPR 3231).
07x,09dec93,pad  added Am29K family support.
07w,22sep93,jmm  added #include for scsiLib.h (spr 1965)
07v,30aug93,jag  added host and backpl structures.  This allows for implicit
		 passing of the strucs to in_netof() (SPR #2068).
07u,12feb93,jwt  added workaround for SPARClite cache for boot ROMs.
07t,12feb93,dzb  included additional header files.  updated copyright.
07s,03feb93,wmd  fixed display of ethernet address in N command (spr# 1961).
07r,28jan93,caf  undid 06c: call sysNvRamSet() and sysNvRamGet() w/offset 0.
07q,18jan93,wmd  added mEnet() and code in bootCmdLoop to set the last 3 bytes 
		 of a board's ethernet address.  Define ETHERNET_ADR_SET and 
		 ENET_DFAULT in config.h to conditionally compile it. 
07p,11jan93,jdi  documentation cleanup.
07o,08dec92,rfs  Undid 07n, and instead added method to add a new driver
                 without modifying this file, via IF_USR.
07n,08dec92,rfs  Added attach support for "eitp" driver.
07m,23oct92,ccc  changed I960 to call sysInitAlt(). fixed mangen problem.
07l,22oct92,jcf  disabled data cache to avoid driver mmu requirements.
07k,19oct92,jcf  added USER_[ID]_CACHE_ENABLE.
07j,01oct92,jcf  reworked cache initialization.  disabled cache in go().
07i,25sep92,ccc  removed second parameter for scsiAutoConfig(), only call
		 sysScsiInit() once.
07h,24sep92,jcf  removed relative pathnames.
07g,24sep92,wmd  removed I960 bzero() invocation to clear RAM.
07f,23sep92,kdl  changed INCLUDE_PROXYCLNT to INCLUDE_PROXY_CLIENT;
		 added bootp, tftp and proxy arp boot flags to bootHelp().
07e,22sep92,rdc  temp mod - disabled cache for sparc (mmu interaction).
07d,22sep92,jcf  added compressedEntry() to clean up bootstrapping issues.
07c,21sep92,ajm  fixes for lack of #elif support on mips
07b,18sep92,jcf  moved usrKernel.c farther down the file.
07a,18sep92,jcf  major cleanup.
06e,10sep92,caf  added BSP version to printBootLogo().
		 removed ethernet address kludge for frc30, frc37 and frc40.
06d,03sep92,ccc  added OMTI3500 initialization in SCSI boot, clean warnings.
		 added exattach() and enpattach() AM parameter.
06c,02sep92,caf  changed to call sysNvRamSet() and sysNvRamGet() with
		 NV_BOOT_OFFSET.
06b,01sep92,kdl  changed copyright date in boot banner to 1992.
06a,14aug92,wmd  removed the I960 conditionals in go(), and the divide
		 by 4 for taskDelay().
05z,08aug92,ajm  removed BUS_SNOOP, added lnsgi ethernet interface
05y,31jul92,ccc  moved tyCoDrv() and tyCoDevCreate() prototypes to sysLib.h.
05x,30jul92,jmm  changed INCLUDE_TFTP to INCLUDE_TFTP_CLIENT
05w,30jul92,jwt  changed MIPS and SPARC tyCoDevCreate() buffers to 512.
05v,29jul92,elh  Added default to include both backplane drivers.
05u,28jul92,rdc  removed definition of vmLibInfo.
05t,27jul92,jcf  changed qInit call for ready queue.
05s,25jul92,elh  changed BP macros to SM.
05r,23jul92,jwt  added fn (Fujitsu NICE) ethernet driver.
05q,21jul92,rrr  removed any reference to signals.
05p,19jul92,rrr  fixed scalable configuration, added taskHookInit(),
                   wdLibInit() and memInit(). John can prune it down.
05o,16jul92,rfs  added NULL ptr check for call to if_init routine.
05n,14jul92,jcf  added scalable configuration.
		 added vmLibInfo structure. 
05m,11jul92,jwt  added 5.1 cache support; deleted old cache code; deleted 
                 TARGET_SUN1E code in usrRoot(); removed compiler warnings.
05l,04jul92,jcf  added scalable configuration.
05k,30jun92,wmd  changed loadBoutInit() to bootBoutInit().
05j,23jun92,elh  changed netLoad to take host address instead of 
		 host name.  Changed parameters to bootpParamsGet.
		 added INCLUDE_BOUT (for rrr).
05i,09jun92,ajm  5.0.5 mips merge, object module independance
05h,04jun92,wmd  added absolute path for smNetLib.h.
05g,02jun92,elh  the tree shuffle
		  -changed includes to have absolute path from h/
05f,27may92,elh  renamed smVx calls to smNet.
05e,03may92,elh  changed parameters to smVxInit.
05d,27apr92,ccc  added SCSI booting.
05c,23apr92,elh  added usrSlipInit, changed boothelp to not show loopback as
		 a boot device.
05b,22apr92,yao  removed if conditionals of trcStack() call.
05a,16apr92,elh  Added in proxy arp.
04z,11apr92,elh  introduced usrBpInit.  Added some headers for ANSI.
		 Added sequential backplane addressing.
04y,04apr92,elh  Added in new backplane driver.
04x,01mar92,elh  Added TFTP.
04w,28feb92,elh  Added BOOTP.
04v,08jan92,jcf  reworked cache configuration by removing cacheReset().
		 added cacheClear to push out data from copyback caches in go().
            shl  added BUS_SNOOP
		 added construction of ethernet address for frc40.
04u,16dec91,hdn  added a cacheDisable() if it is EXCELAN, for TRON.
04t,05dec91,yao  added statSymTbl. removed unused slipBoot.
04s,25nov91,jpb  changed to ignore subnet masks for SLIP.
		 fixed some spelling errors.
04r,08nov91,rfs  added support for SN network driver.
04q,06nov91,rrr  moved the struct netIf after usrInit so usrInit would be
		  the first thing, it makes the bootroms happy.
		  Also added some forward decls to shut up some warnings.
04p,06nov91,rrr  removed sparc mapping tables (now in excLib.c)
04o,15oct91,rrr  fixed a syntax error in SLIP stuff.
04n,11oct91,shl  reorganized 68k cache initialization.
04m,08oct91,elh  split usrNetIfConfig into usrNetIf{Attach,Config}, moved slip
04l,07oct91,rrr  fixed some bad parameter passing
04k,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
04j,26sep91,yao  added CPU32 support.
04i,19sep91,ajm  added MIPS support.
04h,13sep91,jpb  added DMA address modifier to exattach argument list.
04g,14aug91,yao  added configuration for slip. added hostAdrs parameter to
		 to usrNetIfConfig () call. removed eiEnetAddr[].
04f,09aug91,hdn  enabled STOREBUFFER for G200.
04e,01aug91,gae  changed args for eiattach() for new ei driver.
04d,13jul91,gae  moved i960 PND & IMR bit specific code to BSPs.
		 fixed TRON typos.
04c,11jul91,gae  fixed cache ifdef's for non-68k architectures.
04b,08jul91,del  added isascii to printExcMsg.
04a,04jul91,gae  deleted NV_RAM_SIZE usage accidentally re-introduced in 03x --
		 resulted in sysNvRamSet not being called in some instances.
03z,20jun91,hdn  fixed #ifdef CPU_FAMILY==TRON, to #if.
03y,18jun91,hdn  added G100 specifics; set PSW in vector table for network if's.
03x,21may91,jwt  merged with SPARC bootConfig.c.
03w,19may91,gae  moved BOOT_LINE_SIZE to configAll.h.
03v,18may91,gae  changed NV_RAM_SIZE to BOOT_LINE_SIZE - now fixed at 256.
03u,09may91,hdn  added TRON definitions and macros.
03t,01may91,del  added I960 specifics and 82596 (if_ei.c) references.
03s,25apr91,elh  added mbufConfig and clusterConfig.
03r,05apr91,jdi	 documentation -- removed header parens and x-ref numbers;
		 doc review by dnw.
*/

/*
DESCRIPTION
This is the WRS-supplied configuration module for the VxWorks boot ROM.
It is a stripped-down version of usrConfig.c, having no VxWorks shell or
debugging facilities.  Its primary function is to load an object module
over the network with either RSH or FTP.  Additionally, a simple set of
single letter commands is provided for displaying and modifying memory
contents.  Use this module as a starting point for placing applications 
in ROM.
*/

#include "vxworks.h"
#include "bootecofflib.h"
#include "bootlib.h"
#include "bootloadlib.h"
#include "bootplib.h"
#include "cachelib.h"
#include "ctype.h"
#include "dosfslib.h"
#include "errno.h"
#include "errnolib.h"
#include "fcntl.h"
#include "fiolib.h"
#include "ftplib.h"
#include "hostlib.h"
#include "icmplib.h"
#include "iflib.h"
#include "if_bp.h"
#include "if_sl.h"
#include "inetlib.h"
#include "intlib.h"
#include "iolib.h"
#include "ioslib.h"
#include "loadaoutlib.h"
#include "loadcofflib.h"
#include "loadlib.h"
#include "loglib.h"
#include "memlib.h"
#include "msgqlib.h"
#include "netlib.h"
#include "pipedrv.h"
#include "proxylib.h"
#include "qlib.h"
#include "qpribmaplib.h"
#include "rebootlib.h"
#include "remlib.h"
#include "routelib.h"
#include "scsilib.h"
#include "semlib.h"
#include "stdio.h"
#include "string.h"
#include "syslib.h"
#include "syssymtbl.h"
#include "taskhooklib.h"
#include "tasklib.h"
#include "tftplib.h"
#include "ticklib.h"
#include "trclib.h"
#include "unistd.h"
#include "version.h"
#include "wdlib.h"
#include "net/if.h"
#include "net/mbuf.h"
#include "netinet/if_ether.h"
#include "drv/netif/smnetlib.h"
#include "private/kernellibp.h"
#include "private/workqlibp.h"
#include "config.h"


/* defines */

#define TIMEOUT		7	/* number of seconds before auto-boot */
#define MAX_LINE        160	/* max line length for input to 'm' routine */
#define RSHD		514	/* rshd service */
#define DEC		FALSE	/* getArg parameters */
#define HEX		TRUE
#define OPT		TRUE
#define MAX_ADR_SIZE 	6 
#define DOS_ID_OFFSET                   3
#define FIRST_PARTITION_SECTOR_OFFSET   (0x1be + 8)
#define VXDOS                           "VXDOS"
#define VXEXT                           "VXEXT"


/* DO NOT ADD ANYTHING BEFORE THE FIRST ROUTINE compressedEntry() */

void		usrInit ();
IMPORT void 	sysInitAlt ();
#if	(CPU_FAMILY==MIPS)
IMPORT void 	sysGpInit ();
#endif	/* (CPU_FAMILY==MIPS) */

#ifdef  INCLUDE_NETWORK
#ifdef ETHERNET_ADR_SET
void		mEnet ();
void 		sysEnetAddrGet ();
void 		sysEnetAddrSet ();
#endif  /* ETHERNET_ADR_SET */
#endif  /* INCLUDE_NETWORK */


/*******************************************************************************
*
* compressedEntry - compressed entry point after decompression 
*
* This routine is the entry point after the bootroms decompress, if
* compression is utilized.  This routine must be the first item of the
* text segment of this file.  With ANSI C, strings will appear in text
* segment so one should avoid entering strings, routines, or anything
* else that might displace this routine from base of the text segment.
*
* It is unwise to add functionality to this routine without due cause.
* We are in the prehistoric period of system initialization.  
*
* NOMANUAL
*/

void compressedEntry 
    (
    int startType
    )
    {

#if (CPU_FAMILY==MIPS)
#if __GNUC__
    __asm volatile (".extern _gp,0; la $gp,_gp");
#endif
#endif

#if	(CPU_FAMILY==I960)
    sysInitAlt (startType);		/* jump to the i960 entry point */
#else
    usrInit (startType);		/* all others procede below */
#endif
    }


/* Wind kernel configuration facility */

#undef	INCLUDE_SHOW_ROUTINES		/* keep out kernel show routines */
#include "usrkernel.c"			/* kernel configuration facility */

/* network interface table definition and initialization*/

typedef struct  /* NETIF - network interface table */
    {
    char *ifName;  		     	/* interface name */
    FUNCPTR attachRtn;  		/* attach routine */
    char *arg1;				/* address */
    int arg2;				/* vector */
    int arg3;				/* level */
    int arg4;
    int arg5;
    int arg6;
    int arg7;
    int arg8;
    } NETIF;

/* imports */

IMPORT char	edata [];			/* defined by the loader */
IMPORT char	end [];			/* defined by the loader */

#ifdef  INCLUDE_NETWORK

IMPORT int	eglattach ();
IMPORT int	eiattach ();
IMPORT int	exattach ();
IMPORT int	enpattach ();
IMPORT int	ieattach ();
IMPORT int	lnattach ();
IMPORT int	lnsgiattach ();
IMPORT int	nicattach ();
IMPORT int	medattach ();
IMPORT int      elcattach ();
IMPORT int      ultraattach ();
IMPORT int      eexattach ();
IMPORT int      eltattach ();
IMPORT int      eneattach ();
IMPORT int	quattach ();
IMPORT int	loattach ();
IMPORT int	snattach ();
IMPORT int	fnattach ();
IMPORT int	bpattach ();
IMPORT STATUS	slipInit ();
IMPORT int	ifreset ();
IMPORT void	if_dettach ();
IMPORT u_long	in_netof ();
IMPORT struct ifnet *	ifunit ();

#ifdef	INCLUDE_IF_USR
IMPORT int IF_USR_ATTACH ();
#endif	/* INCLUDE_IF_USR */

LOCAL NETIF netIf [] =
    {
#ifdef	INCLUDE_IF_USR
        { IF_USR_NAME, IF_USR_ATTACH, IF_USR_ARG1, IF_USR_ARG2, IF_USR_ARG3,
          IF_USR_ARG4, IF_USR_ARG5, IF_USR_ARG6, IF_USR_ARG7, IF_USR_ARG8 },
#endif	/* INCLUDE_IF_USR */
#ifdef	INCLUDE_EGL
	{ "egl", eglattach, (char*)IO_ADRS_EGL, INT_VEC_EGL, INT_LVL_EGL },
#endif	/* INCLUDE_EGL */
#ifdef	INCLUDE_EI
        { "ei", eiattach, (char*)INT_VEC_EI, EI_SYSBUS, EI_POOL_ADRS, 0, 0},
#endif	/* INCLUDE_EI */
#ifdef  INCLUDE_EX
        { "ex", exattach, (char*)IO_ADRS_EX, INT_VEC_EX, INT_LVL_EX,
	  IO_AM_EX_MASTER, IO_AM_EX },
#endif	/* INCLUDE_EX */
#ifdef  INCLUDE_ENP
        { "enp", enpattach, (char*)IO_ADRS_ENP, INT_VEC_ENP, INT_LVL_ENP,
	  IO_AM_ENP },
#endif	/* INCLUDE_ENP */
#ifdef  INCLUDE_IE
        { "ie", ieattach, (char*)IO_ADRS_IE, INT_VEC_IE, INT_LVL_IE },
#endif	/* INCLUDE_IE */
#ifdef  INCLUDE_LN
        { "ln", lnattach, (char*)IO_ADRS_LN, INT_VEC_LN, INT_LVL_LN,
          LN_POOL_ADRS, LN_POOL_SIZE, LN_DATA_WIDTH, LN_PADDING,
	  LN_RING_BUF_SIZE },
#endif	/* INCLUDE_LN */
#ifdef  INCLUDE_LNSGI
        { "lnsgi", lnsgiattach, (char*)IO_ADRS_LNSGI, INT_VEC_LNSGI,
          INT_LVL_LNSGI, LNSGI_POOL_ADRS, LNSGI_POOL_SIZE, LNSGI_DATA_WIDTH,
          LNSGI_PADDING, LNSGI_RING_BUF_SIZE },
#endif  /* INCLUDE_LNSGI */
#ifdef  INCLUDE_NIC
        { "nic", nicattach, (char*)IO_ADRS_NIC, INT_VEC_NIC, INT_LVL_NIC },
#endif	/* INCLUDE_NIC */
#ifdef  INCLUDE_MED
        { "med", medattach, (char*)IO_ADRS_DBETH, INT_VEC_DBETH, INT_LVL_DBETH},
#endif	/* INCLUDE_MED */
#ifdef  INCLUDE_ELC
	{ "elc", elcattach, (char*)IO_ADRS_ELC, INT_VEC_ELC, INT_LVL_ELC,
	  MEM_ADRS_ELC, MEM_SIZE_ELC, CONFIG_ELC},
#endif  /* INCLUDE_ELC */
#ifdef  INCLUDE_ULTRA
	{ "ultra", ultraattach, (char*)IO_ADRS_ULTRA, INT_VEC_ULTRA,
	  INT_LVL_ULTRA, MEM_ADRS_ULTRA, MEM_SIZE_ULTRA, CONFIG_ULTRA},
#endif  /* INCLUDE_ULTRA */
#ifdef  INCLUDE_EEX
	{ "eex", eexattach, (char*)IO_ADRS_EEX, INT_VEC_EEX, INT_LVL_EEX,
	  NTFDS_EEX, CONFIG_EEX},
#endif  /* INCLUDE_EEX */
#ifdef  INCLUDE_ELT
	{ "elt", eltattach, (char*)IO_ADRS_ELT, INT_VEC_ELT, INT_LVL_ELT,
	  NRF_ELT, CONFIG_ELT},
#endif  /* INCLUDE_ELT */
#ifdef  INCLUDE_ENE
	{ "ene", eneattach, (char*)IO_ADRS_ENE, INT_VEC_ENE, INT_LVL_ENE},
#endif  /* INCLUDE_ELT */
#ifdef  INCLUDE_QU
	{ "qu", quattach, (char*)IO_ADRS_QU_EN, INT_VEC_QU_EN, QU_EN_SCC,
          QU_EN_TX_BD, QU_EN_RX_BD, QU_EN_TX_OFF, QU_EN_RX_OFF, QU_EN_MEM},
#endif  /* INCLUDE_QU */
#ifdef  INCLUDE_SN
        { "sn", snattach, (char*)IO_ADRS_SN, INT_VEC_SN },
#endif	/* INCLUDE_SN */
#ifdef  INCLUDE_FN
    { "fn", fnattach },
#endif  /* INCLUDE_FN */

#ifdef  INCLUDE_SM_NET
        { "bp", bpattach,   0, 0, 0, 0 , 0, 0 },
        { "sm", smNetAttach, 0, 0, 0, 0,  0, 0 },
#endif  /* INCLUDE_SM_NET */

#ifdef  INCLUDE_SLIP
	{ "sl", 0, 0, 0, 0, 0 },
#endif	/* INCLUDE_SLIP */
	{ "lo", loattach, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 },
    };

#endif  /* INCLUDE_NETWORK */

/* global variables */

SYMTAB_ID	sysSymTbl;
int		consoleFd;		/* fd of initial console device */
char		consoleName [20];	/* console device name, eg. "/tyCo/0" */
int		sysStartType;		/* BOOT_CLEAR, BOOT_NO_AUTOBOOT, ... */

BOOL		scsiInitialized      = FALSE;
int		bootCmdTaskPriority  = 1;
int		bootCmdTaskOptions   = VX_SUPERVISOR_MODE;
int		bootCmdTaskStackSize = 7000;

#ifdef  INCLUDE_NETWORK

/* mbuf/cluster:: initial, increment, max, partition, partition size */

MBUF_CONFIG mbufConfig  =
    {
    NUM_INIT_MBUFS, NUM_MBUFS_TO_EXPAND, MAX_MBUFS, NULL, 0 
    };

MBUF_CONFIG clusterConfig =
    {
    NUM_INIT_CLUSTERS, NUM_CLUSTERS_TO_EXPAND, MAX_CLUSTERS, NULL, 0 
    };

#endif  /* INCLUDE_NETWORK */

/* forward declarations */

#ifdef __STDC__

void 		usrRoot (char *pMemPoolStart, unsigned memPoolSize);
void 		usrClock (void);
void		usrKernelInit (void);
LOCAL void	bootCmdLoop (void);
LOCAL char	autoboot (int timeout);
LOCAL void	printBootLogo (void);
LOCAL void	bootHelp (void);
LOCAL STATUS	bootLoad (char *bootString, FUNCPTR *pEntry);
LOCAL void	go (FUNCPTR entry);
LOCAL void	m (char *adrs);
LOCAL void	d (char *adrs, int nwords);
LOCAL void	bootExcHandler (int tid);
LOCAL void	skipSpace (char **strptr);
LOCAL void	printExcMsg (char *string);
LOCAL STATUS	getArg (char **ppString, int *pValue, BOOL defaultHex,
			BOOL optional);
LOCAL void	usrBootLineInit (int startType);
LOCAL STATUS	usrBootLineCrack (char *bootString, BOOT_PARAMS *pParams);

#ifdef  INCLUDE_NETWORK
LOCAL STATUS	netLoad (char *hostName, char *fileName, char *usr, 
			 char *passwd, FUNCPTR *pEntry);
LOCAL void	netifAdrsPrint (char *ifname);
LOCAL STATUS	checkInetAddrField (char *pInetAddr, BOOL subnetMaskOK);
LOCAL STATUS	usrNetIfAttach (char *devName, char *inetAdrs);
LOCAL STATUS	usrNetIfConfig (char *devName, char *inetAdrs, char *inetName,
			        int netmask);
LOCAL STATUS	usrBpInit (char *devName, u_long startAddr);
LOCAL STATUS	usrSlipInit (char *pBootDev, char *localAddr, char *peerAddr);
LOCAL STATUS	bootpGet (char *netDev, char *pBootDevAddr, char *pBootFile,
			  char *pHostAddr, int *pMask);
#endif  /* INCLUDE_NETWORK */

#ifdef	INCLUDE_SCSI_BOOT
LOCAL STATUS	scsiLoad (int bootDevId, int bootDevLUN, char *fileName,
		          FUNCPTR *pEntry);
#endif	/* INCLUDE_SCSI_BOOT */

#ifdef  INCLUDE_FD
LOCAL STATUS    fdLoad (int drive, int type, char *fileName, FUNCPTR *pEntry);
#endif  /* INCLUDE_FD */

#ifdef  INCLUDE_IDE
LOCAL STATUS    ideLoad (int drive, int type, char *fileName, FUNCPTR *pEntry);
#endif  /* INCLUDE_IDE */

#else

void		usrRoot ();
void		usrClock ();
void		usrKernelInit ();
LOCAL void	bootCmdLoop ();
LOCAL char	autoboot ();
LOCAL void	printBootLogo ();
LOCAL void	bootHelp ();
LOCAL STATUS	bootLoad ();
LOCAL void	go ();
LOCAL void	m ();
LOCAL void	d ();
LOCAL void	bootExcHandler ();
LOCAL void	skipSpace ();
LOCAL void	printExcMsg ();
LOCAL STATUS	getArg ();
LOCAL void	usrBootLineInit ();
LOCAL STATUS	usrBootLineCrack ();

#ifdef  INCLUDE_NETWORK
LOCAL STATUS	netLoad ();
LOCAL void	netifAdrsPrint ();
LOCAL STATUS	checkInetAddrField ();
LOCAL STATUS	usrNetIfAttach ();
LOCAL STATUS	usrNetIfConfig ();
LOCAL STATUS	usrBpInit ();
LOCAL STATUS	usrSlipInit ();
LOCAL STATUS	bootpGet ();
#endif  /* INCLUDE_NETWORK */

#ifdef	INCLUDE_SCSI_BOOT
LOCAL STATUS	scsiLoad();
#endif	/* INCLUDE_SCSI_BOOT */

#ifdef  INCLUDE_FD
LOCAL STATUS    fdLoad ();
#endif  /* INCLUDE_FD */

#ifdef  INCLUDE_IDE
LOCAL STATUS    ideLoad ();
#endif  /* INCLUDE_IDE */

#endif	/* __STDC__ */


/*******************************************************************************
*
* usrInit - user-defined system initialization routine
*
* This routine is called by the start-up code in romStart().  It is called
* before kernel multi-tasking is enabled, with the interrupts locked out.
*
* It starts by clearing BSS, so all variables are initialized to 0 as per
* the C specification.  Then it sets up exception vectors, initializes the
* hardware by calling sysHwInit(), and finally starts the kernel with the
* usrRoot() task to do the remainder of the initialization.
*
* NOMANUAL
*/

void usrInit 
    (
    int startType
    )
    {
#if (CPU_FAMILY == SPARC)
    excWindowInit ();				/* SPARC window management */
#endif

#if	(CPU_FAMILY == MIPS)
    sysGpInit ();				/* MIPS global pointer */
#endif	/* (CPU_FAMILY == MIPS) */


    /* configure data and instruction cache if available and leave disabled */

    cacheLibInit (USER_I_CACHE_MODE, USER_D_CACHE_MODE);

#if (CPU == SPARClite)
    cacheLib.textUpdateRtn = NULL;		/* XXX - mod hist 07u */
#endif

    /* don't assume bss variables are zero before this call */

    bzero (edata, end - edata);		/* zero out bss variables */

    sysStartType = startType;

    intVecBaseSet ((FUNCPTR *) VEC_BASE_ADRS);	/* set vector base table */

#if (CPU_FAMILY == AM29XXX)
    excSpillFillInit ();                        /* am29k stack cache managemt */
#endif

    excVecInit ();				/* install exception vectors */

    sysHwInit ();				/* initialize system hardware */

    usrKernelInit ();				/* configure the Wind kernel */

#if	(CPU==SPARC) || (CPU_FAMILY==I80X86)	/* XXX workaround for sun1e */
#undef USER_I_CACHE_ENABLE	/* XXX disable instruction cache */
#endif	/* (CPU==SPARC) || (CPU_FAMILY==I80X86)	*/

#ifdef 	USER_I_CACHE_ENABLE
    cacheEnable (INSTRUCTION_CACHE);		/* enable instruction cache */
#endif	/* USER_I_CACHE_ENABLE */

    /* start the kernel specifying usrRoot as the root task */

    kernelInit ((FUNCPTR) usrRoot, ROOT_STACK_SIZE, (char *) FREE_RAM_ADRS,
		sysMemTop (), ISR_STACK_SIZE, INT_LOCK_LEVEL);
    }

/*******************************************************************************
*
* usrRoot - user-defined root task
*
* The root task performs any initialization that should be done
* subsequent to the kernel initialization.
*
* It initializes the I/O system, install drivers, create devices,
* sets up the network, etc., as necessary for the particular configuration.
* It may also create the system symbol table if one is to be included.
* Finally, it spawns the boot command loop task.
*
* NOMANUAL
*/

void usrRoot
    (
    char *      pMemPoolStart,          /* start of system memory partition */
    unsigned    memPoolSize             /* initial size of mem pool */
    )

    {
    char tyName [20];
    int ix;

    /* Initialize the memory pool before initializing any other package.
     * The memory associated with the root task will be reclaimed at the
     * completion of its activities.                                 
     */

    memInit (pMemPoolStart, memPoolSize);/* XXX select between memPartLibInit */

    /* set up system timer */

    sysClkConnect ((FUNCPTR) usrClock, 0);/* connect clock interrupt routine */
    sysClkRateSet (60);			/* set system clock rate */
    sysClkEnable ();			/* start it */

    /* initialize I/O and file system */

    iosInit (NUM_DRIVERS, NUM_FILES, "/null");
    consoleFd = NONE;

    /* install driver for on-board serial ports and make devices */

#ifdef  INCLUDE_TYCODRV_5_2
#ifdef  INCLUDE_TTY_DEV
    if (NUM_TTY > 0)
        {
        tyCoDrv ();                             /* install console driver */

        for (ix = 0; ix < NUM_TTY; ix++)        /* create serial devices */
            {
            sprintf (tyName, "%s%d", "/tyCo/", ix);

            (void) tyCoDevCreate (tyName, ix, 512, 512);

            if (ix == CONSOLE_TTY)
                strcpy (consoleName, tyName);   /* store console name */
            }

        consoleFd = open (consoleName, O_RDWR, 0);

        /* set baud rate */

        (void) ioctl (consoleFd, FIOBAUDRATE, CONSOLE_BAUD_RATE);
        (void) ioctl (consoleFd, FIOSETOPTIONS,
			OPT_ECHO | OPT_CRMOD | OPT_TANDEM | OPT_7_BIT);
        }
#endif  /* INCLUDE_TTY_DEV */

#else   /* !INCLUDE_TYCODRV_5_2 */
#ifdef  INCLUDE_TTY_DEV
    if (NUM_TTY > 0)
        {
        ttyDrv();                               /* install console driver */

        for (ix = 0; ix < NUM_TTY; ix++)        /* create serial devices */
            {
            sprintf (tyName, "%s%d", "/tyCo/", ix);
            (void) ttyDevCreate (tyName, sysSerialChanGet(ix), 512, 512);

            if (ix == CONSOLE_TTY)              /* init the tty console */
                {
                strcpy (consoleName, tyName);
                consoleFd = open (consoleName, O_RDWR, 0);
                (void) ioctl (consoleFd, FIOBAUDRATE, CONSOLE_BAUD_RATE);
                (void) ioctl (consoleFd, FIOSETOPTIONS,
			OPT_ECHO | OPT_CRMOD | OPT_TANDEM | OPT_7_BIT);
                }
            }
        }
#endif  /* INCLUDE_TTY_DEV */


#ifdef INCLUDE_PC_CONSOLE
    pcConDrv ();
    for (ix = 0; ix < N_VIRTUAL_CONSOLES; ix++)
        {
        sprintf (tyName, "%s%d", "/pcConsole/", ix);
        (void) pcConDevCreate (tyName,ix, 512, 512);
        if (ix == PC_CONSOLE)           /* init the console device */
            {
            strcpy (consoleName, tyName);
            consoleFd = open (consoleName, O_RDWR, 0);
            (void) ioctl (consoleFd, FIOBAUDRATE, CONSOLE_BAUD_RATE);
            (void) ioctl (consoleFd, FIOSETOPTIONS,
			OPT_ECHO | OPT_CRMOD | OPT_TANDEM | OPT_7_BIT);
            }
        }
#endif  /* INCLUDE_PC_CONSOLE */

#endif  /* !INCLUDE_TYCODRV_5_2 */

    ioGlobalStdSet (STD_IN,  consoleFd);
    ioGlobalStdSet (STD_OUT, consoleFd);
    ioGlobalStdSet (STD_ERR, consoleFd);

    pipeDrv ();					/* install pipe driver */
    excInit ();					/* init exception handling */
    excHookAdd ((FUNCPTR) bootExcHandler);	/* install exc handler */
    logInit (consoleFd, 5);			/* initialize logging */

#ifdef	INCLUDE_DOSFS
    hashLibInit ();				/* hashLib used by dosFS */
#endif

    /* initialize object module loader */

#if	defined(INCLUDE_AOUT)
    bootAoutInit ();				/* use a.out format */
#else	/* coff or ecoff */
#if	defined(INCLUDE_ECOFF)
    bootEcoffInit ();				/* use ecoff format */
#else	/* coff */
#if	defined(INCLUDE_COFF)
    bootCoffInit ();				/* use coff format */
#else   /* coff */
#if	defined(INCLUDE_ELF)
    bootElfInit ();				/* use elf format */
#endif
#endif 						/* mips cpp no elif */
#endif
#endif

    taskSpawn ("tBoot", bootCmdTaskPriority, bootCmdTaskOptions,
		bootCmdTaskStackSize, (FUNCPTR) bootCmdLoop,
		0,0,0,0,0,0,0,0,0,0);
    }
/*******************************************************************************
*
* usrClock - user defined system clock interrupt routine
*
* This routine is called at interrupt level on each clock interrupt.  It is
* installed a call to sysClkConnect().  It calls any other facilities that
* need to know about clock ticks, including the kernel itself.
*
* If the application needs anything to happen at clock interrupt level,
* it should be added to this routine.
*
* NOMANUAL
*/

void usrClock (void)

    {
    tickAnnounce ();	/* announce system tick to kernel */
    }
/*******************************************************************************
*
* bootCmdLoop - read and execute user commands forever (until boot)
*/

LOCAL void bootCmdLoop (void)

    {
    BOOT_PARAMS params;
    char line [MAX_LINE];
    char *pLine;
    int nwords;
    int nbytes;
    int value;
    int adr;
    int adr2;
    FUNCPTR entry;
    char key = 0;

    /* flush standard input to get rid of any garbage;
     * E.g. the Heurikon HKV2F gets junk in USART if no terminal connected.
     */

    (void) ioctl (STD_IN, FIOFLUSH, 0 /*XXX*/);

    if (sysStartType & BOOT_CLEAR)
	printBootLogo ();

    usrBootLineInit (sysStartType);


    /* print out any new exception message -
     * the first byte is zeroed after printing so that we won't print
     * it again automatically.  However, 'e' command will still print out
     * the remainder. */

    printExcMsg (sysExcMsg);
    *sysExcMsg = EOS;		/* indicate exception message is old */


    /* start autoboot, unless no-autoboot specified */

    bootStringToStruct (BOOT_LINE_ADRS, &params);
    sysFlags = params.flags;

    if (!(sysStartType & BOOT_NO_AUTOBOOT) &&
	!(sysFlags & SYSFLG_NO_AUTOBOOT))
	{
	int timeout = TIMEOUT;

	if ((sysStartType & BOOT_QUICK_AUTOBOOT) ||
	    (sysFlags & SYSFLG_QUICK_AUTOBOOT))
	    {
	    timeout = 1;
	    }

	key = autoboot (timeout);	/* doesn't return if successful */
	}


    /* If we're here, either we aren't auto-booting, or we got an error
     * auto-booting, or the auto-booting was stopped. */

    /* put console in line mode */

    (void) ioctl (consoleFd, FIOSETOPTIONS, OPT_TERMINAL);

    /* read and execute the ROM commands */

    printf ("\n");

    FOREVER
	{
	if ((key == '!') || (key == '@'))
	    {
	    line [0] = key;
	    line [1] = EOS;
	    key = 0;
	    }
	else
	    {
	    printf ("[VxWorks Boot]: ");
	    fioRdString (STD_IN, line, sizeof (line));
	    }

	adr = adr2 = 0;
	nwords = 0;

	/* take blanks off end of line */

	pLine = line + strlen (line) - 1;		/* point at last char */
	while ((pLine >= line) && (*pLine == ' '))
	    {
	    *pLine = EOS;
	    pLine--;
	    }

	pLine = line;
	skipSpace (&pLine);

	switch (*(pLine++))
	    {
	    case EOS:		/* blank line */
		break;

	    case 'd':		/* display */
		if ((getArg (&pLine, &adr, HEX, OPT) == OK) &&
		    (getArg (&pLine, &nwords, DEC, OPT) == OK))
		    d ((char *) adr, nwords);
		break;

	    case 'e':		/* exception */
		printExcMsg (sysExcMsg + 1);
		break;

	    case 'f':		/* fill */
		if ((getArg (&pLine, &adr, HEX, !OPT) == OK) &&
		    (getArg (&pLine, &nbytes, DEC, !OPT) == OK) &&
		    (getArg (&pLine, &value, DEC, !OPT) == OK))
		    {
		    bfillBytes ((char *) adr, nbytes, value);
		    }

		break;

	    case 't':		/* transpose(?) (running out of letters!) */
		if ((getArg (&pLine, &adr, HEX, !OPT) == OK) &&
		    (getArg (&pLine, &adr2, HEX, !OPT) == OK) &&
		    (getArg (&pLine, &nbytes, HEX, !OPT) == OK))
		    {
		    bcopy ((char *) adr, (char *) adr2, nbytes);
		    }
		break;

	    case 'm':		/* modify */
		if (getArg (&pLine, &adr, HEX, !OPT) == OK)
		    m ((char *) adr);
		break;

#ifdef	TARGET_HK_V2F
	    case 's':		/* system controller */
		{
		extern ULONG sysBCLSet ();

		if (getArg (&pLine, &value, DEC, !OPT) == OK)
		    {
		    if (value != 0)
			{
			(void) sysBCLSet ((ULONG)HK_BCL_SYS_CONTROLLER,
					  (ULONG)HK_BCL_SYS_CONTROLLER);
			printf ("System controller on.\n");
			}
		    else
			{
			(void) sysBCLSet ((ULONG)HK_BCL_SYS_CONTROLLER,
					  (ULONG)0);
			printf ("System controller off.\n");
			}
		    }
		break;
		}
#endif	/* TARGET_HK_V2F */

#if defined(TARGET_FRC_30) || defined(TARGET_FRC_31) || defined(TARGET_FRC_33)
	    case 's':		/* system controller */
		if (getArg (&pLine, &value, DEC, !OPT) == OK)
		    {
		    if (value != 0)
			{
			*FGA_CTL1 |= FGA_CTL1_SCON;
			printf ("System controller on.\n");
			}
		    else
			{
			*FGA_CTL1 &= ~FGA_CTL1_SCON;
			printf ("System controller off.\n");
			}
		    }
		break;
#endif	/* TARGET_FRC_30 || TARGET_FRC_31 || TARGET_FRC_33 */

	    case 'p':		/* print boot params */
		bootParamsShow (BOOT_LINE_ADRS);
		break;

	    case 'c':		/* change boot params */
		bootParamsPrompt (BOOT_LINE_ADRS);
		(void) sysNvRamSet (BOOT_LINE_ADRS,
				    strlen (BOOT_LINE_ADRS) + 1, 0);
		break;

	    case 'g':		/* go */
		if (getArg (&pLine, (int *) &entry, HEX, !OPT) == OK)
		    go (entry);
		break;

#ifdef  INCLUDE_NETWORK
	    case 'n':
		netifAdrsPrint (pLine);
		break;

#ifdef ETHERNET_ADR_SET
	    case 'N':
		mEnet ();
		break;
#endif  /* ETHERNET_ADR_SET */
#endif  /* INCLUDE_NETWORK */

	    case '?':			/* help */
            case 'h':			/* help */
		bootHelp ();
		break;

            case '@':			/* load and go with internal params */
	    case '$':			/* load and go with internal params */

		if (bootLoad (pLine, &entry) == OK)
		    {
		    go (entry);
		    }
		else
		    {
		    taskDelay (sysClkRateGet ());	/* pause a second */
		    reboot (BOOT_NO_AUTOBOOT);		/* something is awry */
		    }
		break;

	    case 'l':			/* load with internal params */

		if (bootLoad (pLine, &entry) == OK)
		    {
		    printf ("entry = 0x%x\n", (int) entry);
		    }
		else
		    {
		    taskDelay (sysClkRateGet ());	/* pause a second */
		    reboot (BOOT_NO_AUTOBOOT);		/* something is awry */
		    }
		break;


	    default:
		printf ("Unrecognized command. Type '?' for help.\n");
		break;

            } /* switch */
        } /* FOREVER */
    }
/******************************************************************************
*
* autoboot - do automatic boot sequence
*
* RETURNS: Doesn't return if successful (starts execution of booted system).
*/

LOCAL char autoboot 
    (
    int timeout		/* timeout time in seconds */
    )
    {
    int autoBootTime;
    int timeLeft;
    UINT timeMarker;
    int bytesRead = 0;
    FUNCPTR entry;
    char key;

    if (timeout > 0)
	{
	printf ("\nPress any key to stop auto-boot...\n");

	/* Loop looking for a char, or timeout after specified seconds */

	autoBootTime = tickGet () + sysClkRateGet () * timeout;
	timeMarker = tickGet () + sysClkRateGet ();
	timeLeft = timeout;

	printf ("%2d\r", timeLeft);

	 while ((tickGet () < autoBootTime) && (bytesRead == 0))
	    {
	    (void) ioctl (consoleFd, FIONREAD, (int) &bytesRead);

	    if (tickGet () == timeMarker)
		{
		timeMarker = tickGet () + sysClkRateGet ();
		printf ("%2d\r", --timeLeft);
		}
	    }
	}

    if (bytesRead == 0)    /* nothing typed so auto-boot */
	{
	/* put the console back in line mode so it echoes (so's you can bang
	 * on it to see if it's still alive) */

	(void) ioctl (consoleFd, FIOSETOPTIONS, OPT_TERMINAL);

	printf ("\nauto-booting...\n\n");

	if (bootLoad (BOOT_LINE_ADRS, &entry) == OK)
	    go (entry);				/* ... and never return */
	else
	    {
	    printf ("Can't load boot file!!\n");
	    taskDelay (sysClkRateGet ());	/* pause a second */
	    reboot (BOOT_NO_AUTOBOOT);		/* something is awry */
	    }
	}
    else
	{
	/* read the key that stopped autoboot */

	read (consoleFd, &key, 1);
	return (key & 0x7f);		/* mask off parity in raw mode */
	}

    return (ERROR);			/* for lint - can't really get here */
    }
/******************************************************************************
*
* printBootLogo - print initial boot banner page
*/

LOCAL void printBootLogo (void)

    {
    printf ("\n\n\n\n\n\n\n\n\n\n\n");
    printf ("%28s%s", "","VxWorks System Boot");
    printf ("\n\n\nCopyright 1984-1995  Wind River Systems, Inc.\n\n\n\n\n\n");
    printf ("CPU: %s\n", sysModel ());
    printf ("Version: %s\n", vxWorksVersion);
    printf ("BSP version: " BSP_VERSION BSP_REV "\n");
    printf ("Creation date: " __DATE__ "\n\n");
    }

/*******************************************************************************
*
* bootHelp - print brief help list
*/

LOCAL void bootHelp (void)

    {
    static char *helpMsg[] =
	{
	"?",                      "- print this list",
	"@",                      "- boot (load and go)",
	"p",                      "- print boot params",
	"c",                      "- change boot params",
	"l",                      "- load boot file",
	"g adrs",                 "- go to adrs",
	"d adrs[,n]",             "- display memory",
	"m adrs",                 "- modify memory",
	"f adrs, nbytes, value",  "- fill memory",
	"t adrs, adrs, nbytes",   "- copy memory",
	"e",                      "- print fatal exception",
#ifdef INCLUDE_NETWORK
	"n netif",	  	  "- print network interface device address",
#if defined(ETHERNET_ADR_SET)
	"N",			  "- set ethernet address",
#endif  /* ETHERNET_ADR_SET */
#endif  /* INCLUDE_NETWORK */
#if defined(TARGET_HK_V2F) || defined(TARGET_FRC_30) || \
    defined(TARGET_FRC_31) || defined(TARGET_FRC_33)
	"s [0/1]",                "- system controller 0 = off, 1 = on",
#endif	/* TARGET_HK_V2F/FRC_30/FRC_31/FRC_33 */
	"$dev(0,procnum)host:/file h=# e=# b=# g=# u=usr [pw=passwd] f=#", "",
	"                          tn=targetname s=script o=other", "",

#ifdef INCLUDE_FD
	"boot device and file would be: fd=drive,fdType      /fd0/vxWorks","",
#endif  /* INCLUDE_FD */

#ifdef INCLUDE_IDE
	"boot device and file would be: ide=drive,configType /ide0/vxWorks","",
#endif  /* INCLUDE_IDE */

	"Boot flags:",		  "",
#if defined(TARGET_HK_V2F) || defined(TARGET_FRC_30) || \
    defined(TARGET_FRC_31) || defined(TARGET_FRC_33)
	"  0x01  - don't be system controller",	"",
#endif	/* TARGET_HK_V2F/FRC_30/FRC_31/FRC_33 */
	"  0x02  - load local system symbols",		"",
	"  0x04  - don't autoboot",			"",
	"  0x08  - quick autoboot (no countdown)",	"",
#ifdef  INCLUDE_NETWORK
	"  0x20  - disable login security",		"",
	"  0x40  - use bootp to get boot parameters",	"",
	"  0x80  - use tftp to get boot image",		"",
	"  0x100 - use proxy arp",			"",
#endif  /* INCLUDE_NETWORK */
	NULL
	};

    FAST char **pMsg;
#ifdef  INCLUDE_NETWORK
    FAST NETIF *pNif;
#endif  /* INCLUDE_NETWORK */

    printf ("\n");

    for (pMsg = helpMsg; *pMsg != NULL; pMsg += 2)
	printf (" %-21s %s\n", *pMsg, *(pMsg + 1));

#ifdef  INCLUDE_NETWORK

    printf ("\navailable boot devices:");

    for (pNif = netIf; pNif->ifName != 0; pNif++)
	{
	if (strncmp (pNif->ifName, "lo", 2) != 0)
	    printf (" %s", pNif->ifName);
	}

#endif  /* INCLUDE_NETWORK */

#ifdef  INCLUDE_FD
    printf (" fd");
#endif  /* INCLUDE_FD */

#ifdef  INCLUDE_IDE
    printf (" ide");
#endif  /* INCLUDE_IDE */

    printf ("\n");
    }

/*******************************************************************************
*
* bootLoad - load a module into memory
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS bootLoad 
    (
    char *   bootString,
    FUNCPTR *pEntry
    )
    {
    BOOT_PARAMS		params;
#ifdef  INCLUDE_NETWORK
    char		nad [20];	/* host's network internet addr */
    int			netmask = 0;	/* temporary storage */
    char		mask [30];	/* string mask */
    char 		netDev [BOOT_DEV_LEN + 1];
    char 		bootDev [BOOT_DEV_LEN];
    BOOL		backplaneBoot;
    char *		pBootAddr;
#endif  /* INCLUDE_NETWORK */

    /* copy bootString to low mem address, if specified */

    if ((bootString != NULL) && (*bootString != EOS))
	strcpy (BOOT_LINE_ADRS, bootString);

    /* interpret boot command */

    if (usrBootLineCrack (BOOT_LINE_ADRS, &params) != OK)
	return (ERROR);

    /* Display boot parameters */

    bootParamsShow (BOOT_LINE_ADRS);

    /* set our processor number: may establish vme access, etc. */

    sysFlags = params.flags;
    sysProcNumSet (params.procNum);

#ifdef	INCLUDE_SCSI_BOOT

    /*
     * initialize either the SCSI1 or SCSI2 interface; initialize SCSI2 when
     * the SCSI2 interface is available.
     */

#ifndef INCLUDE_SCSI2
    scsi1IfInit ();
#else
    scsi2IfInit ();
#endif

    if (strncmp (params.bootDev, "scsi", 4) == 0)
	{
	int bootDevId = NONE;
	int bootDevLUN = NONE;

	/* check for absence of bus ID and LUN, in which case
	 * auto-configure and display results
	 */

	if (strlen (params.bootDev) == 4)
	    {
	    if (!scsiInitialized)
		{
	    	if (sysScsiInit () == ERROR)
		    {
		    printErr ("Could not initialize SCSI.\n");
		    return (ERROR);
		    }
		scsiInitialized = TRUE;
		}

	    scsiAutoConfig (pSysScsiCtrl);
	    scsiShow (pSysScsiCtrl);

	    /* return ERROR to indicate that no file was loaded */
	    return (ERROR);
	    }

	sscanf (params.bootDev, "%*4s%*c%d%*c%d", &bootDevId, &bootDevLUN);

	if (scsiLoad (bootDevId, bootDevLUN, params.bootFile, pEntry) != OK)
	    {
	    printErr ("\nError loading file: errno = 0x%x.\n", errno);
	    return (ERROR);
	    }

	return (OK);
	}

#endif	/* INCLUDE_SCSI_BOOT */

#ifdef  INCLUDE_FD

    if (strncmp (params.bootDev, "fd", 2) == 0)
	{
	int type = 0;
	int drive = 0;

	if (strlen (params.bootDev) == 2)
	    return (ERROR);
	else
	    sscanf (params.bootDev, "%*2s%*c%d%*c%d", &drive, &type);

	if (fdLoad (drive, type, params.bootFile, pEntry) != OK)
	    {
	    printErr ("\nError loading file: errno = 0x%x.\n", errno);
	    return (ERROR);
	    }

	return (OK);
	}

#endif  /* INCLUDE_FD */

#ifdef	INCLUDE_IDE

    if (strncmp (params.bootDev, "ide", 3) == 0)
	{
	int type = 0;
	int drive = 0;

	if (strlen (params.bootDev) == 3)
	    return (ERROR);
	else
	    sscanf (params.bootDev, "%*3s%*c%d%*c%d", &drive, &type);

	if (ideLoad (drive, type, params.bootFile, pEntry) != OK)
	    {
	    printErr ("\nError loading file: errno = 0x%x.\n", errno);
	    return (ERROR);
	    }

	return (OK);
	}

#endif	/* INCLUDE_IDE */

#ifndef  INCLUDE_NETWORK

    printf ("\nError loading file: networking code not present.\n");
    return (ERROR);
    }

#else  /* INCLUDE_NETWORK */


    /* start the network */

    hostTblInit ();		/* initialize host table */
    netLibInit ();

    /* attach and configure boot interface */

    if (strncmp (params.bootDev, "sl", 2) == 0)
	{
  	if (usrSlipInit (params.bootDev, params.ead, ((params.gad[0] == EOS)?
	                 params.had : params.gad)) == ERROR)
	    return (ERROR);
        }
    else
	{
        strncpy (bootDev, params.bootDev, sizeof (bootDev));

	if ((strncmp (params.bootDev, "bp", 2) != 0) &&
            (strncmp (params.bootDev, "sm", 2) != 0))
	    {
	    pBootAddr = params.ead;
	    backplaneBoot = FALSE;
	    }
        else
	    {
	    if (sysProcNumGet () == 0)
		{
                printf (
                  "Error: processor number must be non-zero to boot from bp\n");
	        return (ERROR);
                }

	    if (usrBpInit (bootDev, 0) == ERROR)
		return (ERROR);

	    pBootAddr = params.bad;
	    backplaneBoot = TRUE;
	    }

        netmask = 0;
        bootNetmaskExtract (pBootAddr, &netmask);

	if (usrNetIfAttach (bootDev, pBootAddr) != OK)
	    return (ERROR);

	(void) sprintf (netDev, "%s%d", bootDev, 0);

        if ((sysFlags & SYSFLG_BOOTP) || (sysFlags & SYSFLG_PROXY) ||
	    (netmask == 0))
            {
    	    struct ifnet *	pIf;

	    /* Initialize the boot device */

            if ((pIf = ifunit (netDev)) == NULL) 
                {
                printf ("invalid device \"%s\"\n", netDev);
                return (ERROR);			/* device not attached */
                }

	    if (pIf->if_init != NULL)
		{
            	if ((*pIf->if_init) (pIf->if_unit) != 0)
		    {
                    printf ("initialization failed for device \"%s\"\n",netDev);
                    return (ERROR);
                    }
		}
            }

#ifdef INCLUDE_SM_NET
 	if (backplaneBoot)
	    {
	    if ((params.bad [0] == EOS) &&
	    	(strncmp (bootDev, "sm", 2) == 0) &&
		(smNetInetGet (netDev, params.bad, NONE) == OK))
	    	printf ("Backplane inet address: %s\n", params.bad);

	    if (params.bad [0] == EOS) 
	    	{
	    	printf ("no backplane address specified\n");
		return (ERROR);
		}

	    if ((sysFlags & SYSFLG_BOOTP) && !(sysFlags & SYSFLG_PROXY))
	        {
	        printf ("BOOTP over backplane only with proxy arp\n");
	        return (ERROR);
	        }

	    if (sysFlags & SYSFLG_PROXY)
		{
#ifdef INCLUDE_PROXY_CLIENT
	    	printf ("registering proxy client: %s", params.bad);

	    	if (proxyReg (netDev, params.bad) == ERROR)
		    {
	    	    printf ("client registered failed %x\n", errno);
		    return (ERROR);
		    }
		printf ("done.\n");

#else /* INCLUDE_PROXY_CLIENT */
    	    	printf ("proxy client referenced but not included.\n");
    	    	return (ERROR);
#endif /* INCLUDE_PROXY_CLIENT */
		}
	    }
#endif /* INCLUDE_SM_NET */

        /* get boot parameters via bootp */

        if (sysFlags & SYSFLG_BOOTP)
	    {
	    if (bootpGet (netDev, pBootAddr, params.bootFile, params.had,
		       	  &netmask) == ERROR)
	        return (ERROR);
	    }

        if (netmask == 0)
	    {
	    (void) icmpMaskGet (netDev, pBootAddr, backplaneBoot ?
				NULL : params.had, &netmask);
	    if (netmask != 0)
	        printf ("Subnet Mask: 0x%x\n", netmask);
	    }

        /* configure the device */

        if (usrNetIfConfig (bootDev, pBootAddr, (char *) NULL, netmask) != OK)
            return (ERROR);

 	/* get gateway address */

#ifdef INCLUDE_SM_NET
	if (backplaneBoot && (params.gad [0] == EOS) && 
	    !(sysFlags & SYSFLG_PROXY))
	    {
	        struct in_addr host;		/* Internet Address */
	        struct in_addr backpl;		/* Internet Address */

	        host.s_addr = inet_addr (params.had);
	        backpl.s_addr = inet_addr (params.bad);

	        if ( in_netof(host) != in_netof(backpl) )
	        {
	        /* We can get the gateway address (assumed to be master)  */

	        if ((strncmp (bootDev, "sm", 2) == 0) && 
	            (smNetInetGet (netDev, params.gad, 0) == OK))
		    printf ("Gateway inet address: %s\n", params.gad);
		}
	    }
#endif /* INCLUDE_SM_NET */

	if (netmask != 0)		/* reconstruct address with mask */
	    {
	    sprintf (mask, ":%x", netmask);
	    strcat  (pBootAddr, mask);
	    }

        /* in case boot string changed */

        bootStructToString (BOOT_LINE_ADRS, &params);
        }

    usrNetIfAttach ("lo", "127.0.0.1");
    usrNetIfConfig ("lo", "127.0.0.1", "localhost", 0);

    /* if a gateway was specified, extract the network part of the host's
     * address and add a route to this network
     */

    if (params.gad[0] != EOS)
        {
	inet_netof_string (params.had, nad);
	routeAdd (nad, params.gad);
        }

    /* associate hostName with the specified host address */

    hostAdd (params.hostName, params.had);

    /* load specified file */

    if (netLoad (params.had, params.bootFile, params.usr,
		 params.passwd, pEntry) != OK)
	{
	printf ("\nError loading file: errno = 0x%x.\n", errno);
	return (ERROR);
	}

    return (OK);
    }

/*******************************************************************************
*
* netLoad - downLoad a file from a remote machine via the network.
*
* The remote shell daemon on the machine 'host' is used to download
* the given file to the specified previously opened network file descriptor.
* The remote userId should have been set previously by a call to iam().
* If the file does not exist, the error message from the Unix 'host'
* is printed to the VxWorks standard error fd and ERROR is returned.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS netLoad 
    (
    char *hostName,
    char *fileName,
    char *usr,
    char *passwd,
    FUNCPTR *pEntry
    )
    {
    int fd;
    int errFd;		/* for receiving standard error messages from Unix */
    char command[100];
    BOOL bootFtp = (passwd[0] != EOS);
    BOOL bootRsh = FALSE;

    printf ("Loading... ");

#ifdef INCLUDE_TFTP_CLIENT
    if (sysFlags & SYSFLG_TFTP)		/* use tftp to get image */
        {
	if (tftpXfer (hostName, 0, fileName, "get", "binary", &fd,
		      &errFd) == ERROR)
	    return (ERROR);
	}

   else
#endif
       {
	if (bootFtp)
	    {

	    if (ftpXfer (hostName, usr, passwd, "", "RETR %s", "", fileName,
		         &errFd, &fd) == ERROR)
		return (ERROR);
	    }
	else
	    {
	    bootRsh = TRUE;
	    sprintf (command, "cat %s", fileName);

	    fd = rcmd (hostName, RSHD, usr, usr, command, &errFd);
	    if (fd == ERROR)
		return (ERROR);
	    }
	}

    if (bootLoadModule (fd, pEntry) != OK)
	goto readErr;

    if (bootRsh == FALSE)
	{

	/* Empty the Data Socket before close. PC FTP server hangs otherwise */

	while ((read (fd, command, sizeof (command))) > 0);

	if (bootFtp)
	    (void) ftpCommand (errFd, "QUIT",0,0,0,0,0,0);
	}

    close (fd);
    close (errFd);
    return (OK);

readErr:
    /* check standard error on Unix */

    if (bootRsh == FALSE)
	{

	/* Empty the Data Socket before close. PC FTP server hangs otherwise */

	while ((read (fd, command, sizeof (command))) > 0);

	if (bootFtp)
	    {
	    (void) ftpReplyGet (errFd, FALSE); /* error message on std. err */
	    (void) ftpCommand (errFd, "QUIT",0,0,0,0,0,0);
	    }
	}
    else
	{
	char buf [100];
	int errBytesRecv = fioRead (errFd, buf, sizeof (buf));

	if (errBytesRecv > 0)
	    {
	    /* print error message on standard error fd */

	    buf [errBytesRecv] = EOS;
	    printf ("\n%s:%s: %s\n", hostName, fileName, buf);
	    }
	}

    close (fd);
    close (errFd);

    return (ERROR);
    }

#endif  /* INCLUDE_NETWORK */

#if     (defined (INCLUDE_SCSI_BOOT) || defined (INCLUDE_FD) || \
	 defined (INCLUDE_IDE))

#define	SPIN_UP_TIMEOUT	45	/* max # of seconds to wait for spinup */

/******************************************************************************
*
* devSplit - split the device name from a full path name
*
* This routine returns the device name from a valid UNIX-style path name
* by copying until two slashes ("/") are detected.  The device name is
* copied into <devName>.
*
* RETURNS: N/A
*
* NOMANUAL
*/

void devSplit 
    (
    FAST char *fullFileName,	/* full file name being parsed */
    FAST char *devName		/* result device name */
    )
    {
    FAST int nChars = 0;

    if (fullFileName != NULL)
	{
	char *p0 = fullFileName;
	char *p1 = devName;

	while ((nChars < 2) && (*p0 != EOS))
	    {
	    if (*p0 == '/')
		nChars++;

	    *p1++ = *p0++;
	    }
	*p1 = EOS;
	}
    else
	{
	(void) strcpy (devName, "");
	}
    }

#endif  /* (defined (INCLUDE_SCSI_BOOT) || (INCLUDE_FD) || (INCLUDE_IDE)) */

#ifdef  INCLUDE_SCSI_BOOT

/******************************************************************************
*
* scsiLoad - load a vxWorks image from a local SCSI disk
*
* RETURNS: OK, or ERROR if file can not be loaded.
*/

LOCAL STATUS scsiLoad 
    (
    int     bootDevId,
    int     bootDevLUN,
    char    *fileName,
    FUNCPTR *pEntry
    )
    {
    int fd;

    SCSI_PHYS_DEV *pScsiPhysBootDev;
    BLK_DEV       *pScsiBlkBootDev;
    char bootDir  [BOOT_FILE_LEN];
    int           ix;

    if (!scsiInitialized)		/* skip if this is a retry */
	{
	if (sysScsiInit () == ERROR)
	    {
	    printErr ("Could not initialize SCSI.\n");
	    return (ERROR);
	    }
	scsiInitialized = TRUE;
	}

    taskDelay (sysClkRateGet ());	/* delay 1 second after reset */

    if ((bootDevId  < SCSI_MIN_BUS_ID) ||
	(bootDevId  > SCSI_MAX_BUS_ID) ||
	(bootDevLUN < SCSI_MIN_LUN)    ||
	(bootDevLUN > SCSI_MAX_LUN))
	{
	printErr ("SCSI device parameters < busId = %d, lun = %d > ",
		  bootDevId, bootDevLUN);
	printErr ("are out of range (0-7).\n");
	printErr ("Check boot device format:\n");
	printErr ("    scsi=<busId>,<lun>  e.g.  scsi=2,0\n");
	return (ERROR);
	}

    /* create device handle for TEST UNIT READY commands */

    if ((pScsiPhysBootDev = scsiPhysDevCreate (pSysScsiCtrl, bootDevId,
					       bootDevLUN, 128, 0, 0,
					       0xffff, 512))
	== NULL)
	{
	printErr ("scsiPhysDevCreate failed.\n");
	return (ERROR);
	}

    /* issue a couple fo TEST UNIT READY commands to clear reset execption */

    scsiTestUnitRdy (pScsiPhysBootDev);
    scsiTestUnitRdy (pScsiPhysBootDev);

    /* issue a TEST UNIT READY every second for SPIN_UP_TIMEOUT seconds,
     * or until device returns OK status.
     */

    if (scsiTestUnitRdy (pScsiPhysBootDev) != OK)
        {
        printf ("Waiting for disk to spin up...");

        for (ix = 0; ix < SPIN_UP_TIMEOUT; ix++)
            {
            if (scsiTestUnitRdy (pScsiPhysBootDev) == OK)
                {
                printf (" done.\n");
                break;
		}
            else
		{
                if (ix != (SPIN_UP_TIMEOUT - 1))
                    printf (".");
                else
                    {
                    printf (" timed out.\n");
                    return (ERROR);
		    }
                taskDelay (sysClkRateGet ());
		}
	    }
	}

    /* delete temporary device handle */

    scsiPhysDevDelete (pScsiPhysBootDev);

    printf ("Attaching to scsi device... ");

    /* recreate a device handle, with polling for actual device parameters */

    taskDelay (sysClkRateGet ());

    if ((pScsiPhysBootDev = scsiPhysDevCreate (pSysScsiCtrl, bootDevId,
                                               bootDevLUN, 0, -1, 0, 0, 0))
         == NULL)
	{
        printErr ("scsiPhysDevCreate failed.\n");
        return (ERROR);
        }

    /*-------------------------------------------------------------------------
     *
     * Configuration of an OMTI3500
     *
     *-----------------------------------------------------------------------*/

    if ((strncmp (pScsiPhysBootDev->devVendorID, "SMS", 3) == 0) &&
	(strncmp (pScsiPhysBootDev->devProductID, "OMTI3500", 8) == 0))
	{
	char modeData [4];	/* array for floppy MODE SELECT data */

	/* zero modeData array, then set byte 1 to "medium code" (0x1b).
	 * NOTE: MODE SELECT data is highly device-specific.  If your device
	 * requires configuration via MODE SELECT, please consult the device's
	 * Programmer's Reference for the relevant data format.
	 */

	bzero (modeData, sizeof (modeData));
	modeData [1] = 0x1b;

	/* issue a MODE SELECT cmd to correctly configure floppy controller */

	scsiModeSelect (pScsiPhysBootDev, 1, 0, modeData, sizeof (modeData));

	/* delete and re-create the SCSI_PHYS_DEV so that INQUIRY will return
	 * the new device parameters, i.e., correct number of blocks
	 */

	scsiPhysDevDelete (pScsiPhysBootDev);

	/* recreate a device handle, polling for actual device parameters */

	if ((pScsiPhysBootDev = scsiPhysDevCreate (pSysScsiCtrl, bootDevId,
						   bootDevLUN, 0, -1, 0, 0, 0))
	    == NULL)
	    {
	    printErr ("scsiPhysDevCreate failed.\n");
	    return (ERROR);
	    }
	}
    /*-------------------------------------------------------------------------
     *
     * END of OMTI3500 configuration
     *
     *-----------------------------------------------------------------------*/

    /*-------------------------------------------------------------------------
     *
     * START OF CODE WHICH ASSUMES A DOS-FS PARTITION BEGINNING AT BLOCK 0
     *
     *-----------------------------------------------------------------------*/

    /* create a block device spanning entire disk (non-distructive!) */

    if ((pScsiBlkBootDev = scsiBlkDevCreate (pScsiPhysBootDev, 0, 0)) == NULL)
	{
        printErr ("scsiBlkDevCreate failed.\n");
        return (ERROR);
	}

    dosFsInit (NUM_DOSFS_FILES);        /* initialize DOS-FS */

    /* split off boot device from boot file */

    devSplit (fileName, bootDir);

    /* initialize the boot block device as a dosFs device named <bootDir> */

    if (dosFsDevInit (bootDir, pScsiBlkBootDev, NULL) == NULL)
	{
        printErr ("dosFsDevInit failed.\n");
        return (ERROR);
	}

    /*-------------------------------------------------------------------------
     *
     * END OF CODE WHICH ASSUMES A DOS-FS PARTITION BEGINNING AT BLOCK 0
     *
     *------------------------------------------------------------------------*/

    printErr ("done.\n");

    /* load the boot file */

    printErr ("Loading %s...", fileName);

    fd = open (fileName, O_RDONLY, 0);

    if (fd == ERROR)
	{
        printErr ("\nCannot open \"%s\".\n", fileName);
        return (ERROR);
	}

    if (bootLoadModule (fd, pEntry) != OK)
        goto readErr;

    close (fd);
    return (OK);

readErr:

    printErr ("\nerror loading file: status = 0x%x.\n", errnoGet ());
    close (fd);
    return (ERROR);
    }

#endif	/* INCLUDE_SCSI_BOOT */

#ifdef	INCLUDE_FD

/******************************************************************************
*
* fdLoad - load a vxWorks image from a local floppy disk
*
* RETURNS: OK, or ERROR if file can not be loaded.
*/

LOCAL STATUS fdLoad 
    (
    int     drive,
    int     type,
    char    *fileName,
    FUNCPTR *pEntry
    )
    {
    int fd;
    BLK_DEV *pBootDev;
    char bootDir [BOOT_FILE_LEN];

    if (fdDrv (FD_INT_VEC, FD_INT_LVL) == ERROR)
	{
	printErr ("Could not initialize.\n");
	return (ERROR);
	}

    if ((UINT)drive >= FD_MAX_DRIVES)
	{
	printErr ("drive is out of range (0-%d).\n", FD_MAX_DRIVES - 1);
	return (ERROR);
	}

    printf ("Attaching to floppy disk device... ");

    /* create a block device spanning entire disk (non-distructive!) */

    if ((pBootDev = fdDevCreate (drive, type, 0, 0)) == NULL)
	{
        printErr ("fdDevCreate failed.\n");
        return (ERROR);
	}

    dosFsInit (NUM_DOSFS_FILES);        /* initialize DOS-FS */

    /* split off boot device from boot file */

    devSplit (fileName, bootDir);

    /* initialize the boot block device as a dosFs device named <bootDir> */

    if (dosFsDevInit (bootDir, pBootDev, NULL) == NULL)
	{
        printErr ("dosFsDevInit failed.\n");
        return (ERROR);
	}

    printErr ("done.\n");

    /* load the boot file */

    printErr ("Loading %s...", fileName);

    fd = open (fileName, O_RDONLY, 0);

    if (fd == ERROR)
	{
        printErr ("\nCannot open \"%s\".\n", fileName);
        return (ERROR);
	}

    if (bootLoadModule (fd, pEntry) != OK)
        goto fdLoadErr;

    close (fd);
    return (OK);

fdLoadErr:

    printErr ("\nerror loading file: status = 0x%x.\n", errnoGet ());
    close (fd);
    return (ERROR);
    }

#endif	/* INCLUDE_FD */

#ifdef	INCLUDE_IDE

/******************************************************************************
*
* ideLoad - load a vxWorks image from a local IDE disk
*
* RETURNS: OK, or ERROR if file can not be loaded.
*/

LOCAL STATUS ideLoad 
    (
    int     drive,
    int     type,
    char    *fileName,
    FUNCPTR *pEntry
    )
    {
    int fd;
    char bootDir [BOOT_FILE_LEN];
    BLK_DEV *pBlkDev;
    char buffer[1024];
    int offset = 0;
    IDE_RAW ideRaw;
    IDE_RAW *pIdeRaw = &ideRaw;


    if (ideDrv (IDE_INT_VEC, IDE_INT_LVL, type) == ERROR)
	{
	printErr ("Could not initialize.\n");
	return (ERROR);
	}

    if ((UINT)drive >= IDE_MAX_DRIVES)
	{
	printErr ("drive is out of range (0-%d).\n", IDE_MAX_DRIVES - 1);
	return (ERROR);
	}

    printf ("Attaching to IDE disk device... ");

    dosFsInit (NUM_DOSFS_FILES);        /* initialize DOS-FS */

    /* split off boot device from boot file */

    devSplit (fileName, bootDir);

    /* read the boot sector */

    pIdeRaw->cylinder   = 0;
    pIdeRaw->head       = 0;
    pIdeRaw->sector     = 1;
    pIdeRaw->pBuf       = buffer;
    pIdeRaw->nSecs      = 1;
    pIdeRaw->direction  = 0;

    ideRawio (drive, pIdeRaw);

    /* A VxWorks initialized hard disk is illegal, i.e., unreadable from DOS */

    if ((strncmp((char *)(buffer+DOS_ID_OFFSET), VXDOS, strlen(VXDOS)) != 0) ||
        (strncmp((char *)(buffer+DOS_ID_OFFSET), VXEXT, strlen(VXEXT)) != 0))
        {
        offset =  buffer[FIRST_PARTITION_SECTOR_OFFSET + 3] << 12;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET + 2] << 8;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET + 1] << 4;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET];
	}
  
    if ((pBlkDev = ideDevCreate(drive, 0, offset)) == (BLK_DEV *)NULL)
        {
        printErr ("Error during ideDevCreate: %x\n", errno);
        return (ERROR);
        }

    /* Make DOS file system */

#if	(CPU_FAMILY==I80X86)
    {
    IMPORT char *memTopPhys;

    if ((int)memTopPhys >= 0x300000)
        memAddToPool ((char *)memTopPhys-0x100000, 0x100000);
    }
#endif	/* (CPU_FAMILY==I80X86) */

    if (dosFsDevInit(bootDir, pBlkDev, NULL) == (DOS_VOL_DESC *)NULL)
        {
	printErr ("Error during dosFsDevInit: %x\n", errno);
        return (ERROR);
        }

    printErr ("done.\n");

    /* load the boot file */

    printErr ("Loading %s...", fileName);

    fd = open (fileName, O_RDONLY, 0);

    if (fd == ERROR)
	{
        printErr ("\nCannot open \"%s\".\n", fileName);
        return (ERROR);
	}

    if (bootLoadModule (fd, pEntry) != OK)
        goto ideLoadErr;

    close (fd);
    return (OK);

ideLoadErr:

    printErr ("\nerror loading file: status = 0x%x.\n", errno);
    close (fd);
    return (ERROR);
    }

#endif	/* INCLUDE_IDE */
#ifdef  INCLUDE_NETWORK

/******************************************************************************
*
* netifAdrsPrint - print MAC address of a network interface
*/

LOCAL void netifAdrsPrint 
    (
    char *ifname		/* interface name */
    )
    {
    IMPORT struct ifnet *ifunit ();
    struct ifnet *ifp;
    char  devName [10];

    if (ifname == NULL || *ifname == EOS)
	{
	printf ("Interface not specified\n");
	return;
	}

    while (*ifname == ' ')
	ifname++;       /* skip leading blanks */

    if (*ifname == EOS)
	{
	printf ("Interface not specified\n");
	return;
	}

    sprintf (devName, "%s0", ifname);

    if (strncmp (devName, "bp", 2) == 0)
	{
	/* address for backplane is just processor number */

	printf ("Address for device \"%s\" == 00:00:00:00:00:%02x\n",
		devName,  sysProcNumGet ());
	return;
	}

    /* start the network */

    hostTblInit ();		/* initialize host table */
    netLibInit ();

    if ((ifp = ifunit (devName)) == NULL)
	{
	if ((usrNetIfAttach (ifname, "0") != OK) ||
	    (usrNetIfConfig (ifname, "0", (char *) NULL, 0) != OK))
	    {
	    printf ("Cannot initialize interface named \"%s\"\n", devName);
	    return;
	    }

	if ((ifp = ifunit (devName)) == NULL)
	    {
	    printf ("Device named \"%s\" doesn't exist.\n", devName);
	    return;
	    }
	}

    if (!(ifp->if_flags & IFF_POINTOPOINT) &&
	!(ifp->if_flags & IFF_LOOPBACK))
	{
        printf ("Address for device \"%s\" == %02x:%02x:%02x:%02x:%02x:%02x\n",
		devName,
		((struct arpcom *)ifp)->ac_enaddr [0],
		((struct arpcom *)ifp)->ac_enaddr [1],
		((struct arpcom *)ifp)->ac_enaddr [2],
		((struct arpcom *)ifp)->ac_enaddr [3],
		((struct arpcom *)ifp)->ac_enaddr [4],
		((struct arpcom *)ifp)->ac_enaddr [5]);
	}

    if_dettach (ifunit (devName));	/* dettach interface for fresh start */
    }

#endif  /* INCLUDE_NETWORK */

/*******************************************************************************
*
* go - start at specified address
*/

LOCAL void go 
    (
    FUNCPTR entry
    )
    {
    printf ("Starting at 0x%x...\n\n", (int) entry);

    taskDelay (sysClkRateGet ());	/* give the network a moment to close */

#ifdef  INCLUDE_NETWORK
    ifreset ();				/* reset network to avoid interrupts */
#endif  /* INCLUDE_NETWORK */

    cacheClear (DATA_CACHE, NULL, ENTIRE_CACHE);	/* push cache to mem */

    (entry) ();		/* go to entry point - never to return */
    }

/*******************************************************************************
*
* m - modify memory
*
* This routine prompts the user for modifications to memory, starting at the
* specified address.  It prints each address, and the current contents of
* that address, in turn.  The user can respond in one of several ways:
*
*	RETURN   - No change to that address, but continue
*		   prompting at next address.
*	<number> - Set the contents to <number>.
*	. (dot)	 - No change to that address, and quit.
*	<EOF>	 - No change to that address, and quit.
*
* All numbers entered and displayed are in hexadecimal.
* Memory is treated as 16-bit words.
*/

LOCAL void m 
    (
    char *adrs		/* address to change */
    )
    {
    char line[MAX_LINE + 1];	/* leave room for EOS */
    char *pLine;		/* ptr to current position in line */
    int value;			/* value found in line */
    char excess;

    /* round down to word boundary */

    for (adrs = (char *) ((int) adrs & 0xfffffffe);	/* start on even addr */
         ;						/* FOREVER */
	 adrs = (char *) (((short *) adrs) + 1))	/* bump as short ptr */
	{
	/* prompt for substitution */

	printf ("%06x:  %04x-", (int) adrs, (*(short *)adrs) & 0x0000ffff);

	/* get substitution value:
	 *   skip empty lines (CR only);
	 *   quit on end of file or invalid input;
	 *   otherwise put specified value at address */

	if (fioRdString (STD_IN, line, MAX_LINE) == EOF)
	    break;

	line[MAX_LINE] = EOS;	/* make sure input line has EOS */

	for (pLine = line; isspace (*pLine); ++pLine)	/* skip leading spaces*/
	    ;

	if (*pLine == EOS)			/* skip field if just CR */
	    continue;

	if (sscanf (pLine, "%x%1s", &value, &excess) != 1)
	    break;				/* quit if not number */

	* (short *) adrs = value;		/* assign new value */
	}

    printf ("\n");
    }
/*******************************************************************************
*
* d - display memory
*
* Display contents of memory, starting at adrs.  Memory is displayed in
* words.  The number of words displayed defaults to 64.  If
* nwords is non-zero, that number of words is printed, rounded up to
* the nearest number of full lines.  That number then becomes the default.
*/

LOCAL void d 
    (
    FAST char *adrs,	/* address to display */
    int	       nwords	/* number of words to print. */
    )			/* If 0, print 64 or last specified. */
    {
    static char *last_adrs;
    static int dNbytes = 128;
    char ascii [17];
    FAST int nbytes;
    FAST int byte;

    ascii [16] = EOS;			/* put an EOS on the string */

    nbytes = 2 * nwords;

    if (nbytes == 0)
	nbytes = dNbytes;	/* no count specified: use current byte count */
    else
	dNbytes = nbytes;	/* change current byte count */

    if (adrs == 0)
	adrs = last_adrs;	/* no address specified: use last address */

    adrs = (char *) ((int) adrs & ~1);	/* round adrs down to word boundary */


    /* print leading spaces on first line */

    bfill ((char *) ascii, 16, '.');

    printf ("%06x:  ", (int) adrs & ~0xf);

    for (byte = 0; byte < ((int) adrs & 0xf); byte++)
	{
	printf ("  ");
	if (byte & 1)
	    printf (" ");	/* space between words */
	if (byte == 7)
	    printf (" ");	/* extra space between words 3 and 4 */

	ascii[byte] = ' ';
	}


    /* print out all the words */

    while (nbytes-- > 0)
	{
	if (byte == 16)
	    {
	    /* end of line:
	     *   print out ascii format values and address of next line */

	    printf ("  *%16s*\n%06x:  ", ascii, (int) adrs);

	    bfill ((char *) ascii, 16, '.');	/* clear out ascii buffer */
	    byte = 0;				/* reset word count */
	    }

	printf ("%02x", *adrs & 0x000000ff);
	if (byte & 1)
	    printf (" ");	/* space between words */
	if (byte == 7)
	    printf (" ");	/* extra space between words 3 and 4 */

	if (*adrs == ' ' || (isascii (*adrs) && isprint (*adrs)))
	    ascii[byte] = *adrs;

	adrs++;
	byte++;
	}


    /* print remainder of last line */

    for (; byte < 16; byte++)
	{
	printf ("  ");
	if (byte & 1)
	    printf (" ");	/* space between words */
	if (byte == 7)
	    printf (" ");	/* extra space between words 3 and 4 */

	ascii[byte] = ' ';
	}

    printf ("  *%16s*\n", ascii);	/* print out ascii format values */

    last_adrs = adrs;
    }
/*******************************************************************************
*
* bootExcHandler - bootrom exception handling routine
*/

LOCAL void bootExcHandler 
    (
    int tid		/* task ID */
    )
    {
    REG_SET regSet;       /* task's registers */

    /* get registers of task to be traced */

    if (taskRegsGet (tid, &regSet) != ERROR)
        {
        trcStack (&regSet, (FUNCPTR) NULL, tid);
        taskRegsShow (tid);
        }
    else
        printf ("bootExcHandler: exception caught but no valid task.\n");

    taskDelay (sysClkRateGet ());       /* pause a second */

    reboot (BOOT_NO_AUTOBOOT);
    }
/*******************************************************************************
*
* skipSpace - advance pointer past white space
*
* Increments the string pointer passed as a parameter to the next
* non-white-space character in the string.
*/

LOCAL void skipSpace 
    (
    FAST char **strptr	/* pointer to pointer to string */
    )
    {
    while (isspace (**strptr))
	++*strptr;
    }
/*******************************************************************************
*
* printExcMsg - print exception message
*
* Avoid printing possible control characters in exception message area.
*/

LOCAL void printExcMsg 
    (
    char *string
    )
    {
    printf ("\n");
    while (isascii(*string) && (isprint (*string) || isspace ( *string)))
	printf ("%c", *string++);
    printf ("\n");
    }

/******************************************************************************
*
* getArg - get argument from command line
*
* This routine gets the next numerical argument from the command line.
* If the argument is not optional, then an error is reported if no argument
* is found.  <ppString> will be updated to point to the new position in the
* command line.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS getArg 
    (
    FAST char **ppString,	/* ptr to ptr to current position in line */
    int *	pValue,		/* ptr where to return value */
    BOOL	defaultHex,	/* TRUE = arg is hex (even w/o 0x) */
    BOOL	optional	/* TRUE = ok if end of line */
    )
    {
    skipSpace (ppString);


    /* if nothing left, complain if arg is not optional */

    if (**ppString == EOS)
	{
	if (!optional)
	    {
	    printf ("missing parameter\n");
	    return (ERROR);
	    }
	else
	    return (OK);
	}


    /* scan arg */

    if (bootScanNum (ppString, pValue, defaultHex) != OK)
	{
	printf ("invalid parameter\n");
	return (ERROR);
	}

    skipSpace (ppString);

    /* if we encountered ',' delimiter, step over it */

    if (**ppString == ',')
	{
	++*ppString;
	return (OK);
	}

    /* if end of line, scan is ok */

    if (**ppString == EOS)
	return (OK);

    /* we got stopped by something else */

    printf ("invalid parameter\n");
    return (ERROR);
    }


/* The following routines are common to bootConfig and usrConfig and will
 * eventually be merged
 */

/******************************************************************************
*
* usrBootLineInit - initialize system boot line
*
* Initializes system boot line as per specified start type.
* If this is a COLD boot, i.e., with CLEAR option to clear memory,
* then the boot line is initialized from non-volatile RAM, if any,
* otherwise from the compiled in default boot line.
*/

LOCAL void usrBootLineInit 
    (
    int startType
    )
    {
    if (startType & BOOT_CLEAR)
	{
	/* this is a cold boot so get the default boot line */

	if ((sysNvRamGet (BOOT_LINE_ADRS, BOOT_LINE_SIZE, 0) == ERROR) ||
	    (*BOOT_LINE_ADRS == EOS))
	    {
	    /* either no non-volatile RAM or empty boot line */

	    strcpy (BOOT_LINE_ADRS, DEFAULT_BOOT_LINE);
	    }
	}
    }
/******************************************************************************
*
* usrBootLineCrack - interpret and verify boot line
*
* This routine interprets the specified boot line and checks the validity
* of certain parameters.  If there are errors, a diagnostic message is
* printed.
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS usrBootLineCrack 
    (
    char *	 bootString,
    BOOT_PARAMS *pParams
    )
    {
    FAST char *	pS;

    pS = bootStringToStruct (bootString, pParams);

    if (*pS != EOS)
	{
	bootParamsErrorPrint (bootString, pS);
	return (ERROR);
	}

#ifdef  INCLUDE_NETWORK
    /* check inet addresses */

    if ((checkInetAddrField (pParams->ead, TRUE) != OK) ||
	(checkInetAddrField (pParams->bad, TRUE) != OK) ||
	(checkInetAddrField (pParams->had, FALSE) != OK) ||
	(checkInetAddrField (pParams->gad, FALSE) != OK))
	{
	return (ERROR);
	}
#endif  /* INCLUDE_NETWORK */

    return (OK);
    }

#ifdef  INCLUDE_NETWORK

/******************************************************************************
*
* checkInetAddrField - check for valid inet address in boot field
*/

LOCAL STATUS checkInetAddrField 
    (
    char *pInetAddr,
    BOOL subnetMaskOK
    )
    {
    char inetAddr [30];
    int netmask;

    if (*pInetAddr == EOS)
	return (OK);

    strncpy (inetAddr, pInetAddr, sizeof (inetAddr));

    if (subnetMaskOK)
	{
	if (bootNetmaskExtract (inetAddr, &netmask) < 0)
	    {
	    printf ("Error: invalid netmask in boot field \"%s\".\n", inetAddr);
	    return (ERROR);
	    }
	}

    if (inet_addr (inetAddr) == ERROR)
	{
	printf ("Error: invalid inet address in boot field \"%s\".\n",inetAddr);
	return (ERROR);
	}

    return (OK);
    }

/******************************************************************************
*
* usrNetIfAttach - attach a network interface
*
* This routine attaches the specified network interface.
*
*	- interface is attached
*	- interface name is constructed as "<devName>0"
*
* RETURNS: OK or ERROR
*/

LOCAL STATUS usrNetIfAttach 
    (
    char *devName,
    char *inetAdrs
    )
    {
    FAST NETIF *	pNif;
    STATUS 		status;

    /* find interface in table */

    for (pNif = netIf; pNif->ifName != 0; pNif++)
	{
	if (strcmp (devName, pNif->ifName) == 0)
	    break;
	}

    if (pNif->ifName == 0)
	{
	printf ("Network interface %s unknown.\n", devName);
	return (ERROR);
	}

    printf ("Attaching network interface %s0... ", devName);

#if defined (TARGET_VIP10)
        /* SGI VIP10 boards are supposed to come with the ethernet address
         * in SGI formated non volatile ram.  We can not be sure where this
         * is so we default the upper 4 bytes of the address to SGI standard
         * and use the bottom two bytes of the internet address for the rest.
         */
        if (strcmp (devName, "lnsgi") == 0)
            {
            IMPORT char lnsgiEnetAddr [];      /* ethernet addr for lance */

            u_long inet = inet_addr (inetAdrs);
            lnsgiEnetAddr [4] = (inet >> 8) & 0xff;
            lnsgiEnetAddr [5] = inet & 0xff;
            }
#endif  /* TARGET_VIP10 */

    status = pNif->attachRtn (0, pNif->arg1, pNif->arg2, pNif->arg3,
			      pNif->arg4, pNif->arg5, pNif->arg6,
			      pNif->arg7, pNif->arg8);

    if (status != OK)
	{
        if (errno == S_iosLib_CONTROLLER_NOT_PRESENT)
            printf ("failed.\nError: S_iosLib_CONTROLLER_NOT_PRESENT.\n");
        else if (errno == S_iosLib_INVALID_ETHERNET_ADDRESS)
	    printf ("failed: S_iosLib_INVALID_ETHERNET_ADDRESS, use N command\n");
	else
	    printf ("failed: errno = %#x.\n", errno);
	return (ERROR);
	}

    printf ("done.\n");

    return (OK);
    }

/******************************************************************************
*
* usrNetIfConfig - configure a network interface
*	- subnetmask is extracted from inetAdrs and, if present,
*	  set for interface
*	- inet address is set for interface
*	- if present, inet name for interface is added to host table
*/

LOCAL STATUS usrNetIfConfig 
    (
    char *	devName,		/* device name */
    char *	inetAdrs,		/* inet address */
    char *	inetName,		/* host name */
    int 	netmask			/* subnet mask */
    )
    {
    char 	ifname [20];

    /* check for empty inet address */

    if (inetAdrs[0] == EOS)
	{
	printf ("No inet address specified for interface %s.\n", devName);
	return (ERROR);
	}

    /* build interface name */

    sprintf (ifname, "%s0", devName);

    /* set subnet mask, if any specified */

    if (netmask != 0)
	ifMaskSet (ifname, netmask);

    /* set inet address */

    if (ifAddrSet (ifname, inetAdrs) != OK)
        {
        printf ("Error setting inet address of %s to %s, errno = %#x\n",
                ifname, inetAdrs, errno);
        return (ERROR);
        }

    /* add host name to host table */

    if ((inetName != NULL) && (*inetName != EOS))
	hostAdd (inetName, inetAdrs);

    return (OK);
    }

/******************************************************************************
*
* usrBpInit - initialize backplane driver
*
* usrBpInit initializes the backplane driver shared memory region
* and sets up the backplane parameters to attach.
*
* RETURNS: OK if successful otherwise ERROR
*/

LOCAL STATUS usrBpInit 
    (
    char *	devName,	/* device name */
    u_long	startAddr	/* inet address */
    )
    {
#ifdef	INCLUDE_SM_NET
    char *		bpAnchor;	/* anchor address */
    FAST NETIF *	pNif;		/* netif struct */
    STATUS 		status;		/* status */
    int			procNum;	/* proc num */
    BOOL		newBP = TRUE;	/* old driver */

    /* Pick off optional "=<anchorAdrs>" from backplane
     * device.  Also truncates devName to just "bp" or "sm"
     */

    bpAnchor = SM_ANCHOR_ADRS;		/* default anchor */

    if ((strncmp (devName, "bp=", 3) == 0) ||
        (strncmp (devName, "sm=", 3) == 0))
	{
	if (bootBpAnchorExtract (devName, &bpAnchor) < 0)
	    {
	    printf ("Invalid anchor address specified: \"%s\"\n", devName);
	    return (ERROR);
	    }
	}

    if (strncmp (devName, "bp", 2) == 0)
	newBP = FALSE;

    procNum = sysProcNumGet ();

    /* if we are master, initialize backplane net */

    if (procNum == 0)
	{
	printf ("Initializing backplane net with anchor at %#x... ",
		(int) bpAnchor);

	if (newBP)
	    status = smNetInit ((SM_ANCHOR *) bpAnchor, (char *) SM_MEM_ADRS,
		               (int) SM_MEM_SIZE, SM_TAS_TYPE, 0, 0, startAddr);
	else
	    {					/* old backplane driver */
	    if (!SM_OFF_BOARD) 
		cacheDisable (DATA_CACHE);

	    status = bpInit (bpAnchor, (char *) SM_MEM_ADRS, (int) SM_MEM_SIZE,
			     SM_TAS_TYPE);
	    }

	if (status == ERROR)
  	    {
	    printf ("Error: backplane device %s not initialized\n", devName);
	    return (ERROR);
	    }

	printf ("done.\n");
	}

    /* Locate NETIF structure for backplane */

    for (pNif = netIf; pNif->ifName != 0; pNif++)
   	{
	if (strcmp (devName, pNif->ifName) == 0)
	    break;
	}

    if (pNif->ifName == 0)
	return (ERROR);

    printf ("Backplane anchor at %#x... ", (int) bpAnchor);

    /* configure backplane parameters (most set in NETIF struct) */

    pNif->arg1 = bpAnchor;		/* anchor address */
    pNif->arg3 = SM_INT_NONE;
    pNif->arg4 = 0;
    pNif->arg5 = 0;
    pNif->arg6 = 0;

    if (!newBP)
        pNif->arg2 = procNum;		/* proc num */

    return (OK);
#else	/* INCLUDE_SM_NET */

    printf ("\nError: backplane driver referenced but not included.\n");
    return (ERROR);
#endif	/* INCLUDE_SM_NET */
    }

/*******************************************************************************
*
* usrSlipInit - initialize the slip device
*
* RETURNS: OK if successful, otherwise ERROR.
*/

LOCAL STATUS usrSlipInit 
    (
    char *pBootDev,		/* boot device */
    char *localAddr,		/* local address */
    char *peerAddr		/* peer address */
    )
    {
#ifdef INCLUDE_SLIP
    char 	slTyDev [20];		/* slip device */
    int		netmask;		/* netmask */

#ifndef SLIP_BAUDRATE
#define SLIP_BAUDRATE 0 /* uses previously selected baudrate */
#endif

#ifdef	CSLIP_ENABLE
#undef	CSLIP_ENABLE
#define	CSLIP_ENABLE	TRUE	/* force CSLIP header compression */
#else	/* CSLIP_ENABLE */
#undef	CSLIP_ENABLE
#define	CSLIP_ENABLE	FALSE
#endif	/* CSLIP_ENABLE */

#ifdef	CSLIP_ALLOW
#undef	CSLIP_ALLOW
#define	CSLIP_ALLOW	TRUE	/* enable CSLIP compression on Rx */
#else	/* CSLIP_ALLOW */
#undef	CSLIP_ALLOW
#define	CSLIP_ALLOW	FALSE
#endif	/* CSLIP_ALLOW */

    if (pBootDev [2] == '=')
	{
	/* get the tty device used for SLIP interface e.g. "sl=/tyCo/1" */
	strcpy (slTyDev, &pBootDev [3]);
	pBootDev [2] = '\0';
	}
    else
	{
	/* construct the default SLIP tty device */
	sprintf (slTyDev, "%s%d", "/tyCo/", SLIP_TTY);
	}

    printf ("Attaching network interface %s0... ", pBootDev);

    bootNetmaskExtract (localAddr, &netmask); /* remove and ignore mask */

    if (slipInit (0, slTyDev, localAddr, peerAddr, SLIP_BAUDRATE,
        CSLIP_ENABLE, CSLIP_ALLOW) == ERROR)
	{
	printf ("\nslipInit failed 0x%x\n", errno);
	return (ERROR);
	}

    printf ("done.\n");
    return (OK);

#else /* INCLUDE_SLIP */
    printf ("\nError: slip not included.\n");
    return (ERROR);
#endif	/* INCLUDE_SLIP */
    }

/******************************************************************************
*
* bootpGet - get network parameters via BOOTP.
*
* RETURNS: OK if successful otherwise ERROR
*/

LOCAL STATUS bootpGet 
    (
    char *netDev,		/* boot device */
    char *pBootDevAddr,		/* device address */
    char *pBootFile,		/* file name */
    char *pHostAddr,		/* host address */
    int  *pMask			/* mask */
    )
    {
#ifdef INCLUDE_BOOTP
    char 	clntAddr [INET_ADDR_LEN];/* client address */
    char	bootServer [INET_ADDR_LEN];/* boot server */
    int		fileSize;		/* boot file size */
    int		subnetMask;		/* subnet mask */

    fileSize          = BOOT_FILE_LEN;
    subnetMask 	      = 0;
    bzero (clntAddr, INET_ADDR_LEN);
    bzero (bootServer, INET_ADDR_LEN);

    /* Need inet address to boot over the backplane */

    if ((strncmp (netDev, "bp", 2) == 0) || (strncmp (netDev, "sm", 2) == 0))
	{
	if (pBootDevAddr [0] == EOS)
	    return (ERROR);

	strncpy (clntAddr, pBootDevAddr, INET_ADDR_LEN);
	}


    printf ("Getting boot parameters via network interface %s", netDev);

    if (bootpParamsGet (netDev, 0, clntAddr, bootServer, pBootFile,
			&fileSize, &subnetMask, 0) == ERROR)
	return (ERROR);

    printf ("\nBootp Server:%s\n", bootServer);

    if (pBootFile [0] == EOS)
	return (ERROR);				/* no bootfile */

    printf ("    Boot file: %s\n", pBootFile);

    if (pHostAddr [0] == EOS)			/* fill in host address */
	{
	strncpy (pHostAddr, bootServer, INET_ADDR_LEN);
	printf ("    Boot host: %s\n", pHostAddr);
	}

    if (pBootDevAddr [0] == EOS)		/* fill in inet address */
	{
	strncpy (pBootDevAddr, clntAddr, INET_ADDR_LEN);
	printf ("    Boot device Addr (%s): %s\n", netDev, pBootDevAddr);
	}

    if ((*pMask == 0) && (subnetMask != 0))
	{
        *pMask = subnetMask;
	printf ("    Subnet mask: 0x%x\n", *pMask);
	}

    return (OK);
#else
    printf ("bootp referenced but not included\n");
    return (ERROR);
#endif
    }
#ifdef ETHERNET_ADR_SET
/*******************************************************************************
*
* mEnet - modify the last three bytes of the ethernet address
*
* RETURNS: void
*
* NOMANUAL
*/

void mEnet ()
    {
    uchar_t byte[MAX_ADR_SIZE];		/* array to hold new Enet addr */
    uchar_t curArr[MAX_ADR_SIZE];	/* array to hold current Enet addr */
    char line [MAX_LINE + 1];
    char *pLine;		/* ptr to current position in line */
    int value;			/* value found in line */
    char excess;
    int ix;

    sysEnetAddrGet (0, curArr);
    printf ("Current Ethernet Address is: ");

#if _BYTE_ORDER == _BIG_ENDIAN
    printf ("%02x:%02x:%02x:%02x:%02x:%02x\n", curArr[5], 
	    curArr[4], curArr[3], curArr[2], 
	    curArr[1], curArr[0]);
    byte[5] = ((ENET_DEFAULT & 0x0000ff00) >> 8);
    byte[4] = ((ENET_DEFAULT & 0x00ff0000) >> 16);
    byte[3] = ((ENET_DEFAULT & 0xff000000) >> 24);
    byte[2] = curArr[2];
    byte[1] = curArr[1];
    byte[0] = curArr[0];
#else  /* _BYTE_ORDER == _LITTLE_ENDIAN  */
    printf ("%02x:%02x:%02x:%02x:%02x:%02x\n", curArr[0], 
	    curArr[1], curArr[2], curArr[3], 
	    curArr[4], curArr[5]);
    byte[5] = ((ENET_DEFAULT & 0x00ff0000) >> 16);
    byte[4] = ((ENET_DEFAULT & 0x0000ff00) >> 8);
    byte[3] = (ENET_DEFAULT & 0x000000ff);
    byte[2] = curArr[3];
    byte[1] = curArr[4];
    byte[0] = curArr[5];
#endif /* _BYTE_ORDER == _BIG_ENDIAN */

    printf ("Modify only the last 3 bytes (board unique portion) of Ethernet Address.\n");
    printf ("The first 3 bytes are fixed at manufacturer's default address block.\n");

    for (ix = 5; ix > 2; ix--)
        printf ("%02x- %02x\n", byte[ix], byte[ix]);


    /* start on fourth byte of enet addr */
    for (ix = 2; ix >= 0; ix --)	
	{
	/* prompt for substitution */

	printf ("%02x- ", byte[ix]);

	/* get substitution value:
	 *   skip empty lines (CR only);
	 *   quit on end of file or invalid input;
	 *   otherwise put specified value at address */

	if (fioRdString (STD_IN, line, MAX_LINE) == EOF)
	    break;

	line[MAX_ADR_SIZE] = EOS;	/* make sure input line has EOS */

	for (pLine = line; isspace (*pLine); ++pLine)	/* skip leading spaces*/
	    ;

	if (*pLine == EOS)			/* skip field if just CR */
	    continue;

	if (sscanf (pLine, "%x%1s", &value, &excess) != 1)
	    break;				/* quit if not number */

	byte[ix]  = (uchar_t)value;		/* assign new value */
	}

    printf ("\n");

    sysEnetAddrSet (byte[5], byte[4], byte[3], byte[2], byte[1], byte[0]);

    printf ("New Ethernet Address is: ");
#if _BYTE_ORDER == _BIG_ENDIAN
    for (ix = 5; ix > 0; ix--)
        printf ("%02x:", byte[ix]);
    printf ("%02x\n", byte[0]);
#else  /* _BYTE_ORDER == _LITTLE_ENDIAN */
    for (ix = 5; ix > 0; ix--)
        printf ("%02x:", byte[ix]);
    printf ("%02x\n", byte[0]);
#endif /* _BYTE_ODER == _BIG_ENDIAN */
    }
#endif  /* ETHERNET_ADR_SET */
#endif  /* INCLUDE_NETWORK */
