/* usrNetwork.c - network initialization */

/* Copyright 1992-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,28mar95,kkk  changed smNetInit() parameters to use macros from 
		 configAll.h (SPR #4130)
02z,27mar95,jag  removed dependency of INCLUDE_NFS_SERVER from INCLUDE_NFS
01z,19mar95,dvs  removed tron references - no longer supported.
01y,30jan95,hdn  included nfsdLib.h if INCLUDE_NFS_SERVER is defined.
		 added 6th argument to nfsdInit ().
01x,09dec94,jag  Added INCLUDE_MIB2_AT
01w,13nov94,dzb  fixed placement of INCLUDE_PING.
01v,17aug94,dzb  fixed setting the gateway for a slip connection (SPR #2353).
		 added qu netif device.  added zbuf socket interface init.
		 added ping init.  added CSLIP support.  added tcpTrace init.
01u,11nov94,jag  divided SNMP and MIB-II support.
01t,09feb94,hdn  added support for if_elt 3COM Etherlink III driver.
		 added support for if_ene Eagle NE2000 driver.
01s,13nov93,hdn  added support for Thin Ethernet (10Base2, BNC).
		 added support for if_eex Intel EtherExpress ethernet driver.
		 added support for if_ultra SMC Ultra ethernet driver.
		 added support for floppy and IDE device.
		 added support for if_elc SMC ethernet driver.
01r,30jul93,gae  vxsim: used devName & protocol in passfs to eliminate warnings.
01q,15jul93,gae  vxsim: added default route for ulip.
01p,29jan93,gae  vxsim: added ulip i/f.
01o,19apr94,jmm  added INCLUDE_NFS_SERVER to handle init of NFS server
01n,22feb94,elh  added snmp initialization. 
01m,22jul93,jmm  added initialization of _func_netLsByName (spr 2305)
01l,13feb93,kdl  made sysProcNumSet() call depend on _procNumWasSet (SPR #2011).
01k,12feb93,kdl  added hashLibInit() call during proxy arp init (SPR #1919).
01j,12feb93,jag  changed INCLUDE_PROXY_ARP->INCLUDE_PROXY_SERVER, 
		 INCLUDE_SM_NETAUTO->INCLUDE_SM_SEQ_ADDR.  Added support
		 for INCLUDE_PROXY_DEFAULT_ADDR. (SPR #1898)
01i,11feb93,jag  reworked backplane configuration. (SPR #1898)
01h,09feb93,kdl  added include of rdbLib.h (SPR #1987).
01g,28jan93,caf  made usrBootLineInit() check for empty boot line (SPR #1873)
		 and read NVRAM at offset 0 (SPR #1933).
01f,08dec92,caf  added IF_USR, enabling extension of the net interface table.
01e,14nov92,kdl  removed cache disable when using old bp driver, now handled in 
		 usrDepend.c (SPR #1801); fixed compiler warning (SPR #1692).
01d,03oct92,kdl  changed INCLUDE_PROXYARP to INCLUDE_PROXY_ARP.
01c,23sep92,kdl  changed INCLUDE_FTPD to INCLUDE_FTP_SERVER.
01b,20sep92,kdl  added init of _func_ftpLs.
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to configure and initialize the VxWorks networking support.
This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrNetworkc
#define  __INCusrNetworkc

#include "rdblib.h"
#include "zbufsocklib.h"
#include "pinglib.h"

#ifdef INCLUDE_SNMPD
#include "snmpdlib.h"
#endif

#include "m2lib.h"
#include "private/funcbindp.h"

#ifdef INCLUDE_NFS_SERVER
#include "nfsdlib.h"
#endif

/*
 * Network interface table.
 */

IMPORT int eglattach ();
IMPORT int eiattach ();
IMPORT int exattach ();
IMPORT int enpattach ();
IMPORT int ieattach ();
IMPORT int lnattach ();
IMPORT int lnsgiattach ();
IMPORT int nicattach ();
IMPORT int medattach ();
IMPORT int loattach ();
IMPORT int snattach ();
IMPORT int fnattach ();
IMPORT int elcattach ();
IMPORT int ultraattach ();
IMPORT int eexattach ();
IMPORT int eltattach ();
IMPORT int eneattach ();
IMPORT int quattach ();
IMPORT int slattach ();

#ifdef INCLUDE_SNMPD
#ifdef INCLUDE_MIB_VXWORKS
extern int k_vxdemo_initialize ();
extern int k_vxdemo_terminate ();
#endif /* INCLUDE_MIB_VXWORKS */

#ifdef INCLUDE_MIB_CUSTOM
extern int k_initialize ();
extern int k_terminate ();
#endif
#endif

#ifdef  INCLUDE_TCP_DEBUG
IMPORT void tcpTraceInit ();
#endif  /* INCLUDE_TCP_DEBUG */

#ifdef	INCLUDE_IF_USR
IMPORT int IF_USR_ATTACH ();
#endif	/* INCLUDE_IF_USR */

typedef struct	/* NETIF - network interface table */
    {
    char *ifName;	/* interface name */
    FUNCPTR attachRtn;	/* attach routine */
    char *arg1;		/* address        */
    int arg2;		/* vector         */
    int arg3;		/* level          */
    int arg4;
    int arg5;
    int arg6;
    int arg7;
    int arg8;
    } NETIF;

/* local variables */

LOCAL NETIF netIf [] =	/* network interfaces */
    {
#ifdef  INCLUDE_EGL
        { "egl", eglattach, (char*)IO_ADRS_EGL, INT_VEC_EGL, INT_LVL_EGL },
#endif	/* INCLUDE_EGL */
#ifdef	INCLUDE_EI
	{ "ei", eiattach, (char*)INT_VEC_EI, EI_SYSBUS, EI_POOL_ADRS, 0, 0},
#endif	/* INCLUDE_EI */
#ifdef	INCLUDE_EX
	{ "ex", exattach, (char*)IO_ADRS_EX, INT_VEC_EX, INT_LVL_EX,
	  IO_AM_EX_MASTER, IO_AM_EX },
#endif	/* INCLUDE_EX */
#ifdef	INCLUDE_ENP
	{ "enp", enpattach, (char*)IO_ADRS_ENP, INT_VEC_ENP, INT_LVL_ENP,
	  IO_AM_ENP },
#endif	/* INCLUDE_ENP */
#ifdef	INCLUDE_IE
	{ "ie", ieattach, (char*)IO_ADRS_IE, INT_VEC_IE, INT_LVL_IE },
#endif	/* INCLUDE_IE */
#ifdef	INCLUDE_IF_USR
	{ IF_USR_NAME, IF_USR_ATTACH, IF_USR_ARG1, IF_USR_ARG2, IF_USR_ARG3,
	  IF_USR_ARG4, IF_USR_ARG5, IF_USR_ARG6, IF_USR_ARG7, IF_USR_ARG8 },
#endif	/* INCLUDE_IF_USR */
#ifdef	INCLUDE_LN
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
#ifdef  INCLUDE_QU
	{ "qu", quattach, (char*)IO_ADRS_QU_EN, INT_VEC_QU_EN, QU_EN_SCC,
	  QU_EN_TX_BD, QU_EN_RX_BD, QU_EN_TX_OFF, QU_EN_RX_OFF, QU_EN_MEM},
#endif  /* INCLUDE_QU */
#ifdef  INCLUDE_ENE
	{ "ene", eneattach, (char*)IO_ADRS_ENE, INT_VEC_ENE, INT_LVL_ENE},
#endif  /* INCLUDE_ENE */
#ifdef  INCLUDE_SN
        { "sn", snattach, (char*)IO_ADRS_SN, INT_VEC_SN },
#endif	/* INCLUDE_SN */
#ifdef  INCLUDE_FN
        { "fn", fnattach },
#endif	/* INCLUDE_FN */

#ifdef  INCLUDE_SM_NET
#ifdef  INCLUDE_BP_5_0
        { "bp", bpattach, 0, 0, 0, 0, 0, 0 },
#else
        { "sm", smNetAttach, 0, 0, 0, 0, 0, 0 },
#endif	/* INCLUDE_BP_5_0 */
#endif  /* INCLUDE_SM_NET */

#ifdef	INCLUDE_SLIP
	{"sl", 0, 0, 0, 0, 0},
#endif	/* INCLUDE_SLIP */
	{ "lo", loattach  },
	{ 0, 0, 0, 0, 0, 0 },
    };


/* global variables */

MBUF_CONFIG mbufConfig = 
    {
    NUM_INIT_MBUFS, NUM_MBUFS_TO_EXPAND, MAX_MBUFS, NULL, 0 
    };

MBUF_CONFIG clusterConfig =
    {
    NUM_INIT_CLUSTERS, NUM_CLUSTERS_TO_EXPAND, MAX_CLUSTERS, NULL, 0
    };


/*******************************************************************************
*
* usrNetInit - system-dependent network initialization
*
* This routine initializes the network.  The ethernet and backplane drivers
* and the TCP/IP software are configured.  It also adds hosts (analogous to
* the /etc/hosts file in UNIX), gateways (analogous to /etc/gateways), sets
* up our access rights on the host system (with iam()), optionally
* initializes telnet, rlogin, RPC, and NFS support.
*
* The boot string parameter is normally left behind by the boot ROMs,
* at address BOOT_LINE_ADRS.
*
* Unless the STANDALONE option is selected in usrConfig.c, this routine
* will automatically be called by the root task usrRoot().
*
* RETURNS:
* OK, or
* ERROR if there is a problem in the boot string, or initializing network.
*
* SEE ALSO:  "Network" chapter of the VxWorks Programmer's Guide
*
* NOMANUAL
*/

STATUS usrNetInit (bootString)
    char *bootString;		/* boot parameter string */

    {
    BOOT_PARAMS	params;
    char	devName [MAX_FILENAME_LENGTH];	/* device name */
    char	nad [20];			/* host's network inet addr */
    int		protocol;
    char *	pTail;
    int		netmask;
    BOOL	backplaneBoot = FALSE;

    /* interpret boot command */
    if (bootString == NULL)
	bootString = BOOT_LINE_ADRS;

    if (usrBootLineCrack (bootString, &params) != OK)
	return (ERROR);

    /* fill in any default values specified in configAll */

    if ((params.hostName [0] == EOS) &&			/* host name */
        (strcmp (HOST_NAME_DEFAULT, "") != 0))
	{
	strncpy (params.hostName, HOST_NAME_DEFAULT, BOOT_HOST_LEN);
	printf ("Host Name: %s \n", params.hostName);
	}

    if ((params.targetName [0] == EOS) &&		/* targetname */
        (strcmp (TARGET_NAME_DEFAULT, "") != 0))
	{
	strncpy (params.targetName, TARGET_NAME_DEFAULT, BOOT_HOST_LEN);
	printf ("Target Name: %s \n", params.targetName);
	}

    if ((params.usr [0] == EOS) &&			/* user name (u) */
        (strcmp (HOST_USER_DEFAULT, "") != 0))
	{
	strncpy (params.usr, HOST_USER_DEFAULT, BOOT_USR_LEN);
	printf ("User: %s \n", params.usr);
	}

    if ((params.startupScript [0] == EOS) &&		/* startup script (s) */
        (strcmp (SCRIPT_DEFAULT, "") != 0))
	{
	strncpy (params.startupScript, SCRIPT_DEFAULT, BOOT_FILE_LEN);
	printf ("StartUp Script: %s \n", params.startupScript);
	}

    if ((params.other [0] == EOS) &&			/* other (o) */
        (strcmp (OTHER_DEFAULT, "") != 0))
	{
	strncpy (params.other, OTHER_DEFAULT, BOOT_OTHER_LEN);
	printf ("Other: %s \n", params.other);
	}

    if (params.passwd [0] == EOS)			/* password */
	strncpy (params.passwd, HOST_PASSWORD_DEFAULT, BOOT_PASSWORD_LEN);

    /* fill in system-wide variables */

    bcopy ((char *) &params, (char *) &sysBootParams, sizeof (sysBootParams));

    sysFlags = params.flags;
    strcpy (sysBootHost, params.hostName);	/* backwards compatibility */
    strcpy (sysBootFile, params.bootFile);	/* backwards compatibility */

    /* set processor number: may establish vme bus access, etc. */

    if (_procNumWasSet != TRUE)
	{
    	sysProcNumSet (params.procNum);
	_procNumWasSet = TRUE;
	}

    /* start the network */

    hostTblInit ();		/* initialize host table */
    netLibInit ();

    remLastResvPort = 1010;	/* pick an unused port number so we don't *
				 * have to wait for the one used by the *
				 * by the bootroms to time out */

    _func_ftpLs = ftpLs;	       /* init ptr to ftp dir listing routine */
    _func_netLsByName = netLsByName;   /* init ptr to netDrv listing routine */


    /* attach and configure interfaces */
#ifdef INCLUDE_ULIP
    if (strncmp (params.bootDev, "ul", 2) == 0)
	{
	extern int ulipInit ();

	printf ("Attaching network interface %s0... ", params.bootDev);

	bootNetmaskExtract (params.ead, &netmask); /* remove and ignore mask */

	/* XXX last octet of 'ead' == procNum */

	if (ulipInit (0, params.ead, params.had, params.procNum) == ERROR)
	    {
	    if (errno == S_if_ul_NO_UNIX_DEVICE)
		printf ("\nulipInit failed, errno = S_if_ul_NO_UNIX_DEV\n");
	    else
		printf ("\nulipInit failed, errno = 0x%x\n", errno);

	    return (ERROR);
	    }

	printf ("done.\n");
	}
    else
#endif  /* INCLUDE_ULIP */

    if (strncmp (params.bootDev, "sl", 2) == 0)
	{
	/* booting via slip */

	if (usrSlipInit (params.bootDev, params.ead, ((params.gad[0] == EOS)?
		         params.had : params.gad)) == ERROR)
	    return (ERROR);
	}

    else if ((strncmp (params.bootDev, "bp", 2) == 0) ||
            (strncmp (params.bootDev, "sm", 2) == 0))
	{
	/* booting via backplane */

	netmask = 0;
	bootNetmaskExtract (params.bad, &netmask);
	backplaneBoot = TRUE;

	if (usrBpInit (params.bootDev, 0) == ERROR)
	    return (ERROR);

	if ((usrNetIfAttach (params.bootDev, params.bad) !=OK) ||
	    (usrNetIfConfig (params.bootDev, params.bad, params.targetName,
			     netmask) !=OK))
	    return (ERROR);
	}
    else
	{
	netmask = 0;
	bootNetmaskExtract (params.ead, &netmask);
	
        if ((strncmp (params.bootDev, "scsi", 4) == 0) ||
	    (strncmp (params.bootDev, "ide", 3) == 0) ||
	    (strncmp (params.bootDev, "fd", 2) == 0))
	    {
	    /* booting via scsi, configure network if included */

	    if ((usrNetIfAttach (params.other, params.ead) != OK) ||
		(usrNetIfConfig (params.other, params.ead, params.targetName,
			         netmask) != OK))
		{
		return (ERROR);
		}
	    }
	else
	    {
	    /* booting via ethernet */

	    if ((usrNetIfAttach (params.bootDev, params.ead) !=OK) ||
	        (usrNetIfConfig (params.bootDev, params.ead, params.targetName,
	                         netmask) !=OK))
		return (ERROR);
	    }
	}

    /* attach backplane interface (as second interface) */

#ifdef INCLUDE_SM_NET 	
#define BP_ADDR_GIVEN	        (params.bad [0] != EOS)
#define BP_ADDR_NOT_GIVEN	(params.bad [0] == EOS)

    if ( !backplaneBoot )
	{
    	char 		*bpDev;
    	BOOL	        proxyOn = FALSE;       /* Initialize Defaults */
#ifndef INCLUDE_BP_5_0			       /* Vars needed only for SM */
	u_long		startAddr = 0;         /* Default sequential Addr off */
	int		bpNetMask = 0;
	char		*netDev;
    	BOOL	        seqAddrOn = FALSE;
	BOOL		configureBp = FALSE;
	BOOL		useEtherAddr = FALSE;
					       /* Turn switches ON as needed */
#ifdef INCLUDE_SM_SEQ_ADDR
	seqAddrOn = TRUE;
#endif
#ifdef INCLUDE_PROXY_DEFAULT_ADDR
	useEtherAddr = TRUE;
#endif
#endif /* INCLUDE_BP_5_0 */

#ifdef INCLUDE_PROXY_SERVER
	    proxyOn = TRUE;
#endif

#ifdef INCLUDE_BP_5_0
    	if (BP_ADDR_GIVEN)
	    {             /* Configure BP_5.0 with or without PROXY Arp */
	    bpDev = "bp";
	    bootNetmaskExtract (params.bad, &netmask);

			  /* No Sequential Address support for BP_5_0 */
	    if (usrBpInit (bpDev, 0) == ERROR)
	        return (ERROR);

	    (void) usrNetIfAttach (bpDev, params.bad);

	    (void) usrNetIfConfig (bpDev, params.bad, (char*) NULL, netmask);
	    }
	else
	    {
	    if ( proxyOn )
		printf ("Error: bp with PROXY Arp requires a bp address\n");
	    }
#else	                 /* Configure sm driver with or without PROXY Arp */
        bpDev = "sm";
	netDev = "sm0";
	bootNetmaskExtract (params.bad, &bpNetMask);
	if (proxyOn == TRUE)
	    {
	    if (seqAddrOn == TRUE)   /* PROXY WITH SEQ ADDR */
	        {
	        /* Pick address from backplane or ethernet */
		if (BP_ADDR_GIVEN)
	   	    {
	    	     startAddr = ntohl (inet_addr (params.bad));
	    	     netmask = bpNetMask;
		     configureBp = TRUE;
	   	    }
		else
		    {
		    if ( useEtherAddr )
			{
	    	    	startAddr = ntohl (inet_addr (params.ead)) + 1;
		    	configureBp = TRUE;
			}
		    else		/* Configuration error */
		       {
		       printf(
       "Error: PROXY with sequential addr enable, and default addr disable.\n");
		       printf("Backplane IP Address must be specified.\n");
		       }
		    }
			
		}
	    else                     /* PROXY WITHOUT SEQ ADDR */
	        {
		if (BP_ADDR_NOT_GIVEN)
		    {
		    printf ("Error: PROXY with sequential addr disabled.\n");
		    printf ("Shared mem IP Address must be specified.\n");
		    }
		else
		    {   /* startAddr is left as zero */
	    	    netmask = bpNetMask;
	    	    configureBp = TRUE;
		    }
	        }
	    }
	else
	    {		/* Using Subnet without PROXY Arp */
	    if (BP_ADDR_GIVEN)
	        {
	        if (seqAddrOn == TRUE)
	            {    /* Assign sequential address to backplane */
	    	    startAddr = ntohl (inet_addr (params.bad));
	    	    netmask = bpNetMask;
		    }
		/* Else Don't use sequential addr on backplane */

		configureBp = TRUE;
		}
	    }

	if (configureBp == TRUE)
	    {
	    if (usrBpInit (bpDev, startAddr) == ERROR)
	    	return (ERROR);

	    (void) usrNetIfAttach (bpDev, params.bad);

	    /* Assigned Back Plane Address if needed */
	    if ((BP_ADDR_NOT_GIVEN) &&
		 (smNetInetGet (netDev, params.bad, NONE) == OK))
		 printf ("Backplane address: %s\n", params.bad);

	    (void) usrNetIfConfig (bpDev, params.bad, (char *) NULL, netmask);
	    }
#endif /* INCLUDE_BP_5_0 */
	}


#ifdef INCLUDE_PROXY_SERVER
    if ((sysProcNumGet () == 0) && (params.bad [0] != EOS) && 
	(params.ead [0] != EOS))
	{
	inet_netof_string (params.bad, nad);
	routeDelete (nad, params.bad);

	hashLibInit ();			/* make sure hash functions init'd */

	proxyArpLibInit (8, 8);
	printf ("Creating proxy network: %s\n", params.bad);
	if (proxyNetCreate (params.bad, params.ead) == ERROR)
	    {
	    printf ("proxyNetCreate failed:%x\n", errno);
	    return (ERROR);
	    }
	}
#endif /* INCLUDE_PROXY_SERVER */

#ifdef INCLUDE_SHOW_ROUTINES
#ifndef INCLUDE_BP_5_0
    smNetShowInit ();
#endif /* INCLUDE_BP_5_0 */
#endif /* INCLUDE_SHOW_ROUTINES */

#endif /* INCLUDE_SM_NET */

    if (params.targetName[0] != EOS)
        sethostname (params.targetName, strlen (params.targetName));

    /* add loop-back interface */

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

    /* associate host name with the specified host address */

    hostAdd (params.hostName, params.had);

#ifdef	INCLUDE_ULIP
    /* Set up host names and routes for talking to other vxworks's */
    {
    extern char *vxsim_ip_addr;
    extern char *vxsim_ip_name;
    int vxsim_num_ulips = 16;
    int ix;
    char host [50];
    char hostip [50];

    /* declare other simulated vxWorks' */

    for (ix = 0; ix < vxsim_num_ulips; ix++)
	{
	sprintf (host, vxsim_ip_name, ix);
	sprintf (hostip, vxsim_ip_addr, ix);
	hostAdd (host, hostip);
	}

    /* Add default route thru host */
    routeAdd ("0.0.0.0", params.had);
    }
#endif /* INCLUDE_ULIP */


#ifdef	INCLUDE_PASSFS

    /* The device names for passFs and netDrv are the same, e.g. "host:",
     * therefore, we have and either/or for the two devices.
     * Bulk of work done in usrConfig.c:usrRoot() for passFs installation.
     */
    iam (params.usr, params.passwd);
    devName[0] = EOS;
    protocol = (params.passwd[0] == EOS) ? 0 : 1;	/* pick protocol */

#else
    /* create transparent remote file access device;
     * device name is host name with ':' appended.
     * protocol is rcmd if no password, or ftp if password specified
     */

    netDrv ();					/* init remote file driver */
    sprintf (devName, "%s:", params.hostName);		/* make dev name */
    protocol = (params.passwd[0] == EOS) ? 0 : 1;	/* pick protocol */
    netDevCreate (devName, params.hostName, protocol);	/* create device */


    /* set the user id, and current directory */

    iam (params.usr, params.passwd);
    ioDefPathSet (devName);

    taskDelay (sysClkRateGet () / 4);		/* 1/4 of a second */
#endif /* INCLUDE_PASSFS */

#ifdef  INCLUDE_ZBUF_SOCK
    zbufSockLibInit ();                 /* initialize zbuf socket interface */
#endif  /* INCLUDE_ZBUF_SOCK */

#ifdef  INCLUDE_TCP_DEBUG
    tcpTraceInit ();                    /* initialize TCP debug facility */
#endif  /* INCLUDE_TCP_DEBUG */

    /* start the rlogin and telnet daemon */

#ifdef	INCLUDE_RLOGIN
    rlogInit ();
#endif	/* INCLUDE_RLOGIN */

#ifdef	INCLUDE_TELNET
    telnetInit ();
#endif	/* INCLUDE_TELNET */

    /* initialize rpc daemon if specified */

#ifdef	INCLUDE_RPC
    rpcInit ();
#endif	/* INCLUDE_RPC */

#ifdef  INCLUDE_RDB
    rdbInit ();                 /* initialize remote rpc debugging */
#endif	/* INCLUDE_RDB */

#ifdef INCLUDE_FTP_SERVER
    ftpdInit (0);
#endif

#ifdef INCLUDE_TFTP_SERVER
    tftpdInit (0, 0, 0, FALSE, 0);
#endif

    /* initialize NFS server and client if specified */

#ifdef INCLUDE_NFS_SERVER
	if (nfsdInit (0, 0, 0, 0, 0, 0) == ERROR)
	    return (ERROR);
#endif  /* INCLUDE_NFS_SERVER */
	
#ifdef	INCLUDE_NFS
    /*
     * The following values are the default values used in NFS.
     * They can be reset here if necessary.
     *
     *     nfsMaxMsgLen   = 8192	message size (decrease only)
     *     nfsTimeoutSec  = 5		timeout seconds
     *     nfsTimeoutUSec = 0		timeout microseconds
     */

    nfsAuthUnixSet (params.hostName, NFS_USER_ID, NFS_GROUP_ID, 0, (int *) 0);

    if (nfsDrv () == ERROR)	/* initialize nfs driver */
	printf ("Error initializing NFS, errno = %#x\n", errno);
    else
	{
#ifdef	INCLUDE_NFS_MOUNT_ALL
	printf ("Mounting NFS file systems from host %s", params.hostName);
	if (params.targetName[0] != EOS)
	    printf (" for target %s:\n", params.targetName);
	else
	    printf (":\n");

	nfsMountAll (params.hostName, params.targetName, FALSE);
	printf ("...done\n");
#endif	/* INCLUDE_NFS_MOUNT_ALL */

	/* if the boot pathname starts with a device name, e.g. an nfs mount,
	 * then set the current directory to that device
	 */

	(void) iosDevFind (sysBootFile, &pTail);
	if (pTail != sysBootFile)
	    {
	    devName[0] = EOS;
	    strncat (devName, sysBootFile, pTail - sysBootFile);
	    ioDefPathSet (devName);
	    }
	}
#else	/* INCLUDE_NFS */
    printf ("NFS client support not included.\n");
    pTail = NULL;			/* dummy use to quiet compiler warning*/
#endif	/* INCLUDE_NFS */

#ifdef  INCLUDE_PING
    (void) pingLibInit ();              /* initialize the ping utility */
#endif  /* INCLUDE_PING */

#ifdef INCLUDE_SNMPD
    usrSnmpdInit (); 
#endif

    return (OK);
    }


/* the next routines are in common with bootConfig.c and will be merged */

/******************************************************************************
*
* usrBootLineInit - initialize system boot line
*
* Initializes system boot line as per specified start type.  If the boot
* line is empty or this is a COLD boot, i.e. with CLEAR option to clear
* memory, then the boot line is initialized from non-volatile ram, if any,
* otherwise from the compiled in default boot line.
*
* NOMANUAL
*/

void usrBootLineInit (startType)
    int startType;

    {
    if ((startType & BOOT_CLEAR) || (* BOOT_LINE_ADRS == EOS))
	{
	/* either cold boot or empty boot line -- initialize boot line */

	if ((sysNvRamGet (BOOT_LINE_ADRS, BOOT_LINE_SIZE, 0) == ERROR))
	    {
	    /* no non-volatile RAM -- use default boot line */

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
* NOMANUAL
*/

STATUS usrBootLineCrack (bootString, pParams)
    char * bootString;
    BOOT_PARAMS *pParams;

    {
    FAST char *	pS;

    pS = bootStringToStruct (bootString, pParams);
    if (*pS != EOS)
	{
	bootParamsErrorPrint (bootString, pS);
	return (ERROR);
	}

    /* check inet addresses */

    if ((checkInetAddrField (pParams->ead, TRUE) != OK) ||
	(checkInetAddrField (pParams->bad, TRUE) != OK) ||
	(checkInetAddrField (pParams->had, FALSE) != OK) ||
	(checkInetAddrField (pParams->gad, FALSE) != OK))
	{
	return (ERROR);
	}

    return (OK);
    }

/******************************************************************************
*
* checkInetAddrField - check for valid inet address in boot field
*
* RETURNS: OK, or ERROR if invalid inet address
*
* NOMANUAL
*/

STATUS checkInetAddrField (pInetAddr, subnetMaskOK)
    char *pInetAddr;
    BOOL subnetMaskOK;

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
* usrNetIfAttach - attach a  network interface
*
* This routine attaches the specified network interface.
*
*	- interface is attached
*	- interface name is constructed as "<devName>0"
*
* RETURNS: OK or ERROR
*
* NOMANUAL
*/

STATUS usrNetIfAttach (devName, inetAdrs)
    char *devName;
    char *inetAdrs;

    {
    FAST NETIF *	pNif;
    STATUS 		status;

    /* attach interface */

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
        else
	    printf ("failed: errno = %#x.\n", errno);
	return (ERROR);
	}

    printf ("done.\n");

    return (OK);
    }

/******************************************************************************
*
* usrNetIfConfig - configure network interface
*
* This routine configures the specified network interface with the specified
* boot parameters:
*
*	- subnetmask is extracted from inetAdrs and, if present,
*	  set for interface
*	- inet address is set for interface
*	- if present, inet name for interface is added to host table.
*
* RETURNS: OK or ERROR
*
* NOMANUAL
*/

STATUS usrNetIfConfig (devName, inetAdrs, inetName, netmask)
    char *	devName;
    char *	inetAdrs;
    char *	inetName;
    int 	netmask;

    {
    char ifname [20];

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
* usrBpInit - initailize backplane driver
*
* usrBpInit initializes the backplane driver shared memory region
* and sets up the backplane parameters to attach.
*
* RETURNS: OK if successful otherwise ERROR
*
* NOMANUAL
*/

STATUS usrBpInit (devName, startAddr)
    char *		devName;	/* device name */
    u_long		startAddr;	/* inet address */

    {
#ifdef	INCLUDE_SM_NET
    char *		bpAnchor;	/* anchor address */
    FAST NETIF *	pNif;		/* netif struct */
    STATUS 		status;		/* status */
    int			procNum;	/* proc num */

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

     procNum = sysProcNumGet ();

    /* if we are master, initialize backplane net */

    if (procNum == 0)
	{
	printf ("Initializing backplane net with anchor at %#x... ",
		(int) bpAnchor);

#ifdef INCLUDE_BP_5_0
	status = bpInit (bpAnchor, (char *) SM_MEM_ADRS, (int) SM_MEM_SIZE,
			 SM_TAS_TYPE);
#else 
	status = smNetInit ((SM_ANCHOR *) bpAnchor, (char *) SM_MEM_ADRS,
			    (int) SM_MEM_SIZE, SM_TAS_TYPE, SM_CPUS_MAX, 
			    SM_PKTS_SIZE, startAddr);
#endif

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
	{
#ifdef INCLUDE_BP_5_0
	if (strncmp (devName, "sm", 2) == 0)
	    printf ("INCLUDE_BP_5_0 defined - use 'bp' interface\n");
#else /* INCLUDE_BP_5_0 */
        if (strncmp (devName, "bp", 2) == 0)
	    printf ("INCLUDE_BP_5_0 not defined - use 'sm' interface\n");
#endif /* INCLUDE_BP_5_0 */
	return (ERROR);
	}

    printf ("Backplane anchor at %#x... ", (int) bpAnchor);

    /* configure backplane parameters (most set in NETIF struct) */

    pNif->arg1 = bpAnchor;	/* anchor address */
    pNif->arg3 = SM_INT_TYPE;
    pNif->arg4 = SM_INT_ARG1;
    pNif->arg5 = SM_INT_ARG2;
    pNif->arg6 = SM_INT_ARG3;

#ifdef INCLUDE_BP_5_0
    pNif->arg2 = procNum;		/* proc num */
#endif /* INCLUDE_BP_5_0 */
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
*
* NOMANUAL
*/

STATUS usrSlipInit (pBootDev, localAddr, peerAddr)
    char * 	pBootDev;		/* boot device */
    char * 	localAddr;		/* local address */
    char * 	peerAddr;		/* peer address */

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

#ifdef INCLUDE_SNMPD

/* 
 * The following structure defines which MIB modules are initialized
 * by the SNMP agent.  The initialization routines are called 
 * during agent start up.  The termination routines are called 
 * after the agent is deleted.  To add a custom MIB module (for the
 * extensible agent) either use the {k_initialze, k_terminate } routines 
 * defined above and below, or replace them with the custom routines to the
 * structure below and the prototypes above.
 */

MIB_MODULE snmpMibModules []  =

    { 	
	/* initializtion, terminate */

	{ usrMib2Init, usrMib2CleanUp },
	
#ifdef INCLUDE_MIB_CUSTOM
	/*  Link in custom MIBs with the following line, if the
	 *  routine names are appropriate. Or insert the appropriate 
	 *  routine names in this structure. 
	 */
	{ k_initialize, k_terminate },
#endif  				/* INCLUDE_MIB_CUSTOM */

#ifdef INCLUDE_MIB_VXWORKS 
	{ k_vxdemo_initialize, k_vxdemo_terminate },
#endif 					/* INCLUDE_MIB_VXWORKS */

        { NULL, NULL },			/* Last entry both must be NULL */
    };

/*****************************************************************************
*
* usrSnmpdInit - initialize the SNMP agent
* 
* This routine initializes the SNMP agent as well as the MIB-2 libraries.
* In addition, it initializes the environment variable SR_AGT_CONF_DIR.
* SR_AGT_CONF_DIR specifies the location of the agent configuration files. 
*
* RETURNS: OK if successful, otherwise ERROR.
*/

STATUS usrSnmpdInit (void)

    {
    int		debugLevel = 0;

#ifdef INCLUDE_SNMPD_DEBUG
    /* 
     * Enabling this call would drag in the agent debugging strings,
     * and initiate a debugging mode on the agent.
     */
	
    snmpdDebugInit ();
    debugLevel = SNMPD_LOG_APALL;

#endif 	/* INCLUDE_SNMPD_DEBUG */

    /* initialize agent configuration directory */

    if (SNMPD_CONFIG_DIR != NULL)
	{
        char 		envStr [250];		/* string for env vars */

	sprintf (envStr,"SR_AGT_CONF_DIR=%s", SNMPD_CONFIG_DIR);	
	putenv (envStr);
	}

    snmpdInit (snmpMibModules, debugLevel, 0, 0, 0);	/* start snmp agent */

    return (OK);
    }


/*******************************************************************************
*
* usrMib2Init - initialize the MIB-II library 
*
* This routine initializes the MIB-II library interface. 
*
* RETURNS: OK (always)
*/


STATUS usrMib2Init (void)
    {
    static M2_OBJECTID	sysObjectId = { MIB2_SYS_OBJID_LEN,
				        MIB2_SYS_OBJID };

    /* this includes and initializes the MIB-2 interface */

#ifdef INCLUDE_MIB2_SYSTEM
    m2SysInit (MIB2_SYS_DESCR, MIB2_SYS_CONTACT, MIB2_SYS_LOCATION,
	       &sysObjectId);
#endif /* INCLUDE_MIB2_SYSTEM */

#ifdef INCLUDE_MIB2_IF
    m2IfInit ((FUNCPTR) generateTrap, 0);	/* the interfaces group */
#endif /* INCLUDE_MIB2_IF */

#ifdef INCLUDE_MIB2_TCP
    m2TcpInit ();			/* the TCP group */
#endif /* INCLUDE_MIB2_TCP */ 

#ifdef INCLUDE_MIB2_UDP
    m2UdpInit ();			/* the UDP group */
#endif /* INCLUDE_MIB2_UDP */

#ifdef INCLUDE_MIB2_ICMP
    m2IcmpInit ();			/* the ICMP group */
#endif /* INCLUDE_MIB2_ICMP */

#if defined(INCLUDE_MIB2_IP) || defined(INCLUDE_MIB2_AT)
    m2IpInit (0);			/* the IP group */
#endif /* MIB_IP */

    k_mib2_initialize ();		/* initialize agent */ 

    return (OK);
    }

/*******************************************************************************
*
* usrMib2CleanUp - clean up MIB-II state
*
* RETURNS: OK (always)
*/

STATUS usrMib2CleanUp (void)
    {
#ifdef INCLUDE_MIB2_SYSTEM
    m2SysDelete ();			/* system clean up */
#endif /* INCLUDE_MIB2_SYSTEM */

#ifdef INCLUDE_MIB2_IF
    m2IfDelete ();			/* interface clean up */
#endif /* INCLUDE_MIB2_IF */

#if defined(INCLUDE_MIB2_IP) || defined(INCLUDE_MIB2_AT)
    m2IpDelete ();			/* IP clean up */
#endif /* MIB_IP */

    return (OK);
    }

#else					/* Case for MIB-II support only */

#if defined (INCLUDE_MIB2_ALL)
VOIDFUNCPTR _m2files[] =
    {
    (VOIDFUNCPTR) m2Init
    };
#else

#if defined(INCLUDE_MIB2_SYSTEM) || defined(INCLUDE_MIB2_IF)
#define INCLUDE_M2_FUNCS
#endif

#if defined(INCLUDE_MIB2_TCP) || defined(INCLUDE_MIB2_UDP)
#define INCLUDE_M2_FUNCS
#endif

#if defined(INCLUDE_MIB2_ICMP) || defined(INCLUDE_MIB2_IP)
#define INCLUDE_M2_FUNCS
#endif

#if defined(INCLUDE_MIB2_AT)
#define INCLUDE_M2_FUNCS
#endif

#if defined(INCLUDE_M2_FUNCS)
VOIDFUNCPTR _m2files[] =
    {
#if defined(INCLUDE_MIB2_SYSTEM)
    (VOIDFUNCPTR) m2SysInit,
#endif

#if defined(INCLUDE_MIB2_IF)
    (VOIDFUNCPTR) m2IfInit,
#endif

#if defined(INCLUDE_MIB2_TCP)
    (VOIDFUNCPTR) m2TcpInit,
#endif

#if defined(INCLUDE_MIB2_UDP)
    (VOIDFUNCPTR) m2UdpInit,
#endif

#if defined(INCLUDE_MIB2_ICMP)
    (VOIDFUNCPTR) m2IcmpInit,
#endif

#if defined(INCLUDE_MIB2_IP) || defined(INCLUDE_MIB2_AT)
    (VOIDFUNCPTR) m2IpInit,
#endif

    (VOIDFUNCPTR) 0L
    };
#endif /* INCLUDE_M2_FUNCS */
#endif /* INCLUDE_M2_ALL */

#endif /* INCLUDE_SNMPD */

#endif /* __INCusrNetworkc */
