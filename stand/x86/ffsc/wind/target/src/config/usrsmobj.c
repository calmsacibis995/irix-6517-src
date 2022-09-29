/* usrSmObj.c - shared memory object initialization */

/* Copyright 1992-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,30sep93,rrr  added missing 01h modhist.
01h,30sep93,rrr  fixed spr 2460 (set smAnchor to default if boot line is
                 not sm=value).
01g,13feb93,kdl  added call to sysProcNumSet(), in case no network (SPR #2011).
01f,29jan93,pme  added smObjLibInit() return value test.
		 added message printing if mem pool allocation fails SPR #1779.
		 added bad anchor address handling for master CPU.
01e,24dec92,jdi  removed NOMANUAL from usrSmObjInit(), ansified declaration,
		 and cleaned up documentation.
01d,13nov92,jcf  log error message if smObjAttach returns ERROR.
01c,20oct92,pme  made usrSmObjInit return ERROR instead of disabling data cache
		 if cache coherent buffer cannot be allocated.
		 added shared memory probing.
		 adjust pool size if pool not allocated.
01b,29sep92,pme  fixed WARNING in printf call.
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to configure and initialize the VxWorks shared memory
object support.  This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrSmObjc
#define  __INCusrSmObjc

#include "private/funcbindp.h"


/******************************************************************************
*
* usrSmObjInit - initialize shared memory objects
*
* This routine initializes the shared memory objects facility.  It sets up
* the shared memory objects facility if called from processor 0.
* Then it initializes a shared memory descriptor and calls smObjAttach()
* to attach this CPU to the shared memory object facility.
*
* When the shared memory pool resides on the local CPU dual ported memory,
* SM_OBJ_MEM_ADRS must be set to NONE in configAll.h and the shared memory
* objects pool is allocated from the VxWorks system pool.
*
* RETURNS: OK, or ERROR if unsuccessful.
*/

STATUS usrSmObjInit 
    (
    char * bootString		/* boot parameter string */
    )

    {
    char *        smAnchor;             /* anchor address */
    char *        smObjFreeAdrs;        /* free pool address */
    int           smObjMemSize;         /* pool size */
    BOOT_PARAMS   params;               /* boot paramters */
    BOOL          allocatedPool;        /* TRUE if pool is maloced */
    SM_OBJ_PARAMS smObjParams;          /* smObj setup parameters */
    char 	  bb;			/* bit bucket for vxMemProbe */

    allocatedPool = FALSE;

    /* Check for hardware test and set availability */

    if (SM_TAS_TYPE != SM_TAS_HARD)
        {
        printf ("\nError initializing shared memory objects, ");
	printf ("hardware test-and-set required.\n");
        return (ERROR);
        }

    if (smObjLibInit () == ERROR)	/* initialize shared memory objects */
	{
        printf("\nERROR smObjLibInit : shared memory objects already initialized.\n");
	return (ERROR);
	}

    if (bootString == NULL)
        bootString = BOOT_LINE_ADRS;

    /* interpret boot command */

    if (usrBootLineCrack (bootString, &params) != OK)
        return (ERROR);

    /* set processor number: may establish vme bus access, etc. */

    if (_procNumWasSet != TRUE)
	{
    	sysProcNumSet (params.procNum);
	_procNumWasSet = TRUE;
	}

    /* if we booted via the sm device use the same anchor address for smObj */

    smAnchor = (char *) SM_ANCHOR_ADRS;         /* default anchor */

    if (strncmp (params.bootDev, "sm", 2) == 0)
        {
        if (bootBpAnchorExtract (params.bootDev, &smAnchor) < 0)
            {
	    printf ("\nError initializing shared memory objects, invalid ");
            printf ("anchor address specified: \"%s\"\n", params.bootDev);
            return (ERROR);
            }
        }

    /* set up shared memory object if we are shared memory master */

    if (params.procNum == SM_MASTER)
        {
        smObjFreeAdrs = (char *) SM_OBJ_MEM_ADRS;
	smObjMemSize  = SM_OBJ_MEM_SIZE;

        /* allocate the shared memory object pool if needed */

        if (smObjFreeAdrs == (char *) NONE)
            {
            /* check cache configuration - must be read and write coherent */

	    if (!CACHE_DMA_IS_WRITE_COHERENT() || !CACHE_DMA_IS_READ_COHERENT())
                {
		printf ("usrSmObjInit - cache coherent buffer not available. Giving up.  \n");
		return (ERROR);
                }

            allocatedPool = TRUE;

            smObjFreeAdrs = (char *) cacheDmaMalloc (SM_OBJ_MEM_SIZE);

            if (smObjFreeAdrs == NULL)
		{
		printf ("usrSmObjInit - cannot allocate shared memory pool. Giving up.\n");
                return (ERROR);
		}
            }

        if (!allocatedPool)
            {
            /* free memory pool must be behind the anchor */
            smObjFreeAdrs += sizeof (SM_ANCHOR);

	    /* adjust pool size */
	    smObjMemSize = SM_OBJ_MEM_SIZE - sizeof (SM_ANCHOR);
            }

	/* probe anchor address */

	if (vxMemProbe (smAnchor, VX_READ, sizeof (char), &bb) != OK)
	    {
	    printf ("usrSmObjInit - anchor address %#x unreachable. Giving up.\n", (unsigned int) smAnchor);
	    return (ERROR);
	    }

	/* probe beginning of shared memory */

	if (vxMemProbe (smObjFreeAdrs, VX_WRITE, sizeof (char), &bb) != OK)
	    {
	    printf ("usrSmObjInit - shared memory address %#x unreachable. Giving up.\n", (unsigned int) smObjFreeAdrs);
	    return (ERROR);
	    }

        /* set up shared memory objects */

        smObjParams.allocatedPool = allocatedPool;
        smObjParams.pAnchor       = (SM_ANCHOR *) smAnchor;
        smObjParams.smObjFreeAdrs = (char *) smObjFreeAdrs;
        smObjParams.smObjMemSize  = smObjMemSize;
        smObjParams.maxCpus       = DEFAULT_CPUS_MAX;
        smObjParams.maxTasks      = SM_OBJ_MAX_TASK;
        smObjParams.maxSems       = SM_OBJ_MAX_SEM;
        smObjParams.maxMsgQueues  = SM_OBJ_MAX_MSG_Q;
        smObjParams.maxMemParts   = SM_OBJ_MAX_MEM_PART;
        smObjParams.maxNames      = SM_OBJ_MAX_NAME;

        if (smObjSetup (&smObjParams) != OK)
            {
            if (errno == S_smObjLib_SHARED_MEM_TOO_SMALL)
               printf("\nERROR smObjSetup : shared memory pool too small.\n");

            if (allocatedPool)
                free (smObjFreeAdrs);			/* cleanup */

            return (ERROR);
            }
        }

    /* initialize shared memory descriptor */

    smObjInit (&smObjDesc, (SM_ANCHOR *) smAnchor, sysClkRateGet(),
               SM_OBJ_MAX_TRIES, SM_INT_TYPE, SM_INT_ARG1,
	       SM_INT_ARG2, SM_INT_ARG3);

    /* attach to shared memory object facility */

    printf ("Attaching shared memory objects at %#x... ", (int) smAnchor);

    if (smObjAttach (&smObjDesc) != OK)
	{
	printf ("failed: errno = %#x.\n", errno);
        return (ERROR);
	}

    printf("done\n");
    return (OK);
    }

#endif /* __INCusrSmObjc */

