/* usrScript.c - startup script module */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to execute a startup script of VxWorks shell commands.
This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrScriptc
#define  __INCusrScriptc 

#ifdef	INCLUDE_STARTUP_SCRIPT

/******************************************************************************
*
* usrStartupScript - make shell read initial startup script file
*
* This routine reads a startup script before the VxWorks
* shell comes up interactively.
*
* NOMANUAL
*/

void usrStartupScript (fileName)
    char *fileName;

    {
    int old;
    int new;

    if (fileName == NULL)
        return;

    new = open (fileName, O_RDONLY, 0);

    if (new != ERROR)
        {
        printf ("Executing startup script %s ...\n", fileName);
        taskDelay (sysClkRateGet () / 2);

        old = ioGlobalStdGet (STD_IN);  /* save original std in */
        ioGlobalStdSet (STD_IN, new);   /* set std in to script */
        shellInit (SHELL_STACK_SIZE, FALSE);    /* execute commands */

        /* wait for shell to finish */
        do
            taskDelay (sysClkRateGet ());
        while (taskNameToId (shellTaskName) != ERROR);

        close (new);
        ioGlobalStdSet (STD_IN, old);   /* restore original std in */

        printf ("\nDone executing startup script %s\n", fileName);
        taskDelay (sysClkRateGet () / 2);
        }
    else
        printf ("Unable to open startup script %s\n", fileName);
    }

#endif	/* INCLUDE_STARTUP_SCRIPT */
#endif	/* __INCusrScriptc */
