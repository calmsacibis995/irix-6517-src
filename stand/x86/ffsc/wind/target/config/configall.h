/* configAll.h - default configuration header */

/* Copyright 1984-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
06e,11oct95,ms   made 5.2 configuration easier to recreate (SPR #5134).
06d,10oct95,dat	 backward compatible BSP_VERSION and BSP_REV
06c,28sep95,dat	 new #define INCLUDE_WDB_ANNOUNCE
06b,21sep95,ms	 switched some WDB macros between here and usrWdb.c
06a,27jun95,ms	 renamed WDB_COMM_XXX macros
05z,21jun95,ms	 added INCLUDE_WDB_TTY_TEST and INCLUDE_WDB_EXIT_NOTIFY
05y,21jun95,tpr  added #define INCLUDE_WDB_MEM.
05x,13jun95,srh  Updated C++ support triggers.
05w,07jun95,ms	 WDB_STACK_SIZE is now CPU dependant.
05v,07jun95,p_m  added INCLUDE_FORMATTED_IO. suppressed spy from default
                 configuration.
05u,01jun95,ms	 all WDB macros start with WDB_ (usrWdb.c modhist has details).
05t,22may95,ms   added WDB agent support.
          + p_m  suppressed shell, symbol table and debug support from default
		 configuration.
05s,29mar95,kdl  added INCLUDE_GCC_FP.
05r,28mar95,kkk  added scalability MACROS, changed edata and end to arrays 
		 (SPR #3917), added misc constants SM_PKTS_SIZE and 
		 SM_CPUS_MAX (SPR #4130), added misc constant CONSOLE_BAUD_RATE
05q,24mar95,tpr  added #define USER_B_CACHE_ENABLE (SPR #4168). 
05p,14mar95,caf  restored mips resident rom support (SPR #3856).
05o,18jan95,tmk  Added MC68060 case for HW_FP
05n,10dec94,kdl  Moved INCLUDE_POSIX_ALL to unincluded (SPR 3822).
05m,10dec94,caf  undid mod 05a, use _sdata for resident roms (SPR #3856).
05l,09dec94,jag  Added INCLUDE_MIB2_AT.
05k,17nov94,kdl  Added INCLUDE_NFS_SERVER (excluded); removed TRON references.
05j,13nov94,dzb  Moved INCLUDE_PING to excluded.
05i,11nov94,dzb  Added ZBUF_SOCK, TCP_DEBUG, and PING defines.
05h,11nov94,jag  cleanup of SNMP and MIB defines.
05g,04nov94,kdl	 initial merge cleanup.
05b,16jun94,caf  defined INCLUDE_ELF for MIPS, updated copyright notice.
05a,27oct93,caf  for resident roms: use RAM_DST_ADRS instead of _sdata.
04v,20nov93,hdn  added support for I80X86.
04v,02dec93,pad  added Am29K defines.
04x,20jul94,ms   changed INCLUDE_HPPA_OUT to INCLUDE_SOM_COFF
04w,11aug93,gae  simhppa.
04v,23jun93,gae  vxsim.
05d,11apr94,jag  Removed conditions for definition of  NFS_GROUP and NFS_USER
		 ID for SNMP demo support.
05c,18feb94,elh  (SNMP VERSION) moved INCLUDE_POSIX_ALL to false section.
		 Added support for the mib2 library.
05f,25may94,kdl	 (POSIX VERSION) removed erroneous NFS def's.
05e,23mar94,smb	 (POSIX VERSION) removed PASSIVE_MODE
05d,15mar94,smb	 renamed tEvtTask parameters.
05c,15feb94,smb	 added define of WV_MODE
05b,12jan94,kdl	 (POSIX VERSION) turned off instrumentation, added 
		 INCLUDE_POSIX_ALL; added INCLUDE_POSIX_SIGNALS; changed
		 INCLUDE_POSIX_MEM_MAN to INCLUDE_POSIX_MEM; added
		 NUM_SIGNAL_QUEUES.
05a,30dec93,c_s  added WV_SERVER_STACK and WV_SERVER_PRIORITY.
04z,16dec93,smb	 turned off erroneous include of dosFs, ramDrv.
04y,10dec93,smb  instrumentation additions 
04x,03dec93,dvs  added INCLUDE_POSIX_AIO_SYSDRV, INCLUDE_POSIX_MQ, MQ_HASH_SIZE,
		 MAX_AIO_SYS_TASKS, AIO_TASK_PRIORITY, and AIO_TASK_STACK_SIZE
04v,22nov93,dvs  added INCLUDE_POSIX_SCHED, INCLUDE_POSIX_MEM_MAN, 
		 INCLUDE_POSIX_SEM, INCLUDE_POSIX_FTRUNC, INCLUDE_POSIX_AIO,
		 and MAX_LIO_CALLS
04z,16dec93,smb	 turned off erroneous include of dosFs, ramDrv.
04y,10dec93,smb  instrumentation additions 
04x,03dec93,dvs  added INCLUDE_POSIX_AIO_SYSDRV, INCLUDE_POSIX_MQ, MQ_HASH_SIZE,
		 MAX_AIO_SYS_TASKS, AIO_TASK_PRIORITY, and AIO_TASK_STACK_SIZE
04v,22nov93,dvs  added INCLUDE_POSIX_SCHED, INCLUDE_POSIX_MEM_MAN, 
		 INCLUDE_POSIX_SEM, INCLUDE_POSIX_FTRUNC, INCLUDE_POSIX_AIO,
		 and MAX_LIO_CALLS
04u,27sep93,jmm  removed DOS, NFS, RAMDRV, RAWFS, RPC, RT11FS for 5.1.1 ship
04t,02sep93,kdl  fixed definition of INCLUDE_ANSI_5_0 (SPR #2464).
04s,23aug93,srh  added INCLUDE_CPLUS_MIN to excluded facilities.
04r,02mar93,yao  RESERVED for SPARC no longer function of LOCAL_MEM_LOCAL_ADRS.
04q,27feb93,rdc  added some comments about MMU.
04p,12feb93,jag  changed INCLUDE_PROXY_ARP->INCLUDE_PROXY_SERVER,
                 INCLUDE_SM_NETAUTO->INCLUDE_SM_SEQ_ADDR. Added 
		 INCLUDE_PROXY_DEFAULT_ADDR. All defines except INCLUDE_SM_SEQ_
                 ADDR are in the excluded section.
04o,31oct92,jcf  removed hw floating point support of 68000/68010.
04n,30oct92,wmd  changed STACK_SAVE to 512 for i960.
04m,19oct92,jcf  added USER_[ID]_CACHE_ENABLE.
04l,03oct92,kdl  added INCLUDE_SM_NETAUTO to excluded options.
04k,01oct92,jcf  added USER_[ID]_CACHE_MODE.
04j,30sep92,smb  moved INCLUDE_SECURITY in the excluded facilities section.
04i,23sep92,kdl  name changes (FTPD -> FTP_SERVER, PROXYCLNT -> PROXY_CLIENT,
		 PROXYARP -> PROXY_ARP).
04h,22sep92,jcf  added VEC_BASE_ADRS for I960.
04g,21sep92,ajm  fixes for lack of #elif support on mips
04f,19sep92,jcf  fixed typo.
04e,18sep92,jcf  cleaned up.
04d,07sep92,smb  added INCLUDE_ANSI_ALL
04c,03sep92,rrr  added INCLUDE_SIGNALS
04b,02sep92,caf  added NV_BOOT_OFFSET, the offset of the boot line within NVRAM.
04a,19aug92,rdc  added INCLUDE_BASIC_MMU_SUPPORT, INCLUDE_FULL_MMU_SUPPORT,
		 INCLUDE_PROTECT_TEXT, INCLUDE_PROTECT_VEC_TABLE in 
		 excluded facilities list.
03z,08aug92,ajm  added INCLUDE_LNSGI
03y,01aug92,srh  added INCLUDE_CPLUS
03x,30jul92,pme  added shared memory objects support.
03w,30jul92,elh  added INCLUDE_OLDBP
03v,30jul92,jmm  changed INCLUDE_TFTP to INCLUDE_TFTP_CLIENT
                 added INCLUDE_TFTP_SERVER and INCLUDE_POSIX_TIMERS
03u,30jul92,kdl  added defines of INCLUDE_HW_FP, INCLUDE_DELETE_5_0; 
		 removed INCLUDE_DBX.
03t,28jul92,rdc  changed PAGE_SIZE to VM_PAGE_SIZE.
03s,27jul92,wmd  use INCLUDE_COFF as default for i960.
03r,24jul92,elh  changed BP macros to SM.
03q,21jul92,jwt  fixed STACK_SAVE and RESERVED for SPARC boot boot ROMs.
03p,21jul92,jmm  added INCLUDE_LOADER and INCLUDE_UNLOADER
03o,08Jul92,wmd  added INCLUDE_COFF for I960.
03n,08jul92,rdc  added mmu support.
03m,04jul92,jcf  added scalable configuration.
03l,24jun92,rrr  changed 960 from INCLUDE_COFF to INCLUDE_BOUT
03k,09jun92,ajm  5.0.5 mips merge, added INCLUDE_ECOFF, INCLUDE_AOUT, 
		  and INCLUDE_COFF
03j,02jun92,elh  the tree shuffle
03i,17may92,elh  deleted INCLUDE_STARTUP_SCRIPT_402, INCLUDE_NFS_MOUNT_402,
		 added boot parameter defaults.
03h,24apr92,yao  changed if def of STACK_GROWS_DOWN to _STACK_DIR.
03g,22apr92,jwt  cleaned up OFFSETs for BP_ANCHOR_, BOOT_LINE_, and EXC_MSG_.
03f,13apr92,wmd  added def of STACK_ADRS for STACK_GROWS_UP, moved defines
		 of STACK_SAVE and RESERVED from bootInit.c to this file.
03e,04apr92,elh  added INCLUDE_BOOTP and INCLUDE_TFTP.
03d,11oct91,shl  added INCLUDE_{DATA,INSTRUCTION}_CACHE.
03c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
03b,02oct91,jwt  modified SPARC portions slightly based on 5.0.2 SPARC Beta-2.
03a,02oct91,ajm  changed default ENP int level to 3 for mips
02z,01oct91,yao  moved SLIP_TTY after INCLUDE_SLIP.
02w,19sep91,ajm  added INCLUDE_FP_EMULATION from 5.02 tape
		 added mips architecture support
02v,13sep91,jpb  inserted default IO_AM_EX and IO_AM_ENP defines.
02u,14aug91,yao  added INCLUDE_SLIP, SLIP_TTY to include slip.
02t,09aug91,hdn  added include of HD648132 for G200 floating point co-processor.
	   +jdi  added SCSI but excluded rest of this file for documentation.
02s,07jun91,hdn  changed comments for TRON.
02r,01jun91,jpb  changed BOOT_LINE_SIZE to 255
02q,21may91,jwt  added macros and defines for SPARC architecture.
02p,19may91,gae  defined BOOT_LINE_SIZE here.
02o,17may91,gae  fixed macros and defines for 960 (+SPARC) architecture.
02n,15may91,gae  made INCLUDE_RDB excluded by default.
02m,14may91,hdn  added macros and defines for TRON architecture.
02l,14may91,elh  added INCLUDE_FTPD for ftp server.
02k,03may91,nfs  added ifdef for documentation
02j,28apr91,del  moved INT_VEC_EI and INT_LVL_EI to ../hkv960/config.h.
02i,24mar91,del  added I960 defines.
02h,21apr91,shl  added definition of STACK_ADRS for resident ROM.
*/

/*
DESCRIPTION
This header contains the parameters that define the default
configuration for VxWorks.
*/

#ifndef	INCconfigAllh
#define	INCconfigAllh

#include "smlib.h"
#include "vme.h"
#include "iv.h"

/******************************************************************************/
/*                                                                            */
/*                      INCLUDED SOFTWARE FACILITIES                          */
/*                                                                            */
/******************************************************************************/

#ifndef PRODUCTION
#define INCLUDE_PING		/* ping() utility */
#define INCLUDE_RPC		/* rpc package */
#define INCLUDE_DEBUG           /* native debugging */
#define INCLUDE_WDB             /* tornado debug agent */
#define INCLUDE_LOADER		/* object module loading */
#define INCLUDE_NET_SHOW        /* network info and status facilities */
#define INCLUDE_NET_SYM_TBL     /* load symbol table from network */
#define INCLUDE_SHELL		/* interactive c-expression interpreter */
#define INCLUDE_TELNET		/* telnet-style remote login */
#endif  /* !PRODUCTION */

#define INCLUDE_SHOW_ROUTINES	/* show routines for system facilities*/

#define INCLUDE_CACHE_SUPPORT	/* include cache support package */
#define INCLUDE_CONSTANT_RDY_Q	/* constant insert time ready queue */
#define INCLUDE_EXC_HANDLING	/* include basic exception handling */
#define INCLUDE_EXC_TASK	/* miscelaneous support task */
#define INCLUDE_FLOATING_POINT	/* floating point I/O */
#define INCLUDE_FORMATTED_IO	/* formatted I/O */
#define INCLUDE_IO_SYSTEM	/* include I/O system */
#define INCLUDE_LOGGING		/* logTask logging facility */
#define INCLUDE_MEM_MGR_FULL	/* full featured memory manager */
#define INCLUDE_MSG_Q		/* include message queues */
#define INCLUDE_NETWORK         /* network subsystem code */
#define INCLUDE_PIPES		/* pipe driver */
#define INCLUDE_SELECT		/* select() facility */
#define INCLUDE_SEM_BINARY	/* include binary semaphores */
#define INCLUDE_SEM_MUTEX	/* include mutex semaphores */
#define INCLUDE_SEM_COUNTING	/* include counting semaphores */
#define INCLUDE_SIGNALS		/* software signal library */
#define INCLUDE_STDIO		/* standard I/O */
#define INCLUDE_TASK_HOOKS	/* include kernel callouts */
#define INCLUDE_TASK_VARS	/* task variable package */
#define INCLUDE_TTY_DEV		/* attach serial drivers */
#define INCLUDE_WATCHDOGS	/* include watchdogs */


#ifdef  FFSC_STANDALONE

#ifndef NETBOOT
#define BOOTABLE		/* Bootable image */
#endif  /* NETBOOT */

#ifndef STANDALONE		/* (May have been defined on cmd line) */
#define STANDALONE		/* Standalone image w/o network init */
#endif	/* !STANDALONE */

#ifndef PRODUCTION
#define INCLUDE_UNLOADER	/* object module unloading */
#endif  /* !PRODUCTION */

#define INCLUDE_SYM_TBL 	/* symbol table package */
#define INCLUDE_STANDALONE_SYM_TBL /* compiled-in symbol table */

#undef  INCLUDE_NET_SYM_TBL

#else   /* !FFSC_STANDALONE */

#define INCLUDE_NET_SYM_TBL     /* load symbol table from network */

#undef  INCLUDE_STANDALONE_SYM_TBL

#endif  /* !FFSC_STANDALONE */


#undef  INCLUDE_ANSI_ALL       /* includes complete ANSI C library functions */

#ifdef  INCLUDE_ANSI_ALL
#define INCLUDE_ANSI_ASSERT	/* ANSI-C assert library functionality */
#define INCLUDE_ANSI_CTYPE	/* ANSI-C ctype library functionality */
#define INCLUDE_ANSI_LOCALE	/* ANSI-C locale library functionality */
#define INCLUDE_ANSI_MATH	/* ANSI-C math library functionality */
#define INCLUDE_ANSI_STDIO	/* ANSI-C stdio library functionality */
#define INCLUDE_ANSI_STDLIB	/* ANSI-C stdlib library functionality */
#define INCLUDE_ANSI_STRING	/* ANSI-C string library functionality */
#define INCLUDE_ANSI_TIME	/* ANSI-C time library functionality */

#else	/* !INCLUDE_ANSI_ALL */

#undef  INCLUDE_ANSI_ASSERT	/* ANSI-C assert library functionality */
#define INCLUDE_ANSI_CTYPE	/* ANSI-C ctype library functionality */
#undef  INCLUDE_ANSI_LOCALE	/* ANSI-C locale library functionality */
#undef  INCLUDE_ANSI_MATH	/* ANSI-C math library functionality */
#define INCLUDE_ANSI_STDIO	/* ANSI-C stdio library functionality */
#define INCLUDE_ANSI_STDLIB	/* ANSI-C stdlib library functionality */
#define INCLUDE_ANSI_STRING	/* ANSI-C string library functionality */
#define INCLUDE_ANSI_TIME	/* ANSI-C time library functionality */

#endif


/* CPU-SPECIFIC INCLUDED SOFTWARE FACILITIES */

/* include support for possibly existing floating point coprocessor */

#if	(CPU==MC68020 || CPU==MC68040 || CPU==MC68060 || CPU==CPU32)
#define INCLUDE_MC68881         /* MC68881/2 (68040) floating pt coprocessor */
#define INCLUDE_HW_FP		/* potential hardware fp support */
#endif	/* CPU==MC68020 || CPU==MC68040 || CPU==MC68060 || CPU==CPU32 */

#if	(CPU_FAMILY == SPARC) && defined(INCLUDE_FLOATING_POINT)
#define INCLUDE_SPARC_FPU       /* SPARC Floating-Point Unit */
#define INCLUDE_HW_FP		/* potential hardware fp support */
#endif	/* CPU_FAMILY == SPARC */

#if	(CPU_FAMILY==MIPS)
#define INCLUDE_R3010           /* R3010 float point co-processor */
#define INCLUDE_HW_FP		/* potential hardware fp support */
#endif	/* CPU_FAMILY==MIPS */

#if	(CPU==I960KB)
#define INCLUDE_HW_FP		/* potential hardware fp support */
#endif  /* I960KB */

#if     (CPU_FAMILY==I80X86)
#define INCLUDE_I80387          /* I80387 float point co-processor */
#define INCLUDE_HW_FP           /* potential hardware fp support */
#endif  /* CPU_FAMILY==I80X86 */


/* define appropriate object module format for a given architecture */

#if	(CPU_FAMILY==MIPS)
#define INCLUDE_ELF             /* MIPS ELF object modules */
#else
#if	((CPU_FAMILY==I960) || (CPU_FAMILY==AM29XXX))
#define INCLUDE_COFF            /* COFF object modules */
#else
#if	(CPU_FAMILY==SIMHPPA)
#define	INCLUDE_SOM_COFF
#else	/* default */
#define INCLUDE_AOUT            /* a.out object modules */
#endif
#endif
#endif

/* INCLUDED HARDWARE SUPPORT */

#if CPU_FAMILY==MIPS
#define INCLUDE_EGL             /* include Interphase Ethernet interface */
#else
#define INCLUDE_EX		/* include Excelan Ethernet interface */
#endif	/* CPU_FAMILY==MIPS */

#define INCLUDE_ENP		/* include CMC Ethernet interface*/
#define INCLUDE_SM_NET		/* include backplane net interface */
#define INCLUDE_SM_SEQ_ADDR     /* shared memory network auto address setup */

/******************************************************************************/
/*                                                                            */
/*                          EXCLUDED FACILITIES                               */
/*                                                                            */
/******************************************************************************/


#if FALSE

/* Rob's exclusions */
#define INCLUDE_NFS             /* nfs package */
#define INCLUDE_NFS_SERVER      /* nfs server */
#define INCLUDE_TFTP_SERVER	/* tftp server */
#define INCLUDE_TFTP_CLIENT	/* tftp client */
#define INCLUDE_PROXY_CLIENT	/* proxy arp client (Slave Board) */

#undef  INCLUDE_RDB             /* remote debugging package */
#undef	INCLUDE_TCP_DEBUG	/* TCP debug facility */
#undef  INCLUDE_RLOGIN          /* remote login */
#undef  INCLUDE_SPY		/* spyLib for task monitoring */
#undef  INCLUDE_TIMEX		/* timexLib for exec timing */
#undef  INCLUDE_ENV_VARS	/* unix compatable environment variables */
#undef  INCLUDE_FTP_SERVER	/* ftp server */
#undef  INCLUDE_GCC_FP		/* gcc floating point support libraries */
#undef  INCLUDE_NET_INIT        /* network subsystem initialization */


#ifdef	INCLUDE_CONFIGURATION_5_2

#define INCLUDE_STARTUP_SCRIPT	/* execute start-up script */
#define INCLUDE_STAT_SYM_TBL	/* create user-readable error status */
#define INCLUDE_SYM_TBL 	/* symbol table package */
#define INCLUDE_TELNET		/* telnet-style remote login */
#define INCLUDE_UNLOADER	/* object module unloading */


#endif	/* INCLUDE_CONFIGURATION_5_2 */

/* Temporary */
#define INCLUDE_BOOTP		/* bootp */
#define INCLUDE_NFS_MOUNT_ALL	/* automatically mount all NFS file systems */


#define INCLUDE_ADA		/* include Ada support */
#define INCLUDE_CPLUS		/* include C++ support */
#define INCLUDE_CPLUS_MIN	/* include minimal C++ support */
#define INCLUDE_CPLUS_IOSTREAMS	/* include iostreams classes */
#define INCLUDE_CPLUS_VXW	/* include VxWorks wrapper classes */
#define INCLUDE_CPLUS_TOOLS	/* include Tools class library */
#define INCLUDE_CPLUS_HEAP	/* include Heap class library */
#define INCLUDE_CPLUS_BOOCH	/* include Booch Components library */
#define INCLUDE_DEMO		/* include simple demo instead of shell */
#define INCLUDE_DOSFS           /* dosFs file system */
#define INCLUDE_INSTRUMENTATION /* windView instrumentation */
#define INCLUDE_MIB_VXWORKS	/* SNMP VxWorks Demo MIB */ 
#define INCLUDE_MIB_CUSTOM	/* custom MIB support */ 
#define INCLUDE_MIB2_ALL        /* All of MIB 2 */
#define INCLUDE_MIB2_SYSTEM	/* the system group */
#define INCLUDE_MIB2_TCP        /* the TCP group */
#define INCLUDE_MIB2_ICMP	/* the ICMP group */
#define INCLUDE_MIB2_UDP        /* the UDP group */
#define INCLUDE_MIB2_IF		/* the interfaces group */
#define INCLUDE_MIB2_AT         /* the AT group */
#define INCLUDE_MIB2_IP		/* the IP group */

/* INCLUDE_MMU_BASIC is defined by many bsp's in config.h. If INCLUDE_MMU_FULL
 * is defined in configAll.h and INCLUDE_MMU_BASIC is defined in config.h,
 * then INCLUDE_MMU_FULL will take precedence. 
 */

#define INCLUDE_MMU_BASIC 	/* bundled mmu support */
#define INCLUDE_MMU_FULL	/* unbundled mmu support */

#undef  INCLUDE_POSIX_ALL       /* include all available POSIX functions */
#define INCLUDE_PROTECT_TEXT	/* text segment write protection (unbundled) */
#define INCLUDE_PROTECT_VEC_TABLE /* vector table write protection (unbundled)*/
#define INCLUDE_PROXY_DEFAULT_ADDR /* Use ethernet addr to generate bp addrs */
#define INCLUDE_PROXY_SERVER	/* proxy arp server (Master Board) */
#define INCLUDE_RAMDRV          /* ram disk driver */
#define INCLUDE_RAWFS           /* rawFs file system */
#define INCLUDE_RT11FS		/* rt11Fs file system */
#define INCLUDE_SECURITY	/* shell security for network access */
#define INCLUDE_SLIP		/* include serial line interface */
#define INCLUDE_SM_OBJ          /* shared memory objects (unbundled) */
#define INCLUDE_SNMPD 	        /* SNMP Agent */
#define INCLUDE_SNMPD_DEBUG	/* SNMP Agent debugging */
#define INCLUDE_STANDALONE_SYM_TBL /* compiled-in symbol table */
#define INCLUDE_SW_FP		/* software floating point emulation */
#define INCLUDE_WINDVIEW	/* WindView command server */
#define	INCLUDE_ZBUF_SOCK	/* zbuf socket interface */

#define INCLUDE_LN		/* include AMD LANCE interface */
#define INCLUDE_LNSGI		/* include AMD LANCE interface for SGI VIP10 */
#define INCLUDE_MED		/* include Matrix network interface*/
#define INCLUDE_NIC		/* include National NIC interface */

#define INCLUDE_ANSI_5_0	/* include only version 5.0 ANSI support */
#define INCLUDE_BP_5_0		/* version 5.0 backplane driver */
#define INCLUDE_DELETE_5_0	/* define delete() function as in VxWorks 5.0 */

#endif	/* FALSE */

/******************************************************************************/
/*                                                                            */
/*                  KERNEL SOFTWARE CONFIGURATION                             */
/*                                                                            */
/******************************************************************************/

#define USER_I_CACHE_MODE	CACHE_WRITETHROUGH  /* default mode */
#define USER_D_CACHE_MODE	CACHE_WRITETHROUGH  /* default mode */
#define USER_I_CACHE_ENABLE		    	    /* undef to leave disabled*/
#define USER_D_CACHE_ENABLE			    /* undef to leave disabled*/
#define USER_B_CACHE_ENABLE			    /* undef to leave disabled*/

#define SYM_TBL_HASH_SIZE_LOG2	8	/* 256 entry hash table symbol table */
#define STAT_TBL_HASH_SIZE_LOG2	6	/* 64 entry hash table for status */
#define MQ_HASH_SIZE		0	/* POSIX message queue hash table size 
					 * 0 = default */
#define NUM_SIGNAL_QUEUES	16	/* POSIX queued signal count */

#define FREE_RAM_ADRS		(end)	/* start right after bss of VxWorks */


/* I/O system parameters */

#define NUM_DRIVERS		20	/* max 20 drivers in drvTable */
#define NUM_FILES		50	/* max 50 files open simultaneously */
#define NUM_DOSFS_FILES		20	/* max 20 dosFs files open */
#define NUM_RAWFS_FILES		5	/* max 5  rawFs files open */
#define NUM_RT11FS_FILES	5	/* max 5  rt11Fs files open */
#define MAX_LOG_MSGS		100      /* max 100 log msgs */

/* These are only used by bootconfig.c */
#define	NUM_TTY			6	/* number of tty channels */
#define	CONSOLE_TTY		0	/* shell (etc.) I/O to COM6 */
#define CONSOLE_BAUD_RATE	9600  	/* console baud rate */

#ifdef	INCLUDE_SLIP
#define	SLIP_TTY		1	/* serial line IP channel */
#endif	/* INCLUDE_SLIP */

#define MAX_LIO_CALLS		0	/* max outstanding lio calls 0=default */
#define MAX_AIO_SYS_TASKS	0	/* max aio system tasks, 0 = default */
#define AIO_TASK_PRIORITY	0	/* aio system tasks prior, 0 = default */
#define AIO_TASK_STACK_SIZE	0	/* aio tasks stack size, 0 = default */


/* kernel and system task parameters by architecture */

#if	CPU_FAMILY==MC680X0
#define INT_LOCK_LEVEL  	0x7	/* 68k interrupt disable mask */
#define ROOT_STACK_SIZE		10000	/* size of root's stack, in bytes */
#define SHELL_STACK_SIZE	10000	/* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x1000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE		1000	/* size of ISR stack, in bytes */
#define TRAP_DEBUG		2	/* trap 2 - breakpoint trap */
#define VEC_BASE_ADRS           ((char *) LOCAL_MEM_LOCAL_ADRS)
#endif	/* CPU_FAMILY==MC680X0 */

#if	CPU_FAMILY==SPARC
#define INT_LOCK_LEVEL  	15	/* SPARC interrupt disable level */
#define ROOT_STACK_SIZE		10000	/* size of root's stack, in bytes */
#define SHELL_STACK_SIZE	50000	/* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x2000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE		10000	/* size of ISR stack, in bytes */
#define VEC_BASE                (LOCAL_MEM_LOCAL_ADRS + 0x1000)
#define VEC_BASE_ADRS           ((char *) VEC_BASE)
#endif	/* CPU_FAMILY==SPARC */

#if     CPU_FAMILY==SIMSPARCSUNOS  || CPU_FAMILY==SIMHPPA
#define INT_LOCK_LEVEL          0x1     /* interrupt disable mask */
#define ROOT_STACK_SIZE         20000   /* size of root's stack, in bytes */
#define SHELL_STACK_SIZE        50000   /* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x2000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE          50000   /* size of ISR stack, in bytes */
#define VEC_BASE_ADRS           0       /* dummy */
#endif  /* CPU_FAMILY==SIMSPARCSUNOS  || CPU_FAMILY==SIMHPPA */

#if	CPU_FAMILY==I960
#define INT_LOCK_LEVEL  	0x1f	/* i960 interrupt disable mask */
#define ROOT_STACK_SIZE		20000	/* size of root's stack, in bytes */
#define SHELL_STACK_SIZE	40000	/* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x2000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE		1000	/* size of ISR stack, in bytes */
#define TRAP_DEBUG		0	/* n/a for the 80960 */
#define VEC_BASE_ADRS           NONE	/* base register not reconfigurable */
#endif	/* CPU_FAMILY==I960 */

#if	CPU_FAMILY==MIPS
#define INT_LOCK_LEVEL          0x1     /* R3K interrupt disable mask */
#define ROOT_STACK_SIZE         (20000) /* size of root's stack, in bytes */
#define SHELL_STACK_SIZE        (20000) /* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	(0x2000)/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE          (5000)  /* size of ISR stack, in bytes */
#define VEC_BASE_ADRS           ((char *) 0x0)  /* meaningless in R3k land */
#define VME_VECTORED            FALSE   /* use vectored VME interrupts */
#define TRAP_DEBUG              0       /* trap 0 - breakpoint trap */
#endif	/* CPU_FAMILY==MIPS */

#if     CPU_FAMILY==I80X86
#define INT_LOCK_LEVEL          0x0     /* 80x86 interrupt disable mask */
#define ROOT_STACK_SIZE         10000   /* size of root's stack, in bytes */
#define SHELL_STACK_SIZE        10000   /* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x1000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE          1000    /* size of ISR stack, in bytes */
#define TRAP_DEBUG              0       /* not used */
#define VEC_BASE_ADRS           ((char *) LOCAL_MEM_LOCAL_ADRS)
#endif  /* CPU_FAMILY==I80X86 */

#if	CPU_FAMILY==AM29XXX
#define INT_LOCK_LEVEL  	0x0001	/* 29k all interrupts disable mask */
#define ROOT_STACK_SIZE		10000	/* size of root's stack, in bytes */
#define SHELL_STACK_SIZE	40000	/* size of shell's stack, in bytes */
#define WDB_STACK_SIZE	 	0x2000	/* size of WDB agents stack, in bytes */
#define ISR_STACK_SIZE		10000	/* size of ISR stack, in bytes */
#define TRAP_DEBUG		15	/* trap 2 - breakpoint trap */
#define VEC_BASE_ADRS           ((char *) LOCAL_MEM_LOCAL_ADRS)
#endif	/* CPU_FAMILY==AM29XXX */

/* WDB debug agent configuration */

#ifdef  INCLUDE_WDB

/* optional agent facilities */

#define INCLUDE_WDB_BANNER		/* print banner after agent starts */
#define INCLUDE_WDB_VIO			/* virtual I/O support */
#define	INCLUDE_WDB_TTY_TEST		/* test serial line communcation */

/* core agent facilities - do not remove */

#define INCLUDE_WDB_CTXT		/* context control */
#define INCLUDE_WDB_FUNC_CALL		/* spawn function as separate task */
#define INCLUDE_WDB_DIRECT_CALL		/* call function in agents context */
#define INCLUDE_WDB_EVENTS		/* host async event notification */
#define INCLUDE_WDB_GOPHER		/* gopher info gathering */
#define INCLUDE_WDB_BP			/* breakpoint support */
#define INCLUDE_WDB_EXC_NOTIFY		/* notify host of exceptions */
#define INCLUDE_WDB_EXIT_NOTIFY		/* notify the host of task exit */
#define INCLUDE_WDB_MEM			/* optional memory services */
#define INCLUDE_WDB_REG			/* get/set hardware registers */

/* agent mode */

#define WDB_MODE        WDB_MODE_DUAL	/* WDB_MODE_[DUAL|TASK|EXTERN] */

/* agent communication paths */

#define WDB_COMM_NETWORK 	0	/* vxWorks network	- task mode */
#define WDB_COMM_SERIAL		1	/* raw serial		- bimodal   */
#define WDB_COMM_TYCODRV_5_2	2	/* older serial driver	- task mode */
#define WDB_COMM_ULIP		3	/* vxSim packet device	- bimodal   */
#define WDB_COMM_NETROM		4	/* netrom packet device	- bimodal   */
#define WDB_COMM_CUSTOM		5	/* custom packet device	- bimodal   */

/* alternative names for the communication paths */

#define WDB_COMM_UDP_SOCK 	WDB_COMM_NETWORK
#define WDB_COMM_UDPL_SLIP	WDB_COMM_SERIAL
#define WDB_COMM_UDPL_TTY_DEV	WDB_COMM_TYCODRV_5_2
#define WDB_COMM_UDPL_ULIP	WDB_COMM_ULIP
#define WDB_COMM_UDPL_NETROM	WDB_COMM_NETROM
#define WDB_COMM_UDPL_CUSTOM	WDB_COMM_CUSTOM

#define WDB_COMM_TYPE   WDB_COMM_NETWORK /* default path is the network */
/* #define WDB_COMM_TYPE   WDB_COMM_SERIAL */

/* communication path configuration */

#define WDB_TTY_CHANNEL	 1		/* default SERIAL channel */
#define WDB_TTY_DEV_NAME "/tyCo/1"	/* default TYCODRV_5_2 device name */
#define WDB_TTY_BAUD     9600		/* default baud rate */
#define WDB_ULIP_DEV	 "/dev/ulip14"	/* default ULIP packet device */

#define	WDB_NETROM_WIDTH	1	/* NETROM width of a ROM word */
#define WDB_NETROM_INDEX	0	/* index into word of pod zero */
#define	WDB_NETROM_NUM_ACCESS	1	/* of pod zero per byte read */
#define WDB_NETROM_POLL_DELAY	2	/* default NETROM poll delay */

/* miscelaneous agent constants */

#define WDB_MTU         	1500	/* max RPC message size */
#define WDB_POOL_SIZE 		LOCAL_MEM_SIZE/16 /* host-controlled memory */
#define WDB_SPAWN_STACK_SIZE	0x5000	/* default stack size of spawned task */

#endif  /* INCLUDE_WDB */

/* Posix facilities */

#ifdef  INCLUDE_POSIX_ALL
#define INCLUDE_POSIX_AIO        /* POSIX async I/O support */
#define INCLUDE_POSIX_AIO_SYSDRV /* POSIX async I/O system driver */
#define INCLUDE_POSIX_FTRUNC	 /* POSIX ftruncate routine */
#define INCLUDE_POSIX_MEM	 /* POSIX memory locking */
#define INCLUDE_POSIX_MQ         /* POSIX message queue support */
#define INCLUDE_POSIX_SCHED	 /* POSIX scheduling */
#define INCLUDE_POSIX_SEM	 /* POSIX semaphores */
#define INCLUDE_POSIX_SIGNALS	 /* POSIX queued signals */
#define INCLUDE_POSIX_TIMERS	 /* POSIX timers */

#else	/* !INCLUDE POSIX ALL */

#undef  INCLUDE_POSIX_AIO        /* POSIX async I/O support */
#undef  INCLUDE_POSIX_AIO_SYSDRV /* POSIX async I/O system driver */
#undef  INCLUDE_POSIX_FTRUNC	 /* POSIX ftruncate routine */
#undef  INCLUDE_POSIX_MEM	 /* POSIX memory locking */
#undef  INCLUDE_POSIX_MQ         /* POSIX message queue support */
#undef  INCLUDE_POSIX_SCHED	 /* POSIX scheduling */
#define INCLUDE_POSIX_SEM	 /* POSIX semaphores */
#define INCLUDE_POSIX_SIGNALS	 /* POSIX queued signals */
#define INCLUDE_POSIX_TIMERS	 /* POSIX timers */

#endif /* INCLUDE_POSIX_ALL */

/* WindView event task and buffer parameters */

#define EVT_STACK_SIZE		2000
#define WV_EVT_STACK		EVT_STACK_SIZE
#define EVT_PRIORITY		0
#define WV_EVT_PRIORITY		EVT_PRIORITY
#define EVTBUFFER_SIZE		20000
#define EVTBUFFER_ADDRESS	(char *)NULL

/* WindView command server task parameters */

#define WV_SERVER_STACK		10000
#define WV_SERVER_PRIORITY	100

/* WindView event task */


/* WindView event collection mode 
 *
 * The WindView collection mode can be one of the following,
 *	CONTINUOUS_MODE		- continuous collection and display of events 
 *	POST_MORTEM_MODE	- collection of events only
 */

#define WV_MODE			CONTINUOUS_MODE

/******************************************************************************/
/*                                                                            */
/*                   "GENERIC" BOARD CONFIGURATION                            */
/*                                                                            */
/******************************************************************************/

/* device controller I/O addresses when included */

#define IO_ADRS_EI      ((char *) 0x000fff00)   /* 32A,32D i82596CA Ethernet */
#define IO_ADRS_EX	((char *) 0x00ff0000)	/* 24A,32D Excelan Ethernet */
#define IO_ADRS_ENP	((char *) 0x00de0000)	/* 24A,32D CMC Ethernet */
#define IO_ADRS_EGL	((char *) 0x00004000)	/* 16A,16D Interphase Enet */

#define IO_AM_EX	VME_AM_STD_SUP_DATA	/* Excelan address modifier */
#define IO_AM_EX_MASTER	VME_AM_STD_SUP_DATA	/* Excellan AM for DMA access */
#define IO_AM_ENP	VME_AM_STD_SUP_DATA	/* CMC address modifier */

/* device controller interrupt vectors when included */

#define INT_VEC_ENP		192	/* CMC Ethernet controller*/
#define INT_VEC_EX		193	/* Excelan Ethernet controller*/
#define INT_VEC_EGL		200	/* Interphase Ethernet controller*/

/* device controller interrupt levels when included */

#define INT_LVL_EGL		5	/* Interphase Ethernet controller */
#define INT_LVL_EX		2	/* Excelan Ethernet controller */
#define INT_LVL_ENP		3	/* CMC Ethernet controller */


/******************************************************************************/
/*                                                                            */
/*                   "MISCELLANEOUS" CONSTANTS                                */
/*                                                                            */
/******************************************************************************/

/* shared memory objects parameters (unbundled) */

#define SM_OBJ_MAX_TASK		40	/* max # of tasks using smObj */
#define SM_OBJ_MAX_SEM		30	/* max # of shared semaphores */
#define SM_OBJ_MAX_MSG_Q	10	/* max # of shared message queues */
#define SM_OBJ_MAX_MEM_PART	4	/* max # of shared memory partitions */
#define SM_OBJ_MAX_NAME		100	/* max # of shared objects names */
#define SM_OBJ_MAX_TRIES	100	/* max # of tries to obtain lock */

/* shared memory network parameters  - defaults to values DEFAULT_PKTS_SIZE
 * and DEFAULT_CPUS_MAX in smPktLib.h respectively
 */

#define SM_PKTS_SIZE            0       /* shared memory packet size */
#define SM_CPUS_MAX             0       /* max # of cpus for shared network */

/* low memory layout */

#if     (CPU_FAMILY == I80X86)
#define GDT_BASE_OFFSET         0x800
#define SM_ANCHOR_OFFSET        0x1100
#define BOOT_LINE_OFFSET        0x1200
#define EXC_MSG_OFFSET          0x1300
#else
#define SM_ANCHOR_OFFSET        0x600
#define BOOT_LINE_OFFSET        0x700
#define EXC_MSG_OFFSET          0x800
#endif  /* (CPU_FAMILY == I80X86) */

/* The backplane driver onboard anchor at the following address */

#define SM_ANCHOR_ADRS	((char *) (LOCAL_MEM_LOCAL_ADRS+SM_ANCHOR_OFFSET))


/* The bootroms put the boot line at the following address */

#define BOOT_LINE_ADRS	((char *) (LOCAL_MEM_LOCAL_ADRS+BOOT_LINE_OFFSET))
#define	BOOT_LINE_SIZE	255	/* use 255 bytes for bootline */

/* The boot line is stored in non-volatile RAM at the following offset */

#define	NV_BOOT_OFFSET	0	/* store the boot line at start of NVRAM */


/* Messages from exceptions during exceptions go at the following address */

#define EXC_MSG_ADRS	((char *) (LOCAL_MEM_LOCAL_ADRS+EXC_MSG_OFFSET))


/* Backplane H/W support */

#define	SM_TAS_TYPE	SM_TAS_HARD	/* hardware supports test-and-set */


/* Resident ROMs constants */

#if     (CPU_FAMILY==I960)
#define STACK_SAVE      512     	/* maximum stack used to preserve */
#else	/* sparc or others */
#if	(CPU_FAMILY==SPARC)
#define	STACK_SAVE	0x1000
#else	/* all other architecutes */
#define STACK_SAVE      0x40    	/* maximum stack used to preserve */
#endif					/* mips cpp no elif */
#endif

#if     (CPU_FAMILY==SPARC)
#define RESERVED        0x2000		/* vector table base plus table size */
#else	/* 68000 or others */
#if	(CPU==MC68000)
#define RESERVED	0x400		/* avoid zeroing MC68302 vector table */
#else	/* all other architectures */
#define RESERVED        0
#endif					/* mips cpp no elif */
#endif

#if     (CPU_FAMILY==MIPS)
#define	STACK_RESIDENT	RAM_DST_ADRS
#else
#define	STACK_RESIDENT	_sdata
#endif

#if	(_STACK_DIR == _STACK_GROWS_DOWN)

#ifdef	ROM_RESIDENT
#define STACK_ADRS	STACK_RESIDENT
#else
#define STACK_ADRS	_romInit
#endif	/* ROM_RESIDENT */

#else	/* _STACK_DIR == _STACK_GROWS_UP */

#ifdef	ROM_RESIDENT
#define STACK_ADRS	(STACK_RESIDENT-STACK_SAVE)
#else
#define STACK_ADRS	(_romInit-STACK_SAVE)
#endif	/*  ROM_RESIDENT */

#endif	/* _STACK_DIR == _STACK_GROWS_UP */


/* Default Boot Parameters */

#define HOST_NAME_DEFAULT	"bootHost"	/* host name */
#define TARGET_NAME_DEFAULT	"vxTarget"	/* target name (tn) */
#define HOST_USER_DEFAULT	"target"	/* user (u) */
#define HOST_PASSWORD_DEFAULT	""		/* password */
#define SCRIPT_DEFAULT		""	 	/* startup script (s) */
#define OTHER_DEFAULT		"" 		/* other (o) */

/* Default NFS parameters - constants may be changed here, variables
 * may be changed in usrConfig.c at the point where NFS is included.
 */

#define NFS_USER_ID		2001		/* dummy nfs user id */
#define NFS_GROUP_ID		100		/* dummy nfs user group id */


/* Login security initial user name and password.
 * Use vxencrypt on host to find encrypted password.
 * Default password provided here is "password".
 */

#ifdef	INCLUDE_SECURITY
#define LOGIN_USER_NAME		"target"
#define LOGIN_PASSWORD		"bReb99RRed"	/* "password" */
#endif	/* INCLUDE_SECURITY */


/* install environment variable task create/delete hooks */

#ifdef  INCLUDE_ENV_VARS
#define	ENV_VAR_USE_HOOKS	TRUE
#endif	/* INCLUDE_ENV_VARS */

/* default page size for MMU is 8k.  68040 will also work with 4k page size */

#define VM_PAGE_SIZE		8192

/* SNMP configuration parameters */

/* The following constant specifies the location of the agent 
 * configuration files.
 */

#define SNMPD_CONFIG_DIR 		"/usr/vw/config/snmp/agt"

/* MIB-2 Variable defaults - see RFC 1213 for complete description */
 
#define MIB2_SYS_DESCR                  "VxWorks SNMPv2 Agent"
#define MIB2_SYS_CONTACT                "Wind River Systems"
#define MIB2_SYS_LOCATION               "Planet Earth"

/* MIB2_SYS__OBJID_LEN is the number of elements in the object id
 * MIB_2_SYS_OBJID is the object id.  The default is "0.0" which
 * has the length of 2
 */
 
#define MIB2_SYS_OBJID_LEN              2
#define MIB2_SYS_OBJID                  { 0, 0 }

#ifdef  INCLUDE_MIB2_ALL
#define INCLUDE_MIB2_SYSTEM      /* the system group */
#define INCLUDE_MIB2_TCP         /* the TCP group */
#define INCLUDE_MIB2_ICMP        /* the ICMP group */
#define INCLUDE_MIB2_UDP         /* the UDP group */
#define INCLUDE_MIB2_IF          /* the interfaces group */
#define INCLUDE_MIB2_AT          /* the AT group */
#define INCLUDE_MIB2_IP          /* the IP group */
#endif /* INCLUDE_MIB2_ALL */

/* for backward compatibility with old 1.0 BSPs */

#ifndef BSP_VERSION
#   define BSP_VERSION	"1.0"	/* old 1.0 style BSP */
#   define BSP_VER_1_0	TRUE
#endif

#ifndef BSP_REV
#   define BSP_REV	"/0"	/* old 1.0 style BSP */
#endif

#endif	/* INCconfigAllh */
