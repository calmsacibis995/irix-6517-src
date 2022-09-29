/*
 * consenter.c
 *	Functions for entering various console modes
 */

#include <vxworks.h>
#include <sys/types.h>

#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "elsc.h"
#include "misc.h"
#include "route.h"
#include "timeout.h"
#include "user.h"

extern console_t* saveConsole;
extern int saveConsoleIdx; 
extern int saveSysPortFD;

/*
 * ceEnterMode
 *	Enter the specified console mode
 */
STATUS
ceEnterMode(console_t *Console, int Mode)
{
	int Result;

	switch (Mode) {

	    case CONM_CONSOLE:
		Result = ceEnterConsoleMode(Console);
		break;

	    case CONM_ELSC:
		Result = ceEnterELSCMode(Console);
		break;

	    case CONM_FFSC:
		Result = ceEnterFFSCMode(Console);
		break;

	    case CONM_COPYUSER:
		Result = ceEnterCopyUserMode(Console);
		break;

	    case CONM_COPYSYS:
		Result = ceEnterCopySysMode(Console);
		break;

	    case CONM_FLASH:
		Result = ceEnterFlashMode(Console);
		break;

	    case CONM_MODEM:
		Result = ceEnterModemMode(Console);
		break;

	    case CONM_IDLE:
		Result = ceEnterIdleMode(Console);
		break;

	    case CONM_WATCHSYS:
		Result = ceEnterWatchSysMode(Console);
		break;

	    case CONM_RAT:
		Result = ceEnterRATMode(Console);
		break;

	    case CONM_PAGER:
		Result = ceEnterPagerMode(Console);
		break;

	    default:
		ffscMsg("ceEnterMode called with mode %d", Mode);
		Result = ERROR;
		break;
	}

	return Result;
}


/*
 * ceEnterConsoleMode
 * ceEnterELSCMode
 * ceEnterFFSCMode
 * ceEnterCopyUserMode
 * ceEnterCopySysMode
 * ceEnterFlashMode
 * ceEnterModemMode
 * ceEnterIdleMode
 * ceEnterWatchSysMode
 * ceEnterRATMode
 * ceEnterPagerMode
 *	Do whatever preparations are needed to enter a particular
 *	user input mode. All return OK/ERROR.
 */
STATUS
ceEnterConsoleMode(console_t *Console)
{
        int sysconIndex;
	console_t *syscon;

	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering CONSOLE mode", Console->Name);
	}


	/* If the BaseIO is currently owned by a babysitter task, */
	/* then it is OK for us to take it back.		  */
	syscon = getSystemConsole();
	if ((syscon!= NULL) && (syscon->Mode == CONM_WATCHSYS)) {

		/* XXX It is generally Not Cool for a console task to	*/
		/*     be calling userXXX functions, those are normally	*/
		/*     only called by the router task. However, it is	*/
		/*     probably safe in this limited case of sending a	*/
		/*     command to consBaseIO, which doesn't usually	*/
		/*     take commands anyway.				*/



		if (userChangeInputMode(syscon->User, 
					CONM_IDLE, 0) != OK)
		{
			ffscMsg("Unable to acquire base I/O port");
			Console->Flags |= CONF_REDIRECTED;
			return ceEnterFFSCMode(Console);
		}

		/* System console "should" be NULL at this point */
	}

	/* Make sure the console isn't owned by someone else */
	syscon = getSystemConsole();
	if (syscon != NULL && syscon != Console) {
		Console->Flags |= CONF_REDIRECTED;
		ffscMsg("Someone else has base I/O");
		return ceEnterFFSCMode(Console);
	}

	/* Remember the new mode */
	ffscMsg("Console %s successfully entered CONSOLE mode", 
		Console->Name);
	Console->Mode	   = CONM_CONSOLE;
	Console->SysState  = CONSM_NORMAL;
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors & logs with our I/O buffers */
	bufAttachFD(Console->SysInBuf, Console->FDs[CCFD_SYS]);
	bufAttachFD(Console->SysOutBuf, Console->FDs[CCFD_SYS]);

	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	bufAttachLog(Console->UserOutBuf, Console->Log);

	/* See if there are any TTY-level restrictions */
	cmCheckTTYFlags(Console);


	/* Take ownership of the IO6 */
	sysconIndex = getSystemConsoleIndex();
	saveConsole = consoles[sysconIndex];
	saveSysPortFD = consoles[sysconIndex]->FDs[CCFD_SYS];
	saveConsoleIdx = sysconIndex;
	consoles[sysconIndex] = Console;



	/* Print a READY message if necessary */
	if (Console->Flags & CONF_READYMSG) {
		char ReadyStr[16];

		sprintf(ReadyStr, "R%ld:READY\r", ffscLocal.rackid);
		bufWrite(Console->UserOutBuf, ReadyStr, strlen(ReadyStr));

		Console->Flags &= ~CONF_READYMSG;
	}

	/* If we hadn't actually planned to end up in console mode, say so */
	if (Console->Flags & CONF_UNSTEALMSG) {
		char Message[80];

		sprintf(Message,
			STR_END "*** System access has been granted/restored");
		bufWrite(Console->UserOutBuf, Message, strlen(Message));
		Console->Flags &= ~CONF_UNSTEALMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}

STATUS
ceEnterELSCMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering MSC mode", Console->Name);
	}

	/* Make sure the console isn't owned by someone else */
	if (Console->ELSC->Owner != EO_ROUTER) {
		Console->ELSC = NULL;
		Console->Flags |= CONF_REDIRECTED;
		return ceEnterFFSCMode(Console);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_ELSC;
	Console->SysState  = CONSM_NORMAL;
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors & logs with our I/O buffers */
	bufAttachFD(Console->SysInBuf, Console->ELSC->FD);
	bufAttachFD(Console->SysOutBuf, Console->ELSC->FD);

	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	bufAttachLog(Console->UserOutBuf, Console->Log);

	/* Take ownership of the ELSC */
	Console->ELSC->Owner = ((Console->Type == CONT_REMOTE)
				? EO_REMOTE
				: EO_CONSOLE);
	routeReEvalFDs = 1;

	/* Print a READY message if necessary */
	if (Console->Flags & CONF_READYMSG) {
		char ReadyStr[16];

		sprintf(ReadyStr, "R%ld:READY\r", ffscLocal.rackid);
		bufWrite(Console->UserOutBuf, ReadyStr, strlen(ReadyStr));

		Console->Flags &= ~CONF_READYMSG;
	}

	/* Print a welcome message if necessary */
	if (Console->Flags & CONF_WELCOMEMSG) {
		char Message[80];
		char ExitChar[20];

		ffscCtrlCharToString(Console->Ctrl[CONC_EXIT], ExitChar);
		sprintf(Message,
			STR_END "*** Entering MSC mode, type %s to exit ***"
			    STR_END,
			ExitChar);
		bufWrite(Console->UserOutBuf, Message, strlen(Message));

		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* Set the appropriate password */
	if (elscSendPassword(Console->ELSC->Bay, Console->User->Authority)
	    != OK)
	{
		char Message[80];

		sprintf(Message,
			STR_END "*** Warning: Unable to set MSC password ***"
			    STR_END);
		bufWrite(Console->UserOutBuf, Message, strlen(Message));
	}

	/* Turn on ELSC echoing if desired */
	if (Console->User != NULL  &&
	    (Console->User->Options & UO_ELSCMODEECHO))
	{
		elscDoAdminCommand(Console->ELSC->Bay, "ech 1", NULL);
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}

STATUS
ceEnterFFSCMode(console_t *Console)
{
	char Message[80];

	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering MMSC mode", Console->Name);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_FFSC;
	Console->SysState  = CONSM_UNKNOWN;	/* Shouldn't get sys input! */
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors & logs with our I/O buffers. */
	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	bufAttachLog(Console->UserOutBuf, Console->Log);

	/* Print a READY message if necessary */
	if (Console->Flags & CONF_READYMSG) {
		char ReadyStr[16];

		sprintf(ReadyStr, "R%ld:READY\r", ffscLocal.rackid);
		bufWrite(Console->UserOutBuf, ReadyStr, strlen(ReadyStr));

		Console->Flags &= ~CONF_READYMSG;
	}

	/* If we hadn't actually planned to end up in FFSC mode, say so */
	if (Console->Flags & CONF_REDIRECTED) {
		sprintf(Message, STR_END "*** Console/MSC is in use\r\n");
		bufWrite(Console->UserOutBuf, Message, strlen(Message));
		Console->Flags &= ~CONF_REDIRECTED;
	}

	/* If we are swapping system console ports, let the user know
	 * which one we are talking to now (COM4/6 IO6).
	 */
	if(Console->Flags & CONF_SYSCDIRECT){
	  sprintf(Message, STR_END "*** Console is IO6 on COM%d",
		  comPortState);
	  bufWrite(Console->UserOutBuf, Message, strlen(Message));
	  Console->Flags &= ~CONF_SYSCDIRECT;
	}

	if (Console->Flags & CONF_STOLENMSG) {
	  sprintf(Message, STR_END "*** System console has been stolen");
	  bufWrite(Console->UserOutBuf, Message, strlen(Message));
	  Console->Flags &= ~CONF_STOLENMSG;
	}

	/* Print a welcome message if necessary */
	if (Console->Flags & (CONF_REDIRECTED | CONF_WELCOMEMSG)) {
		char ExitChar[20];

		ffscCtrlCharToString(Console->Ctrl[CONC_EXIT], ExitChar);
		sprintf(Message,
			STR_END "*** Entering MMSC mode, type %s to exit ***"
			    STR_END,
			ExitChar);
		bufWrite(Console->UserOutBuf, Message, strlen(Message));

		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	return OK;
}

STATUS
ceEnterCopyUserMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering COPYUSER mode", Console->Name);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_COPYUSER;
	Console->SysState  = CONSM_NORMAL;
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors and logs with our I/O buffers */
	bufAttachFD(Console->SysInBuf, Console->FDs[CCFD_REMOTE]);
	bufAttachFD(Console->SysOutBuf, Console->FDs[CCFD_REMOTE]);

	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	if (!(Console->Flags & CONF_NOLOG)) {
		bufAttachLog(Console->UserOutBuf, Console->Log);
	}

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Print a welcome message if necessary */
	if (Console->Flags & CONF_WELCOMEMSG) {
		char Message[80];
		char ExitChar[20];

		ffscCtrlCharToString(Console->Ctrl[CONC_EXIT], ExitChar);
		sprintf(Message,
			STR_END
			    "*** Remote connection complete, type %s to "
			    "exit ***"
			    STR_END,
			ExitChar);
		bufWrite(Console->UserOutBuf, Message, strlen(Message));

		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}

STATUS
ceEnterCopySysMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering COPYSYS mode", Console->Name);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_COPYSYS;
	Console->SysState  = CONSM_NORMAL;
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate appropriate file descriptors with our I/O buffers */
	bufAttachFD(Console->SysInBuf, Console->FDs[CCFD_REMOTE]);
	bufAttachFD(Console->SysOutBuf, Console->FDs[CCFD_REMOTE]);

	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_SYS]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_SYS]);

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* Make arrangements for a progress indicator if desired. */
	if (Console->Flags & CONF_PROGRESS) {
		Console->ProgressCount = 0;
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}

STATUS
ceEnterFlashMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering FLASH mode", Console->Name);
	}

	/* Purge any cruft remaining in the user buffers, mostly for the */
	/* benefit of the Main Loop, so it doesn't try to run UserIn.	 */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Remember the new mode */
	Console->Mode = CONM_FLASH;

	/* In FLASH mode, we communicate with the REMOTE port directly	*/
	/* so no buffers are involved.					*/

	/* If we are talking to a remote FFSC, tell it that the */
	/* connection is established.				*/
	if (Console->Flags & CONF_READYMSG) {
		fdprintf(Console->FDs[CCFD_REMOTE],
			 "R%ld:READY\r",
			 ffscLocal.rackid);
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	return OK;
}

STATUS
ceEnterModemMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering MODEM mode", Console->Name);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_MODEM;
	Console->SysState  = CONSM_NORMAL;
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate appropriate file descriptors with our I/O buffers. */
	/* In this case, the "system" is the modem that is (presumably) */
	/* attached to the other console's normal user port.		*/
	bufAttachFD(Console->SysInBuf, Console->FDs[CCFD_REMOTE]);
	bufAttachFD(Console->SysOutBuf, Console->FDs[CCFD_REMOTE]);

	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	bufAttachLog(Console->UserOutBuf, Console->Log);

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Print a welcome message if necessary */
	if (Console->Flags & CONF_WELCOMEMSG) {
		char Message[80];
		char ExitChar[20];

		ffscCtrlCharToString(Console->Ctrl[CONC_EXIT], ExitChar);
		sprintf(Message,
			STR_END
			    "*** Attached to other console port, type %s to "
			    "exit ***"
			    STR_END,
			ExitChar);
		bufWrite(Console->UserOutBuf, Message, strlen(Message));

		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}


/*
 * ceEnterIdleMode
 *	Enters Idle mode, meaning there is no user for this console
 *	task at the moment. If the console task is holding any
 *	resources, this is a good time for it to let go of them.
 */
STATUS
ceEnterIdleMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering IDLE mode", Console->Name);
	}

	/* If this is a remote console, dissociate from the previous user */
	if (Console->Type == CONT_REMOTE) {

		/* All done with the remote FD */
		if (Console->FDs[CCFD_USER] >= 0) {
			close(Console->FDs[CCFD_USER]);
			Console->FDs[CCFD_USER] = -1;
		}

		/* Release the the console task from the user */
		if (Console->User != NULL  &&
		    !(Console->User->Flags & UF_AVAILABLE))
		{
			userReleaseRemote(Console->User);
		}
	}

	/* Purge any cruft remaining in the user buffers, mostly for the */
	/* benefit of the Main Loop, so it doesn't try to run UserIn.	 */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* Formally switch modes */
	Console->Mode = CONM_IDLE;

	return OK;
}

STATUS
ceEnterWatchSysMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering WATCHSYS mode", Console->Name);
	}

	/* If the BaseIO is currently owned by the another task, bail out */
	if (getSystemConsole() != NULL  &&  portDaemon < 0) {
		ffscMsg("Tried to enter WATCHSYS mode when IO6 owned by %s",
			(getSystemConsole())->Name);
		return ceEnterIdleMode(Console);
	}

	/* Remember the new mode */
	Console->Mode	   = CONM_WATCHSYS;
	Console->SysState  = CONSM_NORMAL;

	/* Associate appropriate file descriptors with our I/O buffers */
	bufAttachFD(Console->SysInBuf, Console->FDs[CCFD_SYS]);
	bufAttachFD(Console->SysOutBuf, Console->FDs[CCFD_SYS]);

	/* See if there are any TTY-level restrictions */
	cmCheckTTYFlags(Console);

	/* Take ownership of the IO6 if there is no daemon port */
	if (portDaemon < 0) {
	  int idx = getSystemConsoleIndex();
	  if(idx >= 0){
	    saveConsole = consoles[idx];
	    saveConsoleIdx = idx;
	    consoles[idx] = Console;
	  }
	}

	/* Ask the router to listen to us */
	if (Console->User == userDaemon) {
		Console->User->Flags |= UF_ACTIVE;
	}
	routeReEvalFDs = 1;

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* OK to read from the system once again */
	bufRelease(Console->SysInBuf);

	return OK;
}

STATUS
ceEnterRATMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering RAT mode", Console->Name);
	}

	/* Remember the new mode */
	Console->Mode      = CONM_RAT;
	Console->SysState  = CONSM_UNKNOWN;	/* Shouldn't get sys input! */
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors and logs with our I/O buffers. */
	bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	bufAttachLog(Console->UserOutBuf, Console->Log);

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	return OK;
}

STATUS
ceEnterPagerMode(console_t *Console)
{
	/* Print a helpful debugging message if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s entering PAGER mode", Console->Name);
	}

	/* Make sure we have a pagebuf to read */
	if (Console->PageBuf == NULL) {
		STATUS Result;

		ffscMsg("Console %s entered PAGER mode with no pagebuf",
			Console->Name);
		Result = ceEnterMode(Console, Console->PrevMode);
		Console->PrevMode = CONM_UNKNOWN;

		return Result;
	}

	/* Remember the new mode */
	Console->Mode      = CONM_PAGER;
	Console->SysState  = CONSM_UNKNOWN;	/* Shouldn't get sys input! */
	Console->UserState = CONUM_NORMAL;

	/* Purge any cruft remaining in the user buffers */
	bufClear(Console->UserInBuf);
	bufClear(Console->UserOutBuf);

	/* Associate file descriptors (but no logs) with our I/O buffers. */
	if (!(Console->Flags & CONF_NOPAGE)) {
		bufAttachFD(Console->UserInBuf, Console->FDs[CCFD_USER]);
	}
	bufAttachFD(Console->UserOutBuf, Console->FDs[CCFD_USER]);

	/* Ignore a request for a READY message */
	if (Console->Flags & CONF_READYMSG) {
		Console->Flags &= ~CONF_READYMSG;
	}

	/* Ignore a request for a welcome message */
	if (Console->Flags & CONF_WELCOMEMSG) {
		Console->Flags &= ~CONF_WELCOMEMSG;
	}

	/* Arrange for the first page of output */
	Console->PageBuf->CurrPtr  = Console->PageBuf->Buf;
	Console->PageBuf->CurrLine = 0;

	if (Console->LinesPerPage < 1) {
		Console->PageBuf->NumPrint = ffscTune[TUNE_PAGE_DFLT_LINES];
	}
	else {
		Console->PageBuf->NumPrint = Console->LinesPerPage;
	}

	/* This will kick off the first cmPagerUpdate */
	Console->Flags |= CONF_SHOWPROMPT;

	return OK;
}
