/*
 * console.c
 *	FFSC console monitor task. Connects a user on some serial port
 *	(presumably a modem or console) to the system (presumably the
 *	lone IO6 serial port).
 */

#include <vxworks.h>
#include <sys/types.h>

#include <errno.h>
#include <selectlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tasklib.h>
#include <time.h>
#include <iolib.h>

#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "elsc.h"
#include "flash.h"
#include "log.h"
#include "misc.h"
#include "nvram.h"
#include "timeout.h"
#include "xfer.h"


/* Global variables. Remember that these are global system-wide, */
/* not just for a single task!					 */
console_t *consoles[NUM_CONSOLES];	/* All known consoles */

/*
 * Saved console: console which we fall back on when something is really
 * hosed (as in we lost our console pointer).
 */
console_t *saveConsole = NULL;

/*
 * Saved index of system console.
 */
int saveConsoleIdx ;
int saveSysPortFD;


/*
 * Which COM port we are talking to as a system (for partitioning).
 */
int comPortState = SYSIN_LOWER;

/* Constants */
const consoleinfo_t consDefaultConsInfo = {
	{				/* Ctrl */
		'\010',			/* CONC_BS:   Ctrl-H (BS) */
		'\024',			/* CONC_ESC:  Ctrl-T */
		'\015',			/* CONC_END:  Ctrl-M (CR) */
		'\025',			/* CONC_KILL: Ctrl-U */
		'\005',			/* CONC_EXIT: Ctrl-E */
		'\0',			/* CONC_NULL (Always last!) */
	},
	{				/* Delim */
		';',			/* COND_PB */
		':',			/* COND_HDR */
		ELSC_CMD_BEGIN_CHAR,	/* COND_ELSC */
		FFSC_ACK_SEPARATOR_CHAR,/* COND_RESPSEP */
		OBP_CHAR,		/* COND_OBP */
		'\0',			/* COND_NULL (Always last!) */
	},
	CONCE_ON,			/* CEcho */
	CONEM_OFF,			/* EMsg */
	CONRM_ON,			/* RMsg */
	-1,				/* NVRAMID */
	CIF_VALID | CIF_PAGERVALID,	/* PFlags */
	-1,				/* Wakeup */
	-1,				/* Lines per page */
	{				/* Pager chars */
		'b',			/* CONP_BACK */
		' ',			/* CONP_FWD */
		'q',			/* CONP_QUIT */
	},
	{ 0, 0 }			/* (reserved) */
};


/* Internal function declarations */
static int console(console_t *);
static console_t *consCreate(int[CCFD_MAX], const char *, int, int,
			     const consoleinfo_t *, log_t *);
static void consReadConsInfo(consoleinfo_t *, int);


/*
 * Global debug state variable. Inherited from debug.c
 */
extern debuginfo_t ffscDebug;

/*
 * Returns FD associated with the current system port.
 */
int getSystemPortFD()
{
  if(comPortState == SYSIN_UPPER){
    ffscMsg("getSystemPortFD: fd=%d, state = %c\n",
	    portDebug,comPortState == SYSIN_UPPER? 'u': 'l');
    return portDebug >= 0 ? portDebug: saveSysPortFD;
  }
  if(comPortState == SYSIN_LOWER){
    ffscMsg("getSystemPortFD: fd=%d, state = %c\n",
	    portSystem,comPortState == SYSIN_UPPER? 'u': 'l');
    return portSystem >= 0 ? portSystem: saveSysPortFD;
  }
  else{
    ffscMsg("getSystemPortFD: INTERNAL ERROR != UPPER or LOWER!!!");
    return saveSysPortFD;
  }
}


/*
 * getSystemConsole
 * Returns current system console, null if none exist.
 */



console_t* getSystemConsole( void )
{
  int i;
  for(i = 0; i < NUM_CONSOLES; i++){
    /* An IO6 attached to COM6 */
    if((consoles[i]->FDs[CCFD_SYS] == portDebug) && 
       ((consoles[i]->Mode == CONM_CONSOLE) ||
	(consoles[i]->Mode == CONM_COPYSYS) ||
	(consoles[i]->Mode == CONM_FLASH)   ||
	(consoles[i]->Mode == CONM_WATCHSYS))) {
      /*      if(consoles[i]->FDs[CCFD_USER] == portTerminal) */
	return consoles[i];
    }
    /* An IO6 attached to COM4 */
    else if ((consoles[i]->FDs[CCFD_SYS] == portSystem) &&
	     ((consoles[i]->Mode == CONM_CONSOLE) ||
	      (consoles[i]->Mode == CONM_COPYSYS) ||
	      (consoles[i]->Mode == CONM_FLASH)   ||
	      (consoles[i]->Mode == CONM_WATCHSYS))) {

      /*      if(consoles[i]->FDs[CCFD_USER] == portTerminal) */
	return consoles[i];
    }
  }

  /*  ffscMsg("getSystemConsole: using saved console..."); */
  
  /* James had it returning the saved console.  No idea why.
     A lot of the code depends on NULL being returned in these cases. */
  /*
  return saveConsole; 

  */


  return NULL;
}


/*
 * getSystemConsole
 * Returns index for system console.
 */
int getSystemConsoleIndex( void )
{
  int i;
  
  for(i = 0; i < NUM_CONSOLES; i++){
    /* An IO6 attached to COM6 */
    if((consoles[i]->FDs[CCFD_SYS] == portDebug) &&
       ((consoles[i]->Mode == CONM_CONSOLE) ||
	(consoles[i]->Mode == CONM_COPYSYS) ||
	(consoles[i]->Mode == CONM_FLASH)   ||
	(consoles[i]->Mode == CONM_WATCHSYS))) {

      /*   if(consoles[i]->FDs[CCFD_USER] == portTerminal) */
      return i;
    }
    /* An IO6 attached to COM4 */
    else if((consoles[i]->FDs[CCFD_SYS] == portSystem) &&
	    ((consoles[i]->Mode == CONM_CONSOLE) ||
	     (consoles[i]->Mode == CONM_COPYSYS) ||
	     (consoles[i]->Mode == CONM_FLASH)   ||
	     (consoles[i]->Mode == CONM_WATCHSYS))) {
      /* if(consoles[i]->FDs[CCFD_USER] == portTerminal) */
      return i;
    }
  }

  ffscMsg("Reverting to saved console index...");
  return saveConsoleIdx;

#if 0
    else{
      if(comPortState == SYSIN_LOWER)
	return PORT_SYSTEM;
      else
	return PORT_DEBUG;
    }

  /* 
   * NOT REACHED
   */
  return -1;
#endif

}



/*
 * console
 *	The main function of each console task
 */
int
console(console_t *Console)
{
	fd_set ReadFDs, WriteFDs;
	fd_set ReadyReadFDs, ReadyWriteFDs;
	int    NumReadFDs, NumWriteFDs;
	int    NumReadyFDs;
	struct timeval WakeUp;

	/* Initialize timeout timer for this task */
	if (timeoutInit() != OK) {
	  ffscMsg("Console tasks cannot proceed without timeout timers");
	  return -1;
	}

	/* Initialize FD sets for select */
	FD_ZERO(&ReadFDs);	NumReadFDs = 0;
	FD_ZERO(&WriteFDs);	NumWriteFDs = 0;

	/* Initialize I/O buffers. Notice that we won't bother assigning */
	/* FDs to the buffers until we enter the appropriate input mode. */
	Console->UserInBuf = bufInit(CONSOLE_KBD_BUF_SIZE,
				     BUF_INPUT,
				     &ReadFDs,
				     &NumReadFDs);
	if (Console->UserInBuf == NULL) {
	  ffscMsg("Could not create user input buffer - console task "
		  "unable to proceed");
	  return -1;
	}

	Console->UserOutBuf = bufInit(CONSOLE_SCREEN_BUF_SIZE,
				      BUF_OUTPUT | BUF_MONEOL,
				      &WriteFDs,
				      &NumWriteFDs);
	if (Console->UserOutBuf == NULL) {
		ffscMsg("Could not create user output buffer - console task "
			    "unable to proceed");
		return -1;
	}

	Console->SysInBuf = bufInit(((Console->Type == CONT_SYSTEM)
				     ? MAX_OOBMSG_LEN
				     : CONSOLE_SCREEN_BUF_SIZE),
				    BUF_INPUT | BUF_HOLD,
				    &ReadFDs,
				    &NumReadFDs);
	if (Console->SysInBuf == NULL) {
		ffscMsg("Could not create system input buffer - console task "
			    "unable to proceed");
		return -1;
	}

	Console->SysOutBuf = bufInit(CONSOLE_KBD_BUF_SIZE,
				     BUF_OUTPUT,
				     &WriteFDs,
				     &NumWriteFDs);
	if (Console->SysOutBuf == NULL) {
		ffscMsg("Could not create system output buffer - console task "
			    "unable to proceed");
		return -1;
	}

	Console->MsgBuf = bufInit(CONSOLE_MSG_BUF_SIZE,
				  BUF_MESSAGE | BUF_HOLD,
				  NULL,
				  NULL);
	if (Console->MsgBuf == NULL) {
		ffscMsg("Could not create message buffer - console task "
			    "unable to proceed");
		return -1;
	}

	/* Prepare to read from the router pipe */
	FD_SET(Console->FDs[CCFD_R2C], &ReadFDs);	++NumReadFDs;

	/* Put the user device into its initial mode */
	switch (Console->BaseMode) {

	    case CONM_CONSOLE:
		ceEnterConsoleMode(Console);
		break;

	    case CONM_FFSC:
		ceEnterFFSCMode(Console);
		break;

	    case CONM_IDLE:
		ceEnterIdleMode(Console);
		break;

	    case CONM_RAT:
		ceEnterRATMode(Console);
		break;

	    case CONM_WATCHSYS:
		ceEnterWatchSysMode(Console);
		break;

	    default:
		ffscMsg("Invalid initial mode for console %s: %d",
			Console->Name, Console->BaseMode);
		break;
	}

	/* If the REBOOTMSG flag is set, tell the user that we are awake */
	if (Console->Flags & CONF_REBOOTMSG) {
		char Message[80];

		/* Tell the user we are alive again */
		sprintf(Message,
			"R%ld:MMSC reset complete" STR_END,
			ffscLocal.rackid);
		(void) bufWrite(Console->UserOutBuf,
				Message,
				strlen(Message));

		/* Turn off the reboot message */
		Console->Flags &= ~CONF_REBOOTMSG;
		consUpdateConsInfo(Console);

		/* If we are in FFSC mode, arrange for a fresh prompt */
		if (Console->Mode == CONM_FFSC) {
			Console->Flags |= CONF_SHOWPROMPT;
		}
	}

	/* Arrange to set up a wake up interval when we first start */
	Console->Flags |= CONF_EVALWAKEUP;

	/* Loop until there are no more FDs to read from */
	while (NumReadFDs > 0  ||  NumWriteFDs > 0) {

		/* Print an appropriate prompt if necessary */
		if (Console->Flags & CONF_SHOWPROMPT) {

			/* In PAGER mode, the "prompt" is a call to */
			/* update the currently displayed page.	    */
			if (Console->Mode == CONM_PAGER  &&
			    Console->PageBuf->NumPrint != 0)
			{
				cmPagerUpdate(Console);
			}
			/* May still want to print an actual prompt   */
			/* if cmPagerUpdate EXIT'ed and changed modes */

			if (Console->Flags & CONF_ECHO) {
            /* Flush the output Buffer */
#if 0
ttyinfo();
            if (ioctl(Console->UserOutBuf->FD, FIOWFLUSH, 0) < 0)
               ffscMsg("Unable to Flush output buffer on FD %d",
                        Console->UserOutBuf->FD);
ttyinfo();
#endif

				(void) bufWrite(Console->UserOutBuf,
						STR_ESCPROMPT,
						STR_ESCPROMPT_LEN);
			}
			else if (Console->Mode == CONM_FFSC) {
				(void) bufWrite(Console->UserOutBuf,
						STR_FFSCPROMPT,
						STR_FFSCPROMPT_LEN);
			}
			else if (Console->Mode == CONM_CONSOLE) {
				(void) bufWrite(Console->UserOutBuf,
						STR_END,
						STR_END_LEN);
			}

			Console->Flags &= ~CONF_SHOWPROMPT;
		}

		/* Re-evaluate the wakeup time if necessary */
		if (Console->Flags & CONF_EVALWAKEUP) {
			int WakeUpUSecs;

			Console->Flags &= ~CONF_EVALWAKEUP;

			/* Arrange to wake up periodically even when  */
			/* there are no ready FDs, in case other work */
			/* (e.g. ELSC messages) have cropped up.      */
			WakeUpUSecs = ((Console->WakeUp > 0)
				       ? Console->WakeUp
				       : ffscTune[TUNE_CW_WAKEUP]);
			WakeUp.tv_sec  = WakeUpUSecs / 1000000;
			WakeUp.tv_usec = WakeUpUSecs % 1000000;
		}

		/* Wait for someone to be ready to read */
		ReadyReadFDs  = ReadFDs;
		ReadyWriteFDs = WriteFDs;
		NumReadyFDs = select(FD_SETSIZE,
				     &ReadyReadFDs,
				     &ReadyWriteFDs,
				     NULL,
				     &WakeUp);
		if (NumReadyFDs < 0) {
			if (errno == EINTR) {
				continue;
			}
			ffscMsgS("select failed - console task unable to "
				     "continue");
			return -1;
		}

		/* If the user port is ready for writing, do so */
		if (Console->UserOutBuf->FD >= 0  &&
		    FD_ISSET(Console->UserOutBuf->FD, &ReadyWriteFDs))
		{
			if (bufFlush(Console->UserOutBuf) < 0) {
				bufDetachFD(Console->UserOutBuf);
				ffscMsg("Write error - shut down UserOut on "
					    "%s",
					Console->Name);
			}

			/* If there is pager activity pending, go try */
			/* to update.				      */
			else if (Console->Mode == CONM_PAGER  &&
				 Console->PageBuf->NumPrint != 0)
			{
				cmPagerUpdate(Console);
			}
		}

		/* If the system port is ready for writing, do so */
		if (Console->SysOutBuf->FD >= 0  &&
		    FD_ISSET(Console->SysOutBuf->FD, &ReadyWriteFDs))
		{
			if (bufFlush(Console->SysOutBuf) < 0) {
				bufDetachFD(Console->SysOutBuf);
				ffscMsg("Write error - shut down SysOut on "
					    "%s",
					Console->Name);
			}
		}

		/* If the system port is ready for reading, do so */
		if (Console->SysInBuf->FD >= 0  &&
		    FD_ISSET(Console->SysInBuf->FD, &ReadyReadFDs))
		{

			/* first reset the FD bits in case the mode changes */
			FD_CLR(Console->SysInBuf->FD, &ReadyReadFDs);

			if (bufRead(Console->SysInBuf) != OK) {
			  ffscMsg("reading console->sysinbuf != OK");
				if (Console->Mode == CONM_COPYUSER  ||
				    Console->Mode == CONM_COPYSYS)
				{

					/* Remote side closed connection */
					char  Response[MAX_FFSC_RESP_LEN + 1];

					/* Say what we are going to do */
					if (ffscDebug.Flags & FDBF_CONSOLE) {
						ffscMsg("%s received EOF in "
							    "COPY mode - "
							    "faking EXIT",
							Console->Name);
					}

					/* Fake an exit command */
					cmSendFFSCCmd(Console,
						      "R . EXIT",
						      Response);
					cmPrintResponse(Console, Response);
				}
				else {
					/* Bad news: EOF/Error on local FD */
					bufDetachFD(Console->SysInBuf);
					ffscMsg("Read error - shut down "
						    "SysIn on %s",
						Console->Name);
				}
			}
		}

		/* If there is something in the system input buffer */
		/* process it unless told not to. We handle this    */
		/* separately from the bufRead, in case there was   */
		/* already something in the buffer.		    */
		if (Console->SysInBuf->Len > 0  &&
		    !(Console->Flags & CONF_HOLDSYSIN))
		{
			csSysIn(Console);
		}

		/* If the user port is ready for reading, do so */
		if (Console->UserInBuf->FD >= 0  &&
		    FD_ISSET(Console->UserInBuf->FD, &ReadyReadFDs))
		{

			/* first reset the FD bits in case the mode changes */
			FD_CLR(Console->UserInBuf->FD, &ReadyReadFDs);

			if (bufRead(Console->UserInBuf) < 0) {
				if (Console->Type == CONT_REMOTE) {

				  /* Remote side closed connection */

				  /* 1/7/99  Sasha
				     It is not clear to me why we can't 
				     just run an exit command.  Sending
				     an exit character has no effect in
				     the following modes: PAGER, CONSOLE,
				     RAT, COPYSYS.  Therefore, I'm 
				     changing this code, to be just like
				     in the case of the user */

				  char  Response[MAX_FFSC_RESP_LEN + 1];

				  /* Say what we are going to do */
				  if (ffscDebug.Flags & FDBF_CONSOLE) {
				    ffscMsg("%s received EOF in "
					    "COPY mode - "
					    "faking EXIT",
					    Console->Name);
				  }

				  /* Fake an exit command */
				  cmSendFFSCCmd(Console,
						"R . EXIT",
						Response);
				  cmPrintResponse(Console, Response);

				}
				else {
					/* EOF/Error in non-remote mode */
					/* That's Trouble.		*/
					bufDetachFD(Console->UserInBuf);
					ffscMsg("Read error - shut down "
						    "UserIn on %s",
						Console->Name);
				}
			}
		}

		/* If there is something in the user input buffer */
		/* process it unless told not to.		  */
		if (Console->UserInBuf->Len > 0  &&
		    !(Console->Flags & CONF_HOLDUSERIN))
		{
			cuUserIn(Console);
		}

		/* Check for something from the router */
		if (Console->FDs[CCFD_R2C] >= 0  &&
		    FD_ISSET(Console->FDs[CCFD_R2C], &ReadyReadFDs))
		{
			if (crRouterIn(Console) < 0) {
				FD_CLR(Console->FDs[CCFD_R2C], &ReadFDs);
				--NumReadFDs;

				Console->FDs[CCFD_R2C] = -1;

				ffscMsg("Read error - shut down R2C on %s",
					Console->Name);
			}
		}

		/* If we are now in FLASH mode, drop everything and	*/
		/* arrange to flash a new firmware image.		*/
		if (Console->Mode == CONM_FLASH) {
			int oldPriority;
			int Result;
			struct flashStruct Flash;

			/* Arrange for improved priority */
			taskPriorityGet(0, &oldPriority);
			taskPrioritySet(0, 60);

			/* Enable flashing */
			enable_downloader();

			/* Open up the flash device */
			initFlash(&Flash);
			Result = openFlash(&Flash,
					   WRITE_MODE,
					   Console->FDs[CCFD_PROGRESS]);
			if (Result != 0) {
				ffscMsg("Error %d from openFlash", Result);
			}

			/* If successful, do the appropriate voodoo */
			else {
				Result = getflash(&Flash,
						  Console->FDs[CCFD_REMOTE],
						  Console->FDs[CCFD_PROGRESS]);
				closeFlash(&Flash);

				/* If successful, we can disable the */
				/* serial downloader.		     */
				if (Result == 0) {
					disable_downloader();
				}
			}

			/* Relinquish our higher priority */
			taskPrioritySet(0, oldPriority);

			/* Return to the base input mode */
			clLeaveFlashMode(Console);
			ceEnterMode(Console, Console->PrevMode);
			Console->PrevMode = CONM_UNKNOWN;
		}

		/* If things have been idle for a while, do some */
		/* housekeeping tasks.				 */
		if (NumReadyFDs == 0) {

			/* If there are any pending message, flush them */
			if (Console->MsgBuf->Len > 0  &&
			    Console->Mode != CONM_FLASH)
			{
				if (bufWrite(Console->UserOutBuf,
					     BUF_MSG_END,
					     BUF_MSG_END_LEN)
				    == BUF_MSG_END_LEN)
				{
					bufAppendMsgs(Console->UserOutBuf,
						      Console->MsgBuf);
				}
			}

			/* If the system side is stuck in one of the */
			/* out-of-band message states, sysctlrd must */
			/* have died. Dump the message and return to */
			/* normal operation.			     */
			if (Console->Flags & CONF_OOBMSG) {
				ffscMsg("%s timed out with incomplete OOB msg "
					    "in state %d",
					Console->Name, Console->SysState);

				bufClear(Console->SysInBuf);

				Console->SysState = CONSM_NORMAL;
				Console->SysOffset = 0;
				Console->Flags &= ~CONF_OOBMSG;

				if (Console->Flags & CONF_HOLDUSERIN) {
					bufRelease(Console->UserInBuf);
					bufRelease(Console->SysOutBuf);
					Console->Flags &= ~CONF_HOLDUSERIN;
				}
			}
		}
	}

	/* Farewell - this "shouldn't" happen */
	ffscMsg("WARNING: console %s shutting down", Console->Name);
	return -1;
}


/*
 * consInit
 *	Create and initialize the various console tasks
 */
STATUS
consInit(void)
{
  int sysconIndex;
  consoleinfo_t ConsInfo;
  int ConsFDs[CCFD_MAX];
  int BaseMode, SysFD;

	/* Initially, no console has access to the IO6 */
	sysconIndex = getSystemConsoleIndex();
	if(sysconIndex >= 0){
	  saveConsole = consoles[sysconIndex] ;
	  saveSysPortFD = consoles[sysconIndex]->FDs[CCFD_SYS];
	  saveConsoleIdx = sysconIndex ;
	  consoles[sysconIndex] = NULL ;
	}
	/* Try to read TERMINAL console parameters from NVRAM */
	consReadConsInfo(&ConsInfo, NVRAM_CONSTERM);

	/* Set up the "normal" terminal console */
	if (portSystem < 0  ||  portTerminal < 0) {
		ffscMsg("Unable to start console TERMINAL: Sys=%d Term=%d",
			portSystem, portTerminal);
		consTerminal = NULL;
	}
	else {
		ConsFDs[CCFD_SYS]     = portSystem;
		ConsFDs[CCFD_USER]    = portTerminal;
		ConsFDs[CCFD_C2RR]    = C2RReqFDs[CONS_TERMINAL];
		ConsFDs[CCFD_C2RA]    = C2RAckFDs[CONS_TERMINAL];
		ConsFDs[CCFD_R2C]     = R2CReqFDs[CONS_TERMINAL];
		ConsFDs[CCFD_REMOTE]  = -1;
		ConsFDs[CCFD_PROGRESS]= -1;

		consTerminal = consCreate(ConsFDs,
					  "TERMINAL",
					  CONT_TERMINAL,
					  CONM_CONSOLE,
					  &ConsInfo,
					  logTerminal);
	}

	/* Set up a console task to babysit the IO6 when the other */
	/* consoles are busy doing something else.		   */
	if (portSystem < 0  &&  portDaemon < 0) {
		ffscMsg("Unable to start DAEMON console - no valid FDs");
		consDaemon = NULL;
	}
	else {
		if (portDaemon < 0) {
			SysFD = portSystem;
			BaseMode = CONM_IDLE;
		}
		else {
			SysFD = portDaemon;
			BaseMode = CONM_WATCHSYS;
		}

		ConsFDs[CCFD_SYS]     = SysFD;
		ConsFDs[CCFD_USER]    = -1;
		ConsFDs[CCFD_C2RR]    = C2RReqFDs[CONS_DAEMON];
		ConsFDs[CCFD_C2RA]    = C2RAckFDs[CONS_DAEMON];
		ConsFDs[CCFD_R2C]     = R2CReqFDs[CONS_DAEMON];
		ConsFDs[CCFD_REMOTE]  = -1;
		ConsFDs[CCFD_PROGRESS]= -1;

		ConsInfo = consDefaultConsInfo;
		ConsInfo.CEcho = CONCE_OFF;
		ConsInfo.EMsg  = CONEM_OFF;
		ConsInfo.RMsg  = CONRM_OFF;

		consDaemon = consCreate(ConsFDs,
					"DAEMON",
					CONT_SYSTEM,
					BaseMode,
					&ConsInfo,
					NULL);
	}

	/*
	 * @@TODO: fix this using a "plumb" mechanism whereby you can assign
	 * a function to the port. For now, when we have debug > 0, we will
	 * use the COM5 port for debug output. Otherwise (if debug == 0), we
	 * will spawn a console task for COM5.
	 */
	/* Try to read debugging flags from NVRAM */
	if (nvramRead(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
	  ffscMsg("ERROR: could not read debug info from NVRAM.\r\n");
	}
	else{
	  /* 
	   * Only init alt-console when debug is zero.
	   */	  
	  if(ffscDebug.Flags == 0){
	    /* Try to read ALTERNATE console parameters from NVRAM */
	    consReadConsInfo(&ConsInfo, NVRAM_CONSALT);
	    
	    /* Set up the Alternate console (i.e. modem) */
	    if (portSystem < 0  ||  portAltCons < 0) {
	      if (portSystem < 0  ||  (ffscDebug.Flags & FDBF_CONSOLE)) {
		ffscMsg("Unable to start ALTCONS console: sys=%d "
			"alt=%d",
			portSystem, portAltCons);
	      }
	      consAlternate = NULL;
	    }
	    else {
	      ConsFDs[CCFD_SYS]     = portSystem;
	      ConsFDs[CCFD_USER]    = portAltCons;
	      ConsFDs[CCFD_C2RR]    = C2RReqFDs[CONS_ALTERNATE];
	      ConsFDs[CCFD_C2RA]    = C2RAckFDs[CONS_ALTERNATE];
	      ConsFDs[CCFD_R2C]     = R2CReqFDs[CONS_ALTERNATE];
	      ConsFDs[CCFD_REMOTE]  = -1;
	      ConsFDs[CCFD_PROGRESS]= -1;
	      
	      consAlternate = consCreate(ConsFDs,
					 "ALTERNATE",
					 CONT_ALTERNATE,
					 CONM_RAT,
					 &ConsInfo,
					 logAltCons);
	    }
	  }
	  else
	    ffscMsg("COM5 is DEBUG.\r\n");
	}

	/* Set up a console task for remote FFSCs to use when they */
	/* actually need a console (e.g. ELSC or FFSC mode)	   */
	ConsFDs[CCFD_SYS]     = -1;
	ConsFDs[CCFD_USER]    = -1;
	ConsFDs[CCFD_C2RR]    = C2RReqFDs[CONS_REMOTE];
	ConsFDs[CCFD_C2RA]    = C2RAckFDs[CONS_REMOTE];
	ConsFDs[CCFD_R2C]     = R2CReqFDs[CONS_REMOTE];
	ConsFDs[CCFD_REMOTE]  = -1;
	ConsFDs[CCFD_PROGRESS]= -1;

	consRemote = consCreate(ConsFDs,
				"REMOTE",
				CONT_REMOTE,
				CONM_IDLE,
				&consDefaultConsInfo,
				NULL);

	/*
	 * Setup a second daemon task to babysit the other 
	 * IO6 connected to the debug port (COM6). We will
	 * only do this if debug == 0, in which case debugging 
	 * goes to the Alternate console. In other cases, the debug
	 * port is used to debugging.
	 */

	if (portDaemon < 0) {
	  SysFD = portDebug; /* Actually, an IO6 in COM6 */
	  BaseMode = CONM_IDLE;
	  ffscMsg("DAEMON2 -portDebug/IDLE");
	}
	else {
	  SysFD = portDaemon;
	  BaseMode = CONM_WATCHSYS;
	  ffscMsg("DAEMON2 -portDaemon/WATCHSYS");
	}
	
	ConsFDs[CCFD_SYS]     = SysFD;
	ConsFDs[CCFD_USER]    = -1;
	ConsFDs[CCFD_C2RR]    = C2RReqFDs[CONS_DAEMON2];
	ConsFDs[CCFD_C2RA]    = C2RAckFDs[CONS_DAEMON2];
	ConsFDs[CCFD_R2C]     = R2CReqFDs[CONS_DAEMON2];
	ConsFDs[CCFD_REMOTE]  = -1;
	ConsFDs[CCFD_PROGRESS]= -1;
	
	ConsInfo = consDefaultConsInfo;
	ConsInfo.CEcho = CONCE_OFF;
	ConsInfo.EMsg  = CONEM_OFF;
	ConsInfo.RMsg  = CONRM_OFF;
	
	/*
	 *  Make another daemon task to monitor our COM6 BaseIO 
	 */

	consDaemon2 = consCreate(ConsFDs,
				 "DAEMON2",
				 CONT_SYSTEM,
				 BaseMode,
				 &ConsInfo,
				 NULL);	  


	  
	return OK;
}


/*
 * consUpdateConsInfo
 *	Updates the console info stored in NVRAM for the specified console
 */
STATUS
consUpdateConsInfo(console_t *Console)
{
	consoleinfo_t ConsInfo;

	/* Don't bother if this console doesn't have an NVRAMID */
	if (Console->NVRAMID < 0) {
		return OK;
	}

	/* Build a consoleinfo_t from the specified console_t */
	bzero((char *) &ConsInfo, sizeof(ConsInfo));

	bcopy(Console->Ctrl, ConsInfo.Ctrl, CONC_MAXCTL);
	bcopy(Console->Delim, ConsInfo.Delim, COND_MAXDELIM);
	bcopy(Console->Pager, ConsInfo.Pager, CONP_MAXPAGER);
	ConsInfo.CEcho		= (char) Console->CEcho;
	ConsInfo.EMsg		= (char) Console->EMsg;
	ConsInfo.RMsg		= (char) Console->RMsg;
	ConsInfo.NVRAMID	= (char) Console->NVRAMID;
	ConsInfo.PFlags		= CIF_VALID | CIF_PAGERVALID;
	ConsInfo.WakeUp		= Console->WakeUp;
	ConsInfo.LinesPerPage	= Console->LinesPerPage;

	/* Propagate relevant flags */
	if (Console->Flags & CONF_REBOOTMSG) {
		ConsInfo.PFlags |= CIF_REBOOTMSG;
	}
	if (Console->Flags & CONF_NOPAGE) {
		ConsInfo.PFlags |= CIF_NOPAGE;
	}

	/* Write the resulting console info to NVRAM */
	return nvramWrite(Console->NVRAMID, &ConsInfo);
}




/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * consCreate
 *	Creates a console_t given a list of relevant file descriptors,
 *	a descriptive name and some flags. A console task is then
 *	spawned off to actually do something with it. A pointer to
 *	the console_t is returned, or NULL if something is wrong.
 */
console_t *
consCreate(int FDs[CCFD_MAX],
	   const char *Name,
	   int Type,
	   int InitMode,
	   const consoleinfo_t *ConsInfo,
	   log_t *Log)
{
	char TaskName[CONS_NAME_LEN + 2];
	console_t *Console;
	int i;

	/* Allocate storage for the console_t */
	Console = (console_t *) malloc(sizeof(console_t));
	if (Console == NULL) {
		ffscMsg("Unable to allocate storage for console_t of %s",
			Name);
		return NULL;
	}

	/* Initialize the relevant parts of the console_t */
	strncpy(Console->Name, Name, CONS_NAME_LEN);
	Console->Name[CONS_NAME_LEN] = '\0';

	Console->Type		= Type;
	Console->BaseMode	= InitMode;
	Console->PrevMode	= CONM_UNKNOWN;
	Console->Mode		= CONM_UNKNOWN;
	Console->SysState	= CONSM_UNKNOWN;
	Console->SysOffset	= 0;
	Console->UserState	= CONUM_UNKNOWN;
	Console->UserOffset	= 0;
	Console->CEcho		= ConsInfo->CEcho;
	Console->EMsg		= ConsInfo->EMsg;
	Console->RMsg		= ConsInfo->RMsg;
	Console->NVRAMID	= ConsInfo->NVRAMID;
	Console->Flags		= 0;
	Console->WakeUp		= ConsInfo->WakeUp;
	Console->LinesPerPage	= ConsInfo->LinesPerPage;
	Console->Log		= Log;

	for (i = 0;  i < CCFD_MAX;  ++i) {
		Console->FDs[i] = FDs[i];
	}

	Console->SysInBuf   = NULL;	/* console task will init these */
	Console->SysOutBuf  = NULL;
	Console->UserInBuf  = NULL;
	Console->UserOutBuf = NULL;
	Console->MsgBuf	    = NULL;
	Console->ELSC	    = NULL;
	Console->User	    = NULL;
	Console->PageBuf    = NULL;

	bcopy(ConsInfo->Ctrl, Console->Ctrl, CONC_MAXCTL);
	bcopy(ConsInfo->Delim, Console->Delim, COND_MAXDELIM);
	bcopy(ConsInfo->Pager, Console->Pager, CONP_MAXPAGER);

	/* Propagate any relevant flags */
	if (ConsInfo->PFlags & CIF_REBOOTMSG) {
		Console->Flags |= CONF_REBOOTMSG;
	}
	if (ConsInfo->PFlags & CIF_NOPAGE) {
		Console->Flags |= CONF_NOPAGE;
	}

	/* Go spawn off a console task */
	sprintf(TaskName, "t%s", Console->Name);
	ffscConvertToLowerCase(TaskName + 2);	/* This is really anal... */
	if (taskSpawn(TaskName, 75, 0, 20000, console,
		      (int) Console, 0, 0, 0, 0, 0, 0, 0, 0, 0)
	    == ERROR)
	{
		ffscMsg("Error spawning console handler for %s", Name);
		free(Console);
		return NULL;
	}

	return Console;
}


/*
 * consReadConsInfo
 *	Read console information from NVRAM and/or defaults
 */
void
consReadConsInfo(consoleinfo_t *ConsInfo, int NVRAMID)
{
	/* Read the console information from NVRAM. If we don't have */
	/* a valid entry there, just use the defaults.		     */
	if (nvramRead(NVRAMID, ConsInfo) != OK  ||
	    !(ConsInfo->PFlags & CIF_VALID))
	{

		/* Console info was not found in NVRAM, so use the */
		/* default info instead.			   */
		bcopy((char *) &consDefaultConsInfo,
		      (char *) ConsInfo,
		      sizeof(consoleinfo_t));

		/* Set the NVRAM ID */
		ConsInfo->NVRAMID = NVRAMID;

		/* Try to write the console info out (but ignore */
		/* any errors that may occur)			 */
		nvramWrite(ConsInfo->NVRAMID, ConsInfo);
	}

	/* If the pager stuff isn't present in the ConsInfo, update it */
	else if (!(ConsInfo->PFlags & CIF_PAGERVALID)) {
		bcopy(consDefaultConsInfo.Pager,
		      ConsInfo->Pager,
		      CONP_MAXPAGER);
		ConsInfo->LinesPerPage = consDefaultConsInfo.LinesPerPage;
		ConsInfo->PFlags |= CIF_PAGERVALID;

		nvramWrite(ConsInfo->NVRAMID, ConsInfo);
	}
}
