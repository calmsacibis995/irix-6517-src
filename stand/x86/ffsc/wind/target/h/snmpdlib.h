/* snmpdLib.h - VxWorks SNMP Agent */

/* Copyright 1984-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01a,18feb94,elh  written. 
*/

#ifndef __INCsnmpdLibh
#define __INCsnmpdLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vxworks.h"

/* module structure */ 

typedef struct mib_module
    {
    FUNCPTR	mibInitialize;		/* initialization routine */
    FUNCPTR	mibTerminate;		/* termination routine */
    } MIB_MODULE;

#define SNMPD_LOG_APWARN	0x1000		/* log warning messages */
#define SNMPD_LOG_APERROR 	0x2000		/* log error messages */
#define SNMPD_LOG_APALL		0x7FFF		/* log all messages */

/* function declarations */
 
 #if defined(__STDC__) || defined(__cplusplus)

extern STATUS snmpdInit (MIB_MODULE * pModules, int loglevel, int prio, 
	      		 int stackSize, int port);
extern STATUS snmpdDebugInit (void);
extern void   generateTrap (int ifTrapType, int interfaceIndex, 
			    void * pM2TrapRtnArg);
extern void k_mib2_initialize (void);

#else   /* __STDC__ */

extern STATUS snmpdInit ();
extern STATUS snmpdDebugInit ();
extern void generateTrap ();
extern void k_mib2_initialize ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif


#endif /* __INCsnmpdLibh */
