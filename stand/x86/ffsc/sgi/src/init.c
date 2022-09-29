/*
 * init.c
 *	FFSC-specific initialization for VxWorks
 */

#include <vxworks.h>
#include "config.h"
#include "usrconfig.h"

#include <iolib.h>
#include <loglib.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netlib.h>
#include <pinglib.h>
#include <pipedrv.h>
#include <stdio.h>
#include <string.h>
#include <tasklib.h>
#include <time.h>
#include <usrlib.h>

#include "ffsc.h"

#include "addrd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "initffsc.h"
#include "log.h"
#include "msg.h"
#include "net.h"
#include "nvram.h"
#include "remote.h"
#include "route.h"
#include "switches.h"
#include "timeout.h"
#include "tty.h"
#include "user.h"



/* Tuneable ("environment") variable information. Note that a tuneable */
/* variable should never have a valid value of "0" - that value means  */
/* "uninitialized".						       */
const tuneinfo_t ffscDefaultTuneInfo[] = {
	{ "BUF_READ",		TUNE_BUF_READ,	      100000,	  TIF_SECRET },
	{ "BUF_WRITE",		TUNE_BUF_WRITE,	      10000000,    TIF_SECRET },

	{ "CI_ROUTER_MSG",	TUNE_CI_ROUTER_MSG,   100000,	  TIF_SECRET },
	{ "CI_ROUTER_RESP",     TUNE_CI_ROUTER_RESP,  20000000,   TIF_SECRET },
	{ "CI_ROUTER_WAIT",	TUNE_CI_ROUTER_WAIT,  2000000000, TIF_SECRET },
	{ "CO_PROGRESS",	TUNE_CO_PROGRESS,     100000,	  TIF_SECRET },
	{ "CO_ROUTER",		TUNE_CO_ROUTER,	      100000,	  TIF_SECRET },
	{ "CW_WAKEUP",		TUNE_CW_WAKEUP,	      2000000,	  TIF_SECRET },

	{ "LO_ROUTER",		TUNE_LO_ROUTER,	      100000,	  TIF_SECRET },

	{ "RI_CONNECT",		TUNE_RI_CONNECT,      3000000,	  TIF_SECRET },
	{ "RI_ELSC_MSG",	TUNE_RI_ELSC_MSG,     1000000,	  TIF_SECRET },
	{ "RI_ELSC_RESP",       TUNE_RI_ELSC_RESP,    5000000,    TIF_SECRET },
	{ "RI_LISTENER",	TUNE_RI_LISTENER,     100000,	  TIF_SECRET },
	{ "RI_REQ",		TUNE_RI_REQ,	      1000000,	  TIF_SECRET },
	{ "RI_RESP",		TUNE_RI_RESP,	      1000000,	  TIF_SECRET },
	{ "RO_CONS",		TUNE_RO_CONS,	      1000000,	  TIF_SECRET },
	{ "RO_ELSC",		TUNE_RO_ELSC,	      1000000,	  TIF_SECRET },
	{ "RO_REMOTE",		TUNE_RO_REMOTE,	      3000000,	  TIF_SECRET },
	{ "RO_RESP",		TUNE_RO_RESP,	      1000000,	  TIF_SECRET },
	{ "RW_RESP_LOCAL",      TUNE_RW_RESP_LOCAL,   4500000,    TIF_SECRET },
	{ "RW_RESP_REMOTE",     TUNE_RW_RESP_REMOTE,  16000000,   TIF_SECRET },

	{ "CONNECT_TIMEOUT",	TUNE_CONNECT_TIMEOUT, 3000000,	  TIF_SECRET },
	{ "LISTEN_PORT",	TUNE_LISTEN_PORT,     5678,	  TIF_SECRET },
	{ "LISTEN_MAX_CONN",	TUNE_LISTEN_MAX_CONN, MAX_RACKS+2,  TIF_SECRET },
	{ "NUM_SWITCH_SAMPLES", TUNE_NUM_SWITCH_SAMPLES, 100,	  TIF_SECRET },
	{ "CONS_MODE_CHANGE",	TUNE_CONS_MODE_CHANGE,   100000,  TIF_SECRET },
	{ "OOBMSG_WRITE",	TUNE_OOBMSG_WRITE,       500000,  TIF_SECRET },

	{ "RI_DISPLAY",		TUNE_RI_DISPLAY,         100000,  TIF_SECRET },
	{ "RO_DISPLAY",		TUNE_RO_DISPLAY,         500000,  TIF_SECRET },

	{ "PROGRESS_INTERVAL",	TUNE_PROGRESS_INTERVAL,	 1024,	  0 },
	{ "XFER_MAX_ERROR",	TUNE_XFER_MAX_ERROR,	 10,	  0 },
	{ "XFER_MAX_TMOUT",	TUNE_XFER_MAX_TMOUT,	 10,	  0 },
	{ "DEBOUNCE_DELAY",	TUNE_DEBOUNCE_DELAY,     10000,  0 },

	{ "LOG_DFLT_NUMLINES",	TUNE_LOG_DFLT_NUMLINES,	 200,	  0 },
	{ "LOG_DFLT_LINELEN",	TUNE_LOG_DFLT_LINELEN,	 80,	  0 },
	{ "PAGE_DFLT_LINES",	TUNE_PAGE_DFLT_LINES,	 23,	  0 },
	{ "PWR_DELAY",		TUNE_PWR_DELAY,		 5000000, 0 },

	{ "CLEAR_FLASH_LOOPS",	TUNE_CLEAR_FLASH_LOOPS,	 10000,	  TIF_SECRET },
	{ "CLEAR_FLASH_RETRY",  TUNE_CLEAR_FLASH_RETRY,	 1,	  TIF_SECRET },

	{ NULL, 0, 0 }
};

tuneinfo_t ffscTuneInfo[NUM_TUNE];


/* Global file descriptors */
int ffscPortFDs[NUM_PORTS];	/* I/O Port to physical serial TTY mappings */

int C2RReqFDs[NUM_CONSOLES];	/* Console->Router request pipes */
int C2RAckFDs[NUM_CONSOLES];	/* Console->Router acknowledge pipes */
int R2CReqFDs[NUM_CONSOLES];	/* Router->Console request pipes */
int D2RReqFD;			/* Display->Router request pipe */
int DispReqFD;			/* Router->Display request pipe */
int L2RMsgFD;			/* Listen->Router message pipe */


/* Other global variables */
macaddr_t	ffscMACAddr;		/* MAC address of this FFSC's NIC */
netinfo_t	ffscNetInfo;		/* Network parameters */
struct ifnet	*ffscNetIF;		/* ifnet for this FFSC's NIC */

int		ffscIsMaster;		/* Non-zero => this is master FFSC */
identity_t	ffscLocal;		/* Identifying info for this FFSC */
identity_t	*ffscMaster;		/* Identity info for master FFSC */

timestamp_t	ffscStartTime;		/* Timestamp at FFSC initialization */

int	  ffscTune[TUNE_MAX];		/* Tuneable values */

char ffscBayName[MAX_BAYS] = { 'U', 'L' };  /* Single char names for bays */

int  ffscInitSwitches;			/* Initial switch settings */


/* Internal functions */
static int	initCreatePipe(char *, int, int);
static STATUS	initNetEther(void);
static STATUS	initNetIP(void);
static STATUS	initNetVxWorks(void);
static STATUS	initPipes(void);


#ifdef PERFORMANCE
/* Performance measurement stuff here */

void idleSpin();

#endif /* PERFORMANCE */



/*
 * ffscEarlyInit
 *	FFSC-specific VxWorks initialization that needs to be done
 *	fairly early in the boot process. This is primarily for
 *	setting up NVRAM, consoles and debugging stuff; most other
 *	things should be initialized in ffscInit.
 */
STATUS
ffscEarlyInit(void)
{
	char devName[20];
	int  ix;

	/* Initialize the VxWorks I/O subsystem */
	iosInit(NUM_DRIVERS, NUM_FILES+10, "/null");

	/* Initialize the display device (even if one isn't actually	*/
	/* attached to this particular FFSC). We don't bother checking  */
	/* for errors because there is nowhere to report them(!).	*/
	(void) pcConDrv();
	for (ix = 0;  ix < N_VIRTUAL_CONSOLES;  ix++) {
		sprintf(devName, "/pcConsole/%d", ix);
		(void) pcConDevCreate(devName, ix, 512, 512);
		if (ix == PC_CONSOLE) {
			DISPLAYfd = open(devName, O_RDWR, 0);
			(void) ioctl(DISPLAYfd, FIOSETOPTIONS, OPT_TERMINAL);
		}
	}

	/* Assuming we were able to set up the console, redirect output */
	/* there for the time being.					*/
	if (DISPLAYfd >= 0) {
		ioGlobalStdSet(STD_IN,  DISPLAYfd);
		ioGlobalStdSet(STD_OUT, DISPLAYfd);
		ioGlobalStdSet(STD_ERR, DISPLAYfd);
	}

	/* Sample the switches on the display panel - they may have */
	/* some special message (e.g. "zap NVRAM") from the user.   */
	ffscInitSwitches = sampleswitches(BOOT_SWITCH_SAMPLES);

#ifndef SECURE	/* Potentially insecure - make it easy to disable */
	/* If the user has asked us to zap NVRAM, happily oblige */
	if (ffscInitSwitches == SWITCH_ZAP_NVRAM) {

		/* Print an initial warning */
		fprintf(stderr, "Clearing MMSC NVRAM....\n");

		/* Go do the dirty work */
		if (nvramZap() == OK) {
			fprintf(stderr,
				"All MMSC NVRAM settings have been cleared\n");
		}
		else {
			fprintf(stderr, "WARNING: Unable to clear NVRAM\n");
		}

		/* Busily wait until the user is ready to go on */
		fprintf(stderr, "\nPress the ENTER key to continue\n");
		while (sampleswitches(BOOT_SWITCH_SAMPLES) != SWITCH_ENTER) {
			/* NOP */;
		}
		fprintf(stderr, "\033[H\033[2J");
	}
#endif

	/* Initialize NVRAM services */
	if (nvramInit() != OK) {
		fprintf(stderr, "NVRAM initialization failed, resetting...\n");
		if (nvramReset() != OK  ||  nvramInit() != OK) {
			fprintf(stderr,
				"Unable to initialize NVRAM services\n");
		}
	}
	else if (ffscDebug.Flags & FDBF_INIT) {
		fprintf(stderr, "Initialized NVRAM services\n");
	}

	/* Initialize the default values for tuneable variables */
	for (ix = 0;  ffscDefaultTuneInfo[ix].Name != NULL;  ++ix) {
		int Index;

		Index = ffscDefaultTuneInfo[ix].Index;
		ffscTuneInfo[Index] = ffscDefaultTuneInfo[ix];
	}

	/* Initialize the tuneable variables */
	if (nvramRead(NVRAM_TUNE, ffscTune) != OK) {

		/* The tuneables are not available in NVRAM, so build */
		/* a fresh batch from the default values.	      */
		bzero((char *) ffscTune, TUNE_MAX * sizeof(int));
		for (ix = 0;  ix < NUM_TUNE;  ++ix) {
			ffscTune[ix] = ffscTuneInfo[ix].Default;
		}

		/* Try to write the debugging info out (but ignore */
		/* any errors that may occur)			   */
		nvramWrite(NVRAM_TUNE, ffscTune);
	}
	else {
		/* If there are any variables with a value of 0, they	*/
		/* may be uninitialized (meaning this is the first time */
		/* we have run since a new variable was defined). Set   */
		/* up a default value now. Notice that this implies	*/
		/* that you can never set a tuneable variable to "0"	*/
		/* unless that happens to be the normal default value.	*/
		for (ix = 0;  ix < NUM_TUNE;  ++ix) {
			if (ffscTune[ix] == 0) {
				ffscTune[ix] = ffscTuneInfo[ix].Default;
			}
		}
	}

	/* Go set up the TTYs */
	if (ttyInit() != OK) {
		fprintf(stderr, "Unable to initialize TTYs\n");
	}
	else if (ffscDebug.Flags & FDBF_INIT) {
		fprintf(stderr, "Initialized TTYs\n");
	}

	/* Initialize SGI logging */
	if (logInitSGI() != OK) {
		fprintf(stderr, "Logging initialization failed!\n");
	}

	/* Initialize debugging services */
	if (ffscDebugInit() != OK) {
		fprintf(stderr, "Debugging initialization failed!\n");
	}

	/* At this point, it is safe to use ffscMsg & friends */

	/* Switch stdin/out/err over to the official target shell TTY */
	/* (if one exists)					      */
#if 0
	/*
	 * @@: TODO: fix this so that when debug is non-zero, you use that
	 * port value.
	 */
	if (portDebug >= 0) {
	  ioGlobalStdSet(STD_IN,  portDebug);
	  ioGlobalStdSet(STD_OUT, portDebug);
	  ioGlobalStdSet(STD_ERR, portDebug);
	}
#else
	if (portAltCons >= 0) {
	  ioGlobalStdSet(STD_IN,  portAltCons);
	  ioGlobalStdSet(STD_OUT, portAltCons);
	  ioGlobalStdSet(STD_ERR, portAltCons);
	}
#endif
	/* Initialize the VxWorks logging task */
	logInit(portAltCons, MAX_LOG_MSGS);


	/* Init Partition Information */
	part_init();

	return OK;
}



/*
 * ffscInit
 *	FFSC-specific VxWorks initialization. By the time this is
 *	called, most kernel-level services should be available.
 */
STATUS
ffscInit(void)
{
	int NoNetwork = 0;

	/* Note the current time. We expect this to be "0" usually. */
	if (clock_gettime(CLOCK_REALTIME, &ffscStartTime) != OK) {
		ffscMsg("Unable to set start time");
	}

	/* Initialize timeout timer for this task */
	if (timeoutInit() != OK) {
		ffscMsg("Warning! init task has no timeout timers");
	}

	/* Internal network initialization required by VxWorks */
	if (netLibInit() != OK) {
		ffscMsg("Unable to initialize netLib");
		NoNetwork = 1;
	}
	else if (ffscDebug.Flags & FDBF_INIT) {
		ffscMsg("Initialized netLib");
	}

	/* Initialize the ethernet interface */
	if (!NoNetwork) {
		if (initNetEther() != OK) {
			ffscMsg("Ethernet initialization failed!");
			NoNetwork = 1;
		}
		else if (ffscDebug.Flags & FDBF_INIT) {
			ffscMsg("Initialized ethernet");
		}
	}

	/* Initialize identity services */
	if (identInit() != OK) {
		ffscMsg("Unable to initialize identity services");
	}
	else if (ffscDebug.Flags & FDBF_INIT) {
		ffscMsg("Initialized identity services");
	}

	/* Initialize TCP/IP networking */
	if (!NoNetwork) {
		if (initNetIP() != OK) {
			ffscMsg("Unable to initialize TCP/IP");
			NoNetwork = 1;
		}
		else if (ffscDebug.Flags & FDBF_INIT) {
			ffscMsg("Initialized TCP/IP");
		}
	}

	/* Initialize other VxWorks' network services */
	if (!NoNetwork) {
		if (initNetVxWorks() != OK) {
			ffscMsg("Unable to initialize VxWorks network "
				    "services");
		}
		else if (ffscDebug.Flags & FDBF_INIT) {
			ffscMsg("Initialized miscellaneous network services");
		}
	}

	/* Set up the address daemon */
	if (!NoNetwork) {
		if (addrdInit() != OK) {
			ffscMsg("Unable to initialize address daemon");
			NoNetwork = 1;
		}
		else if (ffscDebug.Flags & FDBF_INIT) {
			ffscMsg("Initialized address daemon");
		}
	}

	/* Elect a master FFSC */
	if (!NoNetwork) {
		if (addrdElectMaster() != OK) {
			ffscMsg("Unable to elect master MMSC");
			NoNetwork = 1;
		}
		else if (ffscDebug.Flags & FDBF_INIT) {
			if (ffscIsMaster) {
				ffscMsg("This machine has been elected "
					    "Master MMSC");
			}
			else {
				ffscMsg("Another machine is the Master MMSC");
			}
		}
	}

	/* Initialize the various intertask communication pipes */
	if (initPipes() != OK) {
		ffscMsg("Unable to initialize intertask communication pipes");
	}

	/* Prepare to listen for remote requests */
	if (remInit() != OK) {
		ffscMsg("Unable to listen for remote requests");
	}

	/* Initialize ELSC info */
	if (elscInit() != OK) {
		ffscMsg("Unable to initialize MSC data");
	}

	/* Initialize the various consoles and related tasks */
	if (consInit() != OK) {
		ffscMsg("Unable to initialize consoles");
	}

	/* Initialize router users and their message buffers */
	if (userInit() != OK) {
		ffscMsg("Unable to initialize router users");
	}


	/* Spawn the FFSC router task */
	if (taskSpawn("tRouter", ROUTER_PRI, 0, 20000, router,
		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	{
		ffscMsg("Unable to spawn main MMSC task");
	}

	/* Spawn the switch/graphics handler tasks */
	if (taskSpawn("tDisplay", 75, 0, 20000, waitReq,
		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	{
		ffscMsg("Error spawning switch handler");
	}

	if (taskSpawn("tSwDelay", 75, 0, 20000, swdelay,
		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	{
		ffscMsg("Error spawning switch delay task");
	}
	if (taskSpawn("tGui", 125, 0, 20000, (FUNCPTR) guimain,
		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	{
		ffscMsg("Error spawning gui task");
	}






#ifdef PERFORMANCE
	/* Performance measuring stuff here */

	/* spawn the idle task.  The priority should really 
	   match the gui task if this this is to run at all. 
	   */

	if (taskSpawn("tIdleSpin", 125, 0, 20000, idleSpin,
		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	  { 
	    ffscMsg("Error spawning idleSpin task");
	  } 

	/* This enables round-robin scheduling for tasks of the
	   same priority 
	   */

	if (kernelTimeSlice(10) == ERROR)
	  {
	    ffscMsg("Error enabling round-robin scheduling");
	  }


#endif /* PERFORMANCE */


#ifndef PRODUCTION

	/*  Partitioning debug stuff:
	 * Hardcode some partition strings and simulate IP27prom 
	 * spitting out some DSP messages.  A nice little debug feature.

	 * Unfortunatelly it loses big-time when rackid > 2 :)
	 */
	{
#if 0
	  /* Multi-rack system: partition per module */
	  char* part_inf[] = {"P 1 M 1 C",  /* R1 */
			      "P 2 M 2 C",  /* R1 */
			      "P 3 M 3 C",  /* R2 */
			      "P 4 M 4 C"}; /* R2 */
#else /* 0 */
	  /* Multi-rack system: one partition ... */
	  char* part_inf[] = {"P 0 M 1 C",  /* R1 */
			      "P 0 M 2 C",  /* R1 */
			      "P 0 M 3 C",  /* R2 */
			      "P 0 M 4 C"}; /* R2 */

#endif /* 0 */

#if 0
	  int i,j = (ffscLocal.rackid -1) * ffscLocal.rackid;


	  ffscMsg("About to call update_partition_info");
	  for(i = 0; i < 2; i++)
	    update_partition_info(&partInfo, part_inf[i+j]);	  

#endif /*  0 */
	}
#endif /* PRODUCTION */

	/* We have finished all we set out to do */
	
	return OK;
}



/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * initCreatePipe
 *	Create and open a pipe
 */
int
initCreatePipe(char *PipeName, int NumMsgs, int MsgLen)
{
	int FD;

	if (pipeDevCreate(PipeName, NumMsgs, MsgLen) != OK) {
		ffscMsg("Unable to create %s", PipeName);
		FD = -1;
	}
	else {
		FD = open(PipeName, O_RDWR, 0);
		if (FD < 0) {
			ffscMsg("Unable to open %s", PipeName);
		}
	}

	return FD;
}


/*
 * initNetEther
 *	Initialize the ethernet interface itself. Bringing up the network
 *	to the point where it can run IP is done later.
 */
STATUS
initNetEther(void)
{
	struct arpcom *ArpCom;

	/* Attach the ethernet device */
	if (usrNetIfAttach(FFSC_NET_DEVICE, NULL) != OK) {
		ffscMsg("Unable to attach ethernet device");
		return ERROR;
	}

	/* Get gory details about the ethernet device */
	ffscNetIF = ifunit(FFSC_NET_INTERFACE);
	if (ffscNetIF == NULL) {
		ffscMsgS("Unable to find ethernet interface "
			     FFSC_NET_INTERFACE);
		return ERROR;
	}

	/* Extract the MAC address of the ethernet */
	ArpCom = (struct arpcom *) ffscNetIF;
	bcopy((char *) ArpCom->ac_enaddr,
	      (char *) &ffscMACAddr,
	      sizeof(ffscMACAddr));

	return OK;
}


/*
 * initNetIP
 *	Initialize TCP/IP networking
 */
STATUS
initNetIP(void)
{
	char IPAddrStr[INET_ADDR_LEN];

	/* Set up the netmask */
	if (ifMaskSet(FFSC_NET_INTERFACE, ffscNetInfo.NetMask) != OK) {
		ffscMsgS("Unable to set netmask on " FFSC_NET_INTERFACE
			     "to 0x%08x",
			 ffscNetInfo.NetMask);
		return ERROR;
	}

	/* Set up the IP address. Strangely, it has to be in the form	*/
	/* of a string.							*/
	inet_ntoa_b(ffscLocal.ipaddr, IPAddrStr);
	if (ifAddrSet(FFSC_NET_INTERFACE, IPAddrStr) != OK) {
		ffscMsgS("Unable to set IP address on " FFSC_NET_INTERFACE
			     " to %s",
			 IPAddrStr);
		return ERROR;
	}
	else if (ffscDebug.Flags & FDBF_INIT) {
		ffscMsg("IP address initialized to %s", IPAddrStr);
	}

	return OK;
}


/*
 * initNetVxWorks
 *	Network initialization functions taken from the VxWorks
 *	usrNetInit function in usrNetwork.c, which is not invoked
 *	for the FFSC. It's not obvious what all of this is for, or
 *	even if all of it is completely necessary.
 */
STATUS
initNetVxWorks(void)
{
#ifndef PRODUCTION
	BOOT_PARAMS	params;
	char	devName [MAX_FILENAME_LENGTH];
	int	protocol;
#endif

	/* Initialize the host table */
	hostTblInit();

	/* add loop-back interface */
	usrNetIfAttach ("lo", "127.0.0.1");
	usrNetIfConfig ("lo", "127.0.0.1", "localhost", 0);

#ifndef PRODUCTION

	/* interpret boot command */
	if (usrBootLineCrack(BOOT_LINE_ADRS, &params) != OK) {
		ffscMsg("Unable to crack boot line");
		return ERROR;
	}

	/* fill in any default values specified in configAll */
	if ((params.hostName [0] == EOS) &&		/* host name */
	    (strcmp (HOST_NAME_DEFAULT, "") != 0))
	{
		strncpy (params.hostName, HOST_NAME_DEFAULT, BOOT_HOST_LEN);
		printf ("Host Name: %s \n", params.hostName);
	}

	if ((params.targetName [0] == EOS) &&	/* targetname */
	    (strcmp (TARGET_NAME_DEFAULT, "") != 0))
	{
		strncpy (params.targetName,
			 TARGET_NAME_DEFAULT,
			 BOOT_HOST_LEN);
		printf ("Target Name: %s \n", params.targetName);
	}

	if ((params.usr [0] == EOS) &&		/* user name (u) */
	    (strcmp (HOST_USER_DEFAULT, "") != 0))
	{
		strncpy (params.usr, HOST_USER_DEFAULT, BOOT_USR_LEN);
		printf ("User: %s \n", params.usr);
	}

	if ((params.startupScript [0] == EOS) && /* startup script (s) */
	    (strcmp (SCRIPT_DEFAULT, "") != 0))
	{
		strncpy (params.startupScript, SCRIPT_DEFAULT, BOOT_FILE_LEN);
		printf ("StartUp Script: %s \n", params.startupScript);
	}

	if ((params.other [0] == EOS) &&	/* other (o) */
	    (strcmp (OTHER_DEFAULT, "") != 0))
	{
		strncpy (params.other, OTHER_DEFAULT, BOOT_OTHER_LEN);
		printf ("Other: %s \n", params.other);
	}

	if (params.passwd [0] == EOS)		/* password */
	    strncpy (params.passwd, HOST_PASSWORD_DEFAULT, BOOT_PASSWORD_LEN);

	/* fill in system-wide variables */
	bcopy ((char *) &params,
	       (char *) &sysBootParams,
	       sizeof (sysBootParams));

	sysFlags = params.flags;
	strcpy (sysBootHost, params.hostName);	/* backwards compatibility */
	strcpy (sysBootFile, params.bootFile);	/* backwards compatibility */

	/* associate host name with the specified host address */
	hostAdd (params.hostName, params.had);

	/*
	 * create transparent remote file access device;
	 * device name is host name with ':' appended.
	 * protocol is rcmd if no password, or ftp if password specified
	 */
	netDrv ();				/* init remote file driver */
	sprintf (devName, "%s:", params.hostName);	/* make dev name */
	protocol = (params.passwd[0] == EOS) ? 0 : 1;	/* pick protocol */
	netDevCreate (devName, params.hostName, protocol); /* create device */

	/* set the user id, and current directory */
	iam (params.usr, params.passwd);
	ioDefPathSet (devName);

	taskDelay (sysClkRateGet () / 4);		/* 1/4 of a second */

	/* Pull in the symbol table if necessary */
#ifdef  INCLUDE_NET_SYM_TBL
	sysSymTbl = symTblCreate(SYM_TBL_HASH_SIZE_LOG2, TRUE, memSysPartId);
	netLoadSymTbl();			/* fill in table from host */
#endif	/* INCLUDE_NET_SYM_TBL */
#endif  /* !PRODUCTION */

	/* Miscellaneous drivers */

#ifdef	INCLUDE_TELNET
	telnetInit ();
#endif	/* INCLUDE_TELNET */

#ifdef	INCLUDE_RPC
	rpcInit ();
#endif	/* INCLUDE_RPC */

#ifdef  INCLUDE_PING
	(void) pingLibInit ();	/* initialize the ping utility */
#endif  /* INCLUDE_PING */

	/* initialize select only after NFS and RPC for proper delete	*/
	/* hook order							*/
	selectInit ();

	return OK;
}


/*
 * initPipes
 *	Initialize the pipes that are used for communication between
 *	the various FFSC tasks
 */
STATUS
initPipes(void)
{
	char PipeName[80];
	int i;

	/* Console <-> Router pipes */
	for (i = 0;  i < NUM_CONSOLES;  ++i) {

		sprintf(PipeName, "/pipe/C%d2RReq", i);
		C2RReqFDs[i] = initCreatePipe(PipeName,
					      1,
					      MAX_FFSC_CMD_LEN);

		sprintf(PipeName, "/pipe/C%d2RAck", i);
		C2RAckFDs[i] = initCreatePipe(PipeName,
					      1,
					      MAX_FFSC_RESP_LEN);

		sprintf(PipeName, "/pipe/R2C%dReq", i);
		R2CReqFDs[i] = initCreatePipe(PipeName,
					      MAX_REQS,
					      MAX_CONS_CMD_LEN);
	}

	/* Display->Router request */
	D2RReqFD = initCreatePipe("/pipe/D2RReq",
				  1,
				  MAX_FFSC_CMD_LEN);

	/* [anything]->Display requests */
	DispReqFD = initCreatePipe("/pipe/DispReq",
				   MAX_REQS,
				   MAX_DISP_CMD_LEN);

	/* Listener->Router messages */
	L2RMsgFD = initCreatePipe("/pipe/L2RMsg",
				  MAX_REQS,
				  MAX_LISTEN_MSG_LEN);

	return OK;
}





#ifdef PERFORMANCE

/* This is an idle task that never blocks, and should be 
   spawned with the lowest priority.  That way its running
   time will be the same as the idle time.  
   */

void idleSpin() {

  while(1);
}


#endif /* PERFORMACE */
