/* usrLoadSym.c - development symbol table loader */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,25oct94,hdn  swapped 1st and 2nd parameter of usrFdConfig().
01b,13nov93,hdn  added support for floppy and IDE drive.
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to load the development symbol table from either the network
or a SCSI disk. This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrLoadSymc
#define  __INCusrLoadSymc 

#ifdef	INCLUDE_NET_SYM_TBL

/********************************************************************************
* netLoadSymTbl - load system symbol table from network (or from SCSI)
*
* This routine creates a system symbol table and loads it across the network.
* The name of the symbol table file is constructed as:
*      <host>:<bootFile>.sym
*
* RETURNS: OK, or ERROR if symbol table was not loaded.
*
* NOMANUAL
*/

STATUS netLoadSymTbl ()

    {
    char symTblFile [PATH_MAX + 1];   /* name of symbol table file */

    if (strncmp (sysBootParams.bootDev, "scsi", 4) == 0)
	sprintf (symTblFile, "%s.sym", sysBootParams.bootFile);
#ifdef INCLUDE_IDE
    else if (strncmp (sysBootParams.bootDev, "ide", 3) == 0)
	{
        int drive = 0;
        int type = 0;

	if (strlen (sysBootParams.bootDev) == 3)
	    return (ERROR);
	else
	    sscanf (sysBootParams.bootDev, "%*3s%*c%d%*c%d", &drive, &type);

	if (usrIdeConfig (drive, sysBootParams.bootFile) != OK)
	    return (ERROR);

	sprintf (symTblFile, "%s.sym", sysBootParams.bootFile);
	}
#endif /* INCLUDE_IDE */
#ifdef INCLUDE_FD
    else if (strncmp (sysBootParams.bootDev, "fd", 2) == 0)
	{
        int drive = 0;
        int type = 0;

	if (strlen (sysBootParams.bootDev) == 2)
	    return (ERROR);
	else
	    sscanf (sysBootParams.bootDev, "%*2s%*c%d%*c%d", &drive, &type);

	if (usrFdConfig (drive, type, sysBootParams.bootFile) != OK)
	    return (ERROR);

	sprintf (symTblFile, "%s.sym", sysBootParams.bootFile);
	}
#endif /* INCLUDE_FD */
    else
        sprintf (symTblFile, "%s:%s.sym", sysBootParams.hostName,
	         sysBootParams.bootFile);

    printf ("Loading symbol table from %s ...", symTblFile);

    if (loadSymTbl (symTblFile) == ERROR)
	{
	taskDelay (sysClkRateGet () * 3);	/* wait 3 seconds */
	return (ERROR);
	}

    printf ("done\n");
    return (OK);
    }

/*******************************************************************************
*
* loadSymTbl - load system symbol table
*
* This routine loads the system symbol table.
*
* RETURNS: OK or ERROR
*
* NOMANUAL
*/

STATUS loadSymTbl (symTblName)
    char *symTblName;

    {
    char *loadAddr;
    int symfd = open (symTblName, O_RDONLY, 0);

    if (symfd == ERROR)
	{
	printf ("Error opening %s: status = 0x%x\n", symTblName, errno);
	return (ERROR);
	}

    /* load system symbol table */

    loadAddr = 0;		/* to prevent symbols from being relocated */
    if (loadModuleAt (symfd, HIDDEN_MODULE | ((sysFlags & SYSFLG_DEBUG)
					      ? LOAD_ALL_SYMBOLS
					      : LOAD_GLOBAL_SYMBOLS),
		      &loadAddr, &loadAddr, &loadAddr) == NULL)
	{
	printf ("Error loading symbol table: status = 0x%x\n", errno);
	close (symfd);
	return (ERROR);
	}

    close (symfd);

    return (OK);
    }

#endif	/* INCLUDE_NET_SYM_TBL */
#endif	/* __INCusrLoadSymc */
