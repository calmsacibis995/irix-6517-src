/* usrConfig.c - user-defined system configuration library */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
18n,07nov95,srh  fixed C++ support comments.
18m,29oct95,dat  fixed warnings about printf arguments
18l,11oct95,jdi  doc: changed .pG to .tG.
18k,10oct95,dat	 new BSP revision id. Added WDB Banner printing
18j,15jun95,ms	 updated for new serial drivers.
18i,09jun95,ms	 cleaned up console initialization
18h,30may95,p_m  added initialization of formatted I/O library.
		 replaced spyStop() by spyLibInit().
18g,22may95,p_m  no longer initialize object format specific libraries if
		 loader is not included.
18f,22may95,myz  modified new serial device initialization for x86 support
18e,22may95,ms   added some WDB agent support
18d,28mar95,kkk  added scalability support, changed edata and end to arrays
		 (SPR #3917), changed baud rate to be a macro in configAll.h
18c,20nov94,kdl  added hashLibInit() when using dosFs.
18b,09nov94,jds  additions for scsi backward compatability ; scsi[12]IfInit()
18a,04nov94,kdl	 merge cleanup.
17m,19oct93,cd   added ELF loader initialisation
17l,02aug94,tpr  added cacheEnable (BRANCH_CACHE) for the MC68060.
17l,23jul93,yao  fixed parameter cast for kernelInit().
17m,15oct94,hdn  added LPT driver.
17l,22oct93,hdn  added floppy and IDE drivers.
17l,08dec93,pad  added Am29K family support.
17o,20jul94,ms   changed INCLUDE_AOUT_HPPA to INCLUDE_SOM_COFF
17n,02may94,ms   added VxSim HP hack to usrClock().
17m,04nov93,gae  added INCLUDE_AOUT_HPPA.
17l,27jan92,gae  vxsim: added passFs.
17q,15mar94,smb  renamed tEvtTask parameters.
17p,15feb94,smb  defined WV_MODE, EVTBUFFER_ADDRESS for wvInstInit().
17o,12jan94,kdl  modified posix initialization; added queued signal init.
17n,30dec93,c_s  changed call to wvServerInit ()--uses new parameters
17m,10dec93,smb  instrumentation additions
17l,02dec93,dvs  added configuration for posix calls.
17k,15sep93,jmm  added call to stdioShowInit()
17j,23aug93,srh  added support for INCLUDE_CPLUS_MIN.
17i,13feb93,jcf  added some extra documentation.
17h,20jan93,jdi  documentation cleanup for 5.1.
17g,19oct92,jcf  added USER_[ID]_CACHE_ENABLE.
17f,01oct92,jcf  reworked cache initialization.
17e,30sep92,smb  moved usrSmObjInit before mmu initialization.
17d,24sep92,jcf  removed relative pathnames.
17c,24sep92,wmd  removed bzero() call for the I960.
17b,21sep92,ajm  fixes for lack of #elif support on mips
17a,18sep92,jcf  major cleanup.  bulk of code moved to ../../src/config/usr*.
16j,15sep92,ccc  fixed SCSI boot loading symbol table, init other (o) interface.
16i,14sep92,ajm  included all loader headers
16h,10sep92,caf  added BSP version to printConfig().
		 removed ethernet address kludge for frc30, frc37 and frc40.
16g,10sep92,jmm  added call to moduleLibInit(), fixed call to taskSpawn for demo
16f,08sep92,smb  made inclusion of gcc math routines for 680[0,1,2,3]0 only.
16e,07sep92,smb  added initialization header files for ANSI libraries.
                 forced inclusion of gcc math support routines.
16d,04sep92,ajm  added lnsgi interface for VIP10
16c,04sep92,ccc  added address modifier to ENP and EX attach routines.
16b,03sep92,rrr  added INCLUDE_SIGNALS
16a,02sep92,caf  changed to call sysNvRamGet() with NV_BOOT_OFFSET.
15z,23aug92,jcf  turned off cache/mmu for MC68040 temporarily.
15y,02aug92,gae  added includes of time.h, timers.h, timexLib.h, loadAoutLib.h;
	   +kdl	 added clock_gettime() to extra modules; removed call to 
		 clockLibInit().h
15x,01aug92,srh  added INCLUDE_CPLUS, sysCplusEnable initialization
15w,31jul92,dnw  changed sm init to use new cacheLib.
15v,31jul92,ccc  moved tyCoDrv() and tyCoDevCreate() prototypes to sysLib.h.
15u,30jul92,pme  added shared memory objects support.
15t,30jul92,elh  modified backplane initialization sequence. 
15s,30jul92,kdl  removed call to gccUss040Init(), now in 68k mathHardInit().
15r,30jul92,rdc  included mmuLib.h and rebootLib.h,
		 changed all sysToMonitor's to reboot's.
15q,30jul92,jmm  added tftp client and server initialization
                 revised POSIX timers
15p,30jul92,kdl	 cleaned up floating point initialization; added 
		 INCLUDE_FPP_SHOW; removed DBX references; added optional 
		 definition of 5.0 delete().
15o,28jul92,rdc  mmu configuration mods.
15n,27jul92,jcf  changed qInit call for ready queue.
15m,27jul92,rdc  moved vmLibInfo to funcBind.c. mmu configuration mods.
15l,25jul92,elh  changed BP macros to SM.
15k,21jul92,rdc  modified params to loadModuleAt in loadSymTbl.
15j,20jul92,jmm  added INCLUDE_LOADER and INCLUDE_UNLOADER
                 changed loadSymTbl to use new loader flags
15i,17jul92,rrr  removed manual add of the sparc integer symbols. Now done
                 by src/hutils/makeSymTbl.sh
15h,13jul92,rdc  changed vmLib.h to vmLibP.h, and updated vmLibInfo struct.
15g,12jul92,jcf  removed setjmp reference.
15f,09jul92,rdc  added vmLibInfo structure. 
15e,08jul92,rdc  added mmu support.
15d,04jul92,jcf  added scalable configuration.
15c,03jul92,jwt  added 5.1 cache support part III.
15b,03jul92,jwt  added 5.1 cache support part II.
15a,03jul92,jwt  added 5.1 cache support; deleted old cache code; deleted 
                 TARGET_SUN1E code in usrRoot(); removed some compiler 
                 warnings; removed TRAP_NUM from dbgInit() call.
14v,23jun92,elh  changed parameters to proxyNetCreate, 
		 added INCLUDE_BOUT (for rrr).
14u,09jun92,ajm  5.0.5 mips merge, object module independance
14t,02jun92,elh  the tree shuffle
14s,17may92,elh  deleted INCLUDE_STARTUP_SCRIPT_402, INCLUDE_NFS_MOUNT_402,
		 added default parameters, changed smVx -> smNet.
14r,03may92,elh  changed parameters to smVxInit.
14q,27apr92,ccc  added SCSI loading of symbol table.
14p,21apr92,jwt  added NICE (fn) ethernet driver.
14o,21apr92,ccc  fixed SCSI ANSI problems.
14n,16apr92,elh  Added in proxy arp. Changed parameters to usrNetIfConfig.
		 Added usrSlipInit.
14m,11apr92,elh  introduced usrBpInit.  Added sequential bp addressing.
14l,04apr92,elh  added in new backplane driver.
14k,19feb92,wmd  added predeclaration for nfsAuthUnixSet(), and scsiDebug.
14j,24jan92,gae  fixed another typo in #if when calling gccUss040Init().
14i,23jan92,shl  fixed typo in #if when calling gccUss040Init().
14h,08jan92,jcf  reworked cache configuration by removing cacheReset().
	    shl  added BUS_SNOOP.
		 added construction of ethernet address for frc40.
	    jwt  cleaned up ANSI compiler warning messages (TARGET_SUN1E).
14g,19dec91,rrr  removed nfsLib.h so it would compile. Needs to be fixed
14f,18dec91,jwt  merged SPARC 5.0.2b release - standalone symbols, NVRAM.
14e,16dec91,hdn  added a cacheDisable() if it is EXCELAN, for TRON.
14d,09dec91,rrr  added a lot of #includes to silence some ANSI warnings.
14c,04dec91,gae  added POSIX Timer initialization.
14b,25nov91,jpb  changed to ignore subnet masks for SLIP.
                 fixed some spelling errors.  silenced ANSI warnings.
14a,08nov91,rfs  added support for SN network driver.
13z,06nov91,rrr  added more forward function decls to shut up some warnings
13y,02oct91,shl  reorganized 68k cache initialization.
13x,08oct91,elh  split usrNetIfConfig into usrNetIf{Attach,Config}, moved slip
13w,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed READ, WRITE and UPDATE to O_RDONLY O_WRONLY O_RDWR
		  -changed VOID to void
		  -changed copyright notice
13v,02oct91,jwt  modified SPARC initialization portions based on 5.0.2 Beta-2.
13u,01octp1,yao  added conditional compile for STANDALONE_NET.
13t,19sep91,wmd  added invocation for mathInit() and fppInit() for MIPS.
	    ajm  made 5.02 changes, added mips architecture support
		 forward declared usrStartupScript402
		 bumped ring buffers for tty (look into this later)
                 used DEFAULT_BAUD from config.h for uart baud rate
13s,13sep91,jpb  added DMA address modifier to exattach argument list.
13r,19aug91,hdn  enabled STOREBUFFER for G200. added fppInit for G200.
13q,14aug91,yao  added configuration for slip. added hostAdrs parameter to
		 usrNetIfConfig() call. incorporated Intel's change for I960KB.
13p,22jul91,rfs  changed args to eiattach() for new ei driver.
13o,13jul91,gae  moved i960 PND & IMR bit specific code to BSPs.
13n,10jul91,del  added sysExcInit for target cvme960.
13m,20jun91,hdn  fixed #ifdef CPU_FAMILY==TRON, to #if.
13l,18jun91,hdn  added G100 specifics; set PSW in vector table for network if's.
13k,21may91,jwt  cleanup up backplane and cache disabling concept,
		 merged SPARC 4.0.2 usrConfig.c.
13j,19may91,gae  moved BOOT_LINE_SIZE to configAll.h.
13i,18may91,gae  added delay for error printing when loadSymTbl() fails.
		 changed NV_RAM_SIZE to BOOT_LINE_SIZE - now fixed at 256.
		 added diagnostics for standalone symbol table initialization.
13h,29apr91,hdn  added TRON conditionals for cache enable/disable.
13g,29apr91,del  added I960 support. Including 82596 network interface.
13f,26apr91,shl  added forward declaration of usrStartupScript402() (spr 920).
13e,25apr91,elh  added mbufConfig and clusterConfig, moved rpc
		 references to rpcInit, added ftpd.
13d,03apr91,jdi  documentation cleanup; doc review by dnw.
*/

/*
DESCRIPTION
This library is the WRS-supplied configuration module for VxWorks.  It
contains the root task, the primary system initialization routine, the
network initialization routine, and the clock interrupt routine.

The include file config.h includes a number of system-dependent parameters used
in this file.

In an effort to simplify the presentation of the configuration of vxWorks,
this file has been split into smaller files.  These additional configuration
source files are located in ../../src/config/usr[xxx].c and are #included into
this file below.  This file contains the bulk of the code a customer is
likely to customize.

The module usrDepend.c contains checks that guard against unsupported
configurations suchas INCLUDE_NFS without INCLUDE_RPC.  The module
usrKernel.c contains the core initialization of the kernel which is rarely
customized, but provided for information.  The module usrNetwork.c now
contains all network initialization code.  Finally, the module usrExtra.c
contains the conditional inclusion of the optional packages selected in
configAll.h.

The source code necessary for the configuration selected is entirely
included in this file during compilation as part of a standard build in
the board support package.  No other make is necessary.

INCLUDE FILES:
config.h

SEE ALSO:
.tG "Getting Started, Cross-Development"
*/

#include "vxworks.h"			/* always first */
#include "config.h"			/* board support configuration header */
#include "usrconfig.h"			/* general configuration header */
#include "initffsc.h"			/* FFSC-specific header */
#include "usrdepend.c"			/* include dependency rules */
#include "usrkernel.c"			/* kernel configuration */
#include "usrextra.c"			/* conditionally included packages */

/* global variables */

SYMTAB_ID	statSymTbl;		/* system error status symbol table id*/
SYMTAB_ID	standAloneSymTbl;	/* STANDALONE version symbol table id */
SYMTAB_ID	sysSymTbl;		/* system symbol table id */
BOOT_PARAMS	sysBootParams;		/* parameters from boot line */
int		sysStartType;		/* type of boot (WARM, COLD, etc) */

char *          bufferAddress;          /* windview: address of event buffer */

/*******************************************************************************
*
* usrInit - user-defined system initialization routine
*
* This is the first C code executed after the system boots.  This routine is
* called by the assembly language start-up routine sysInit() which is in the
* sysALib module of the target-specific directory.  It is called with
* interrupts locked out.  The kernel is not multitasking at this point.
*
* This routine starts by clearing BSS; thus all variables are initialized to 0,
* as per the C specification.  It then initializes the hardware by calling
* sysHwInit(), sets up the interrupt/exception vectors, and starts kernel
* multitasking with usrRoot() as the root task.
*
* RETURNS: N/A
*
* SEE ALSO: kernelLib
*
* ARGSUSED0
*/

void usrInit 
    (
    int startType
    )
    {
#if	(CPU_FAMILY == SPARC)
    excWindowInit ();				/* SPARC window management */
#endif

    /* configure data and instruction cache if available and leave disabled */

#ifdef  INCLUDE_CACHE_SUPPORT
    cacheLibInit (USER_I_CACHE_MODE, USER_D_CACHE_MODE);
#endif  /* INCLUDE_CACHE_SUPPORT */

#if	CPU_FAMILY!=SIMSPARCSUNOS && CPU_FAMILY!=SIMHPPA
    /* don't assume bss variables are zero before this call */

    bzero (edata, end - edata);		/* zero out bss variables */
#endif	/* CPU_FAMILY!=SIMSPARCSUNOS && CPU_FAMILY!=SIMHPPA */

    sysStartType = startType;			/* save type of system start */

    intVecBaseSet ((FUNCPTR *) VEC_BASE_ADRS);	/* set vector base table */

#if (CPU_FAMILY == AM29XXX)
    excSpillFillInit ();			/* am29k stack cache managemt */
#endif

#ifdef  INCLUDE_EXC_HANDLING
    excVecInit ();				/* install exception vectors */
    excShowInit ();				/* install sysExcMsg handlers */
#endif  /* INCLUDE_EXC_HANDLING */

    sysHwInit ();				/* initialize system hardware */

    usrKernelInit ();				/* configure the Wind kernel */

#ifdef  INCLUDE_CACHE_SUPPORT
#ifdef 	USER_I_CACHE_ENABLE
    cacheEnable (INSTRUCTION_CACHE);		/* enable instruction cache */
#endif	/* USER_I_CACHE_ENABLE */

#ifdef	USER_D_CACHE_ENABLE
    cacheEnable (DATA_CACHE);			/* enable data cache */
#endif 	/* USER_D_CACHE_ENABLE */

#if (CPU == MC68060)
#ifdef 	USER_B_CACHE_ENABLE
    cacheEnable (BRANCH_CACHE);			/* enable branch cache */
#endif	/* USER_B_CACHE_ENABLE */
#endif	/* (CPU == MC68060) */
#endif  /* INCLUDE_CACHE_SUPPORT */

    /* start the kernel specifying usrRoot as the root task */

    kernelInit ((FUNCPTR) usrRoot, ROOT_STACK_SIZE, (char *) FREE_RAM_ADRS, 
		sysMemTop (), ISR_STACK_SIZE, INT_LOCK_LEVEL);
    }

/*******************************************************************************
*
* usrRoot - the root task
*
* This is the first task to run under the multitasking kernel.  It performs
* all final initialization and then starts other tasks.
*
* It initializes the I/O system, installs drivers, creates devices, and sets
* up the network, etc., as necessary for a particular configuration.  It
* may also create and load the system symbol table, if one is to be included.
* It may then load and spawn additional tasks as needed.  In the default
* configuration, it simply initializes the VxWorks shell.
*
* RETURNS: N/A
*/

void usrRoot
    (
    char *	pMemPoolStart,		/* start of system memory partition */
    unsigned	memPoolSize		/* initial size of mem pool */
    )
    {
#if defined(INCLUDE_STAT_SYM_TBL) || defined(INCLUDE_STANDALONE_SYM_TBL)
	int ix;
#endif

    /* Initialize the memory pool before initializing any other package.
     * The memory associated with the root task will be reclaimed at the
     * completion of its activities.
     */
#ifdef INCLUDE_SELECT
	_func_selWakeupListInit = (FUNCPTR) selWakeupListInit;
#endif

#ifdef INCLUDE_MEM_MGR_FULL
    memInit (pMemPoolStart, memPoolSize);	/* initialize memory pool */
#else
    memPartLibInit (pMemPoolStart, memPoolSize);/* initialize memory pool */
#endif /* INCLUDE_MEM_MGR_FULL */

#ifdef	INCLUDE_SHOW_ROUTINES 
    memShowInit ();				/* initialize memShow routine */
#endif	/* INCLUDE_SHOW_ROUTINES */

#if	defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL)
    usrMmuInit ();				/* initialize the mmu */
#endif	/* defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL) */


    /* set up system timer */

    sysClkConnect ((FUNCPTR) usrClock, 0);	/* connect clock ISR */
    sysClkRateSet (60);				/* set system clock rate */
    sysClkEnable ();				/* start it */

    /* RDB: Do early FFSC-specific initialization (including I/O) */
    /*      ** No system I/O must be done before this point **    */
    ffscEarlyInit();

    /* initialize symbol table facilities */

#ifdef	INCLUDE_SYM_TBL
    hashLibInit ();			/* initialize hash table package */
    symLibInit ();			/* initialize symbol table package */
#ifdef 	INCLUDE_SHOW_ROUTINES 
    symShowInit ();			/* initialize symbol table show */
#endif	/* INCLUDE_SHOW_ROUTINES */
#endif	/* INCLUDE_SYM_TBL */


    /* initialize exception handling */

#if     defined(INCLUDE_EXC_HANDLING) && defined(INCLUDE_EXC_TASK)
    excInit ();				/* initialize exception handling */
#endif  /* defined(INCLUDE_EXC_HANDLING) && defined(INCLUDE_EXC_TASK) */

#ifdef	INCLUDE_SIGNALS
    sigInit ();				/* initialize signals */
#endif	/* INCLUDE_SIGNALS */


    /* initialize debugging */

#ifdef	INCLUDE_DEBUG
    dbgInit ();				/* initialize debugging */
#endif	/* INCLUDE_DEBUG */


    /* initialize pipe driver */

#ifdef	INCLUDE_PIPES
    pipeDrv ();				/* install pipe driver */
#endif	/* INCLUDE_PIPES */


    /* initialize standard I/O package */

#ifdef	INCLUDE_STDIO
    stdioInit ();			/* initialize standard I/O library */
#ifdef  INCLUDE_SHOW_ROUTINES
    stdioShowInit ();
#endif  /* INCLUDE_SHOW_ROUTINES */
#endif	/* INCLUDE_STDIO */


    /* initialize POSIX queued signals */

#if defined(INCLUDE_POSIX_SIGNALS) && defined(INCLUDE_SIGNALS)
    sigqueueInit (NUM_SIGNAL_QUEUES); /* initialize queued signals */
#endif

    /* initialize POSIX semaphores */

#ifdef  INCLUDE_POSIX_SEM
    semPxLibInit ();			
#ifdef INCLUDE_SHOW_ROUTINES
    semPxShowInit ();
#endif  /* INCLUDE_SHOW_POUTINES */
#endif  /* INCLUDE_POSIX_SEM */

    /* initialize POSIX message queues */

#ifdef INCLUDE_POSIX_MQ
    mqPxLibInit (MQ_HASH_SIZE);		
#ifdef INCLUDE_SHOW_ROUTINES
    mqPxShowInit ();
#endif  /* INCLUDE_SHOW_ROUTINES */
#endif  /* INCLUDE_POSIX_MQ */

    /* initialize POSIX async I/O support */

#ifdef INCLUDE_POSIX_AIO
    aioPxLibInit (MAX_LIO_CALLS);
#ifdef INCLUDE_POSIX_AIO_SYSDRV
    aioSysInit (MAX_AIO_SYS_TASKS, AIO_TASK_PRIORITY, AIO_TASK_STACK_SIZE);
#endif  /* INCLUDE_POSIX_AIO_SYSDRV */
#endif  /* INCLUDE_POSIX_AIO */

    /* initialize filesystems and disk drivers */

#ifdef	INCLUDE_DOSFS
    hashLibInit ();			/* initialize hash table package */
    dosFsInit (NUM_DOSFS_FILES); 	/* init dosFs filesystem */
#endif	/* INCLUDE_DOSFS */

#ifdef	INCLUDE_RAWFS
    rawFsInit (NUM_RAWFS_FILES); 	/* init rawFs filesystem */
#endif	/* INCLUDE_RAWFS */

#ifdef	INCLUDE_RT11FS
    rt11FsInit (NUM_RT11FS_FILES); 	/* init rt11Fs filesystem */
#endif	/* INCLUDE_RT11FS */

#ifdef	INCLUDE_RAMDRV
    ramDrv ();				/* initialize ram disk driver */
#endif	/* INCLUDE_RAMDRV */


#ifdef	INCLUDE_SCSI

    /*
     * initialize either the SCSI1 or SCSI2 interface; initialize SCSI2 when 
     * the SCSI2 interface is available.
     */

#ifndef INCLUDE_SCSI2
    scsi1IfInit ();
#else
    scsi2IfInit ();
#endif
    sysScsiInit ();			/* initialize SCSI controller */
    usrScsiConfig ();			/* configure SCSI peripherals */

#endif	/* INCLUDE_SCSI */

#ifdef  INCLUDE_FD
    fdDrv (FD_INT_VEC, FD_INT_LVL);     /* initialize floppy disk driver */
#endif  /* INCLUDE_FD */

#ifdef  INCLUDE_IDE
    ideDrv (IDE_INT_VEC, IDE_INT_LVL, IDE_CONFIG); /* init IDE disk driver */
#endif  /* INCLUDE_IDE */

#ifdef  INCLUDE_LPT
    lptDrv (LPT_INT_VEC, LPT_INT_LVL); /* init LPT parallel driver */
#endif  /* INCLUDE_LPT */

#ifdef  INCLUDE_FORMATTED_IO
    fioLibInit ();			/* initialize formatted I/O */
#endif  /* INCLUDE_FORMATTED_IO */

    /* initialize floating point facilities */

#ifdef	INCLUDE_FLOATING_POINT
    floatInit ();			/* initialize floating point I/O */
#endif	/* INCLUDE_FLOATING_POINT */

    /* install software floating point emulation (if applicable) */

#ifdef	INCLUDE_SW_FP
    mathSoftInit ();			/* use software emulation for fp math */
#endif	/* INCLUDE_SW_FP */

    /* install hardware floating point support (if applicable) */

#ifdef	INCLUDE_HW_FP
    mathHardInit (); 			/* do fppInit() & install hw fp math */

#ifdef	INCLUDE_SHOW_ROUTINES 
    fppShowInit ();			/* install hardware fp show routine */
#endif	/* INCLUDE_SHOW_ROUTINES */
#endif	/* INCLUDE_HW_FP */


    /* initialize performance monitoring tools */

#ifdef	INCLUDE_SPY
    spyLibInit ();			/* install task cpu utilization tool */
#endif	/* INCLUDE_SPY */

#ifdef	INCLUDE_TIMEX
    timexInit ();			/* install function timing tool */
#endif	/* INCLUDE_TIMEX */

#ifdef  INCLUDE_ENV_VARS
    envLibInit (ENV_VAR_USE_HOOKS);	/* initialize environment variable */
#endif	/* INCLUDE_ENV_VARS */


    /* initialize object module loader */

#ifdef	INCLUDE_LOADER
    moduleLibInit ();			/* initialize module manager */

#if	defined(INCLUDE_AOUT)
    loadAoutInit ();				/* use a.out format */
#else	/* coff or ecoff */
#if	defined(INCLUDE_ECOFF)
    loadEcoffInit ();				/* use ecoff format */
#else	/* ecoff */
#if	defined(INCLUDE_COFF)
    loadCoffInit ();				/* use coff format */
#else   /* coff */
#if	defined(INCLUDE_ELF)
    loadElfInit ();				/* use elf format */
#else
#if	defined(INCLUDE_SOM_COFF)
    loadSomCoffInit ();
#endif
#endif
#endif
#endif
#endif

#endif	/* INCLUDE_LOADER */

	/* RDB: Do FFSC-specific initialization (including network) */
	ffscInit();

#ifdef	INCLUDE_PASSFS
    {
    extern STATUS passFsInit ();
    extern void *passFsDevInit ();
    char passName [256];

    if (passFsInit (1) == OK)
	{
	extern char vxsim_hostname[];
	extern char vxsim_cwd[];

	sprintf (passName, "%s:", vxsim_hostname);
	if (passFsDevInit (passName) == NULL)
	    {
	    printf ("passFsDevInit failed for <%s>\n", passName);
	    }
	else
	    {
	    sprintf (passName, "%s:%s", vxsim_hostname, vxsim_cwd);
	    ioDefPathSet (passName);
	    }
	}
    else
	printf ("passFsInit failed\n");
    }
#endif	/* INCLUDE_PASSFS */

#ifdef	INCLUDE_DOS_DISK
    {
    char unixName [80];
    extern void unixDrv ();
    extern void unixDiskInit ();
    extern char *u_progname;  /* home of executable */
    char *pLastSlash;

    unixDrv ();

    pLastSlash = strrchr (u_progname, '/');
    pLastSlash = (pLastSlash == NULL) ? u_progname : (pLastSlash + 1);
    sprintf (unixName, "/tmp/%s%d.dos", pLastSlash, sysProcNumGet());
    unixDiskInit (unixName, "A:", 0);
    }
#endif	/* INCLUDE_DOS_DISK */

    /* initialize shared memory objects */

#ifdef INCLUDE_SM_OBJ			/* unbundled shared memory objects */
    usrSmObjInit (BOOT_LINE_ADRS);
#endif /* INCLUDE_SM_OBJ */

    /* write protect text segment & vector table only after bpattach () */

#ifdef	INCLUDE_MMU_FULL		/* unbundled mmu product */
#ifdef	INCLUDE_PROTECT_TEXT
    if (vmTextProtect () != OK)
	printf ("\nError protecting text segment. errno = %x\n", errno);
#endif	/* INCLUDE_PROTECT_TEXT */

#ifdef	INCLUDE_PROTECT_VEC_TABLE
    if (intVecTableWriteProtect () != OK)
	printf ("\nError protecting vector table. errno = %x\n", errno);
#endif	/* INCLUDE_PROTECT_VEC_TABLE */
#endif	/* INCLUDE_MMU_FULL */


    /* create system and status symbol tables */

#ifdef  INCLUDE_STANDALONE_SYM_TBL
    sysSymTbl = symTblCreate (SYM_TBL_HASH_SIZE_LOG2, TRUE, memSysPartId);

    printf ("\nAdding %ld symbols for standalone.\n", standTblSize);

    for (ix = 0; ix < standTblSize; ix++)	/* fill in from built in table*/
	symTblAdd (sysSymTbl, &(standTbl[ix]));
#endif	/* INCLUDE_STANDALONE_SYM_TBL */

#ifdef  INCLUDE_STAT_SYM_TBL
    statSymTbl = symTblCreate (STAT_TBL_HASH_SIZE_LOG2, FALSE, memSysPartId);

    for (ix = 0; ix < statTblSize; ix ++)	/* fill in from built in table*/
	symTblAdd (statSymTbl, &(statTbl [ix]));
#endif	/* INCLUDE_STAT_SYM_TBL */


    /* initialize C++ support library */

#if	defined (INCLUDE_CPLUS) && defined (INCLUDE_CPLUS_MIN)
#error	Define only one of INCLUDE_CPLUS or INCLUDE_CPLUS_MIN, not both
#endif

#ifdef	INCLUDE_CPLUS			/* all standard C++ runtime support */
    cplusLibInit ();
#endif

#ifdef	INCLUDE_CPLUS_MIN		/* minimal C++ runtime support */
    cplusLibMinInit ();
#endif

    /* initialize windview support */

#ifdef  INCLUDE_INSTRUMENTATION

    /* initialize timer support for windview */

#ifdef  INCLUDE_TIMESTAMP
#ifdef  INCLUDE_USER_TIMESTAMP
    wvTmrRegister ((UINTFUNCPTR) USER_TIMESTAMP,
    		   (UINTFUNCPTR) USER_TIMESTAMPLOCK,
    		   (FUNCPTR)     USER_TIMEENABLE,
    		   (FUNCPTR)     USER_TIMEDISABLE,
    		   (FUNCPTR)     USER_TIMECONNECT,
    		   (UINTFUNCPTR) USER_TIMEPERIOD,
    		   (UINTFUNCPTR) USER_TIMEFREQ);
#else	/* INCLUDE_USER_TIMESTAMP */
    wvTmrRegister (sysTimestamp,
    		   sysTimestampLock,
    		   sysTimestampEnable,
    		   sysTimestampDisable,
    		   sysTimestampConnect,
    		   sysTimestampPeriod,
    		   sysTimestampFreq);
#endif  /* INCLUDE_USER_TIMESTAMP */
#else	/* INCLUDE_TIMESTAMP */
    wvTmrRegister (seqStamp,
    		   seqStampLock,
    		   seqEnable,
    		   seqDisable,
    		   seqConnect,
    		   seqPeriod,
    		   seqFreq);
#endif  /* INCLUDE_TIMESTAMP */


    /* initialize WindView host target connection */

    connRtnSet (evtSockInit, evtSockClose, evtSockError, evtSockDataTransfer);

    /* set the characteristics of the event task - tEvtTask */

    wvEvtTaskInit (WV_EVT_STACK, WV_EVT_PRIORITY);		

    /* initialize windview instrumentation buffers and tasks */

    bufferAddress = wvInstInit (EVTBUFFER_ADDRESS, EVTBUFFER_SIZE, WV_MODE);

    /* initialize WindView command server */

#ifdef INCLUDE_WINDVIEW
    wvServerInit (WV_SERVER_STACK, WV_SERVER_PRIORITY);
#endif /* INCLUDE_WINDVIEW */

#endif  /* INCLUDE_INSTRUMENTATION */

    /* initialize the WDB debug agent */

#ifdef  INCLUDE_WDB
    wdbConfig();

#ifdef	INCLUDE_WDB_BANNER
#ifndef INCLUDE_SHELL
    /* short banner, like the bootPrintLogo banner */
    printf ("\n\n");
    printf ("%17s%s",     "","VxWorks\n\n");
    printf ("Copyright 1984-1995  Wind River Systems, Inc.\n\n");
    printf ("            CPU: %s\n", sysModel ());
    printf ("        VxWorks: " VXWORKS_VERSION "\n");
    printf ("    BSP version: " BSP_VERSION BSP_REV "\n");
    printf ("  Creation date: %s\n", __DATE__);
    printf ("            WDB: %s.\n\n",
	    ((wdbRunsExternal () || wdbRunsTasking ()) ?
		 "Ready" : "Agent configuration failed") );
#endif /*INCLUDE_SHELL*/

#endif /*INCLUDE_WDB_BANNER*/

#endif  /* INCLUDE_WDB */

    /* initialize interactive shell */

#ifdef  INCLUDE_SHELL
#ifdef	INCLUDE_SECURITY			/* include shell security */
    if ((sysFlags & SYSFLG_NO_SECURITY) == 0)
	{
        loginInit ();				/* initialize login table */
        shellLoginInstall (loginPrompt, NULL);	/* install security program */

	/* add additional users here as required */

        loginUserAdd (LOGIN_USER_NAME, LOGIN_PASSWORD);	
	}
#endif	/* INCLUDE_SECURITY */

    printLogo ();				/* print out the banner page */

    printf ("                               ");
    printf ("CPU: %s.  Processor #%d.\n", sysModel (), sysProcNumGet ());
    printf ("                              ");
    printf ("Memory Size: 0x%x.", (UINT)(sysMemTop () - (char *)LOCAL_MEM_LOCAL_ADRS));
    printf ("  BSP version " BSP_VERSION BSP_REV ".");
#if defined(INCLUDE_WDB) && defined(INCLUDE_WDB_BANNER)
    printf ("\n                             ");
    printf ("WDB: %s.",
	    ((wdbRunsExternal () || wdbRunsTasking ()) ?
		 "Ready" : "Agent configuration failed") );
#endif /*INCLUDE_WDB && INCLUDE_WDB_BANNER*/
    printf ("\n\n");

#ifdef	INCLUDE_STARTUP_SCRIPT			/* run a startup script */
    if (sysBootParams.startupScript [0] != EOS)
	usrStartupScript (sysBootParams.startupScript);
#endif	/* INCLUDE_STARTUP_SCRIPT */

    shellInit (SHELL_STACK_SIZE, TRUE);		/* create the shell */


    /* only include the simple demo if the shell is NOT included */

#else
#if defined(INCLUDE_DEMO)			/* create demo w/o shell */
    taskSpawn ("demo", 20, 0, 2000, (FUNCPTR)usrDemo, 0,0,0,0,0,0,0,0,0,0);
#endif						/* mips cpp no elif */
#endif	/* INCLUDE_SHELL */

    }

/*******************************************************************************
*
* usrClock - user-defined system clock interrupt routine
*
* This routine is called at interrupt level on each clock interrupt.
* It is installed by usrRoot() with a sysClkConnect() call.
* It calls all the other packages that need to know about clock ticks,
* including the kernel itself.
*
* If the application needs anything to happen at the system clock interrupt
* level, it can be added to this routine.
*
* RETURNS: N/A
*/

void usrClock ()

    {
#define XDB_FIX
#if defined(XDB_FIX) && (CPU == SIMHPPA)
    static int cnt;
    if (++cnt > 60);
        {
        cnt = 0;
        s_sigcatch (22, 0, 0, 0);
        }
#endif /* XDB_FIX */

    tickAnnounce ();	/* announce system tick to kernel */
    }

#ifdef	INCLUDE_DEMO

/********************************************************************************
* usrDemo - example application without shell
*
* This routine is spawned as a task at the end of usrRoot(), if INCLUDE_DEMO
* is defined, and INCLUDE_SHELL is NOT defined in configAll.h or config.h.
* It is intended to indicate how a shell-less application can be linked,
* loaded, and ROMed.
*
* NOMANUAL
*/

void usrDemo (void)

    {
    char string [40];

    printf ("VxWorks (for %s) version %s.\n", sysModel (), vxWorksVersion);
    printf ("Kernel: %s.\n", kernelVersion ());
    printf ("Made on %s.\n", creationDate);

    FOREVER
        {
        printf ("\nThis is a test.  Type something: ");
        fioRdString (STD_IN, string, sizeof (string));
	printf ("\nYou typed \"%s\".\n", string);

	if (strcmp (string, "0") == 0)
	    memShow (0);

	if (strcmp (string, "1") == 0)
	    memShow (1);
        }
    }

#endif	/* INCLUDE_DEMO */
