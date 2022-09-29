/*
 * consleave.c
 *	Functions for leaving various console modes
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


extern int saveConsoleIdx;
extern int saveSysPortFD;
extern console_t* saveConsole;

char  DummyResponse[MAX_FFSC_RESP_LEN + 1];

/*
 * clLeaveCurrentMode
 *	Leave the current console mode
 */
STATUS
clLeaveCurrentMode(console_t *Console)
{
	int Result;

	switch (Console->Mode) {

	    case CONM_CONSOLE:
		Result = clLeaveConsoleMode(Console);
		break;

	    case CONM_ELSC:
		Result = clLeaveELSCMode(Console);
		break;

	    case CONM_FFSC:
		Result = clLeaveFFSCMode(Console);
		break;

	    case CONM_COPYUSER:
		Result = clLeaveCopyUserMode(Console);
		break;

	    case CONM_COPYSYS:
		Result = clLeaveCopySysMode(Console);
		break;

	    case CONM_FLASH:
		Result = clLeaveFlashMode(Console);
		break;

	    case CONM_MODEM:
		Result = clLeaveModemMode(Console);
		break;

	    case CONM_IDLE:
		Result = OK;
		break;

	    case CONM_WATCHSYS:
		Result = clLeaveWatchSysMode(Console);
		break;

	    case CONM_RAT:
		Result = clLeaveRATMode(Console);
		break;

	    case CONM_PAGER:
		Result = clLeavePagerMode(Console);
		break;

	    default:
		ffscMsg("clLeaveMode called with mode %d", Console->Mode);
		Result = ERROR;
		break;
	}

	return Result;
}


/*
 * clLeaveConsoleMode
 * clLeaveELSCMode
 * clLeaveFFSCMode
 * clLeaveCopyUserMode
 * clLeaveCopySysMode
 * clLeaveFlashMode
 * clLeaveModemMode
 * clLeaveWatchSysMode
 * clLeaveRATMode
 * clLeavePagerMode
 *	Do whatever preparations are needed to leave a particular
 *	user device mode. All return OK if successful, -1 if not.
 */
STATUS
clLeaveConsoleMode(console_t *Console)
{
	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* If we had held the system input buffer, that hold can be released */
	if (Console->Flags & CONF_HOLDSYSIN) {
		bufRelease(Console->SysInBuf);
		Console->Flags &= ~CONF_HOLDSYSIN;
	}

	/* If we held the user input buffer, it	can now be released too */
	if (Console->Flags & CONF_HOLDUSERIN) {
		bufRelease(Console->UserInBuf);
		bufRelease(Console->SysOutBuf);
		Console->Flags &= ~CONF_HOLDUSERIN;
	}

	/* Clean up other flags that may have been left on */
	Console->Flags &= ~(CONF_ECHO |
			    CONF_SHOWPROMPT |
			    CONF_OOBMSG |
			    CONF_OOBOKUSER |
			    CONF_OOBOKSYS |
			    CONF_NOFFSCUSER |
			    CONF_NOFFSCSYS);

	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	bufDetachLog(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Relinquish control of the IO6 */
	if (Console == getSystemConsole()) {
	  int idx = getSystemConsoleIndex();
	  if(idx >= 0){
	    saveConsoleIdx = idx;
	    saveConsole = consoles[idx] ;
	    saveSysPortFD = consoles[idx]->FDs[CCFD_SYS];
	    consoles[idx] = NULL ;
	  }
	}

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving CONSOLE mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveELSCMode(console_t *Console)
{
	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	bufDetachLog(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Assuming we were actually attached to a real ELSC, detach */
	if (Console->ELSC != NULL) {

		/* Give ownership of the ELSC back to the router */
		Console->ELSC->Owner = EO_ROUTER;

		/* Restore the ELSC to a known state */
		/* elscSetup(Console->ELSC); */

		/* Advise the ELSC to be quiet, if only to keep */
               	/* the logs a little less cluttered.            */
               	elscDoAdminCommand(Console->ELSC->Bay, "ech 0", NULL);
	
        	/* If we make it this far, we can assume the ELSC is OK */
        	Console->ELSC->Flags &= ~EF_OFFLINE;

		/* Kick the router so it finds out about this */
		routeReEvalFDs = 1;
		cmSendFFSCCmd(Console, "R . NOP", DummyResponse);

		/* We no longer own the ELSC */
		Console->ELSC = NULL;
	}

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving MSC mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveFFSCMode(console_t *Console)
{
	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	bufDetachLog(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving MMSC mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveCopyUserMode(console_t *Console)
{
	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	if (!(Console->Flags & CONF_NOLOG)) {
		bufDetachLog(Console->UserOutBuf);
	}

	/* Close the port associated with the remote system */
	close(Console->FDs[CCFD_REMOTE]);
	Console->FDs[CCFD_REMOTE] = -1;

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;
	Console->Flags	   &= ~(CONF_IGNOREEXIT | CONF_NOLOG);

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving COPYUSER mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveCopySysMode(console_t *Console)
{
	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* Detach the file descriptors associated with our I/O buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	/* Close the port associated with the remote system */
	close(Console->FDs[CCFD_REMOTE]);
	Console->FDs[CCFD_REMOTE] = -1;

	/* If we were doing a progress indicator, clean up */
	if (Console->Flags & CONF_PROGRESS) {
		timeoutWrite(Console->FDs[CCFD_PROGRESS],
			     STR_END,
			     STR_END_LEN,
			     ffscTune[TUNE_CO_PROGRESS]);
		Console->ProgressCount = 0;
		Console->Flags &= ~CONF_PROGRESS;
	}

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving COPYSYS mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveFlashMode(console_t *Console)
{
	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;
	Console->Flags &= ~CONF_PROGRESS;

	/* Done with the file descriptor */
	if (Console->Type == CONT_REMOTE) {
		close(Console->FDs[CCFD_REMOTE]);
	}
	Console->FDs[CCFD_REMOTE] = -1;

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving FLASH mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveModemMode(console_t *Console)
{
	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	bufDetachLog(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving MODEM mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveWatchSysMode(console_t *Console)
{
        int sysconIndex;

	/* Don't try reading from the system for now */
	bufHold(Console->SysInBuf);

	/* If we held the user input buffer, it	can now be released */
	if (Console->Flags & CONF_HOLDUSERIN) {
		bufRelease(Console->UserInBuf);
		bufRelease(Console->SysOutBuf);
		Console->Flags &= ~CONF_HOLDUSERIN;
	}

	/* Clean up other flags that may have been left around */
	Console->Flags &= ~(CONF_OOBMSG |
			    CONF_OOBOKUSER |
			    CONF_OOBOKSYS |
			    CONF_NOFFSCUSER |
			    CONF_NOFFSCSYS);

	/* Detach the file descriptors associated with our I/O buffers */
	bufDetachFD(Console->SysInBuf);
	bufDetachFD(Console->SysOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->SysState   = CONSM_UNKNOWN;
	Console->SysOffset  = 0;

	/* The router can ignore us now if we are the Daemon task */
	if (Console->User == userDaemon) {
		Console->User->Flags &= ~UF_ACTIVE;
	}
	routeReEvalFDs = 1;

	sysconIndex = getSystemConsoleIndex();
	/* Relinquish control of the IO6 */
	if(sysconIndex >= 0){
	  saveConsole = consoles[sysconIndex] ;
	  saveConsoleIdx = sysconIndex ;
	  saveSysPortFD = consoles[sysconIndex]->FDs[CCFD_SYS];
	  consoles[sysconIndex] = NULL ;
	}

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving WATCHSYS mode", Console->Name);
	}

	return OK;
}

STATUS
clLeaveRATMode(console_t *Console)
{
	/* Detach the file descriptors & logs associated with our buffers */
	bufDetachFD(Console->UserInBuf);
	bufDetachFD(Console->UserOutBuf);

	bufDetachLog(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving RAT mode", Console->Name);
	}

	return OK;
}

STATUS
clLeavePagerMode(console_t *Console)
{
	/* Detach the file descriptors & logs associated with our buffers */
	if (!(Console->Flags & CONF_NOPAGE)) {
		bufDetachFD(Console->UserInBuf);
	}
	bufDetachFD(Console->UserOutBuf);

	/* Switch to an undefined state */
	Console->Mode       = CONM_UNKNOWN;
	Console->UserState  = CONUM_UNKNOWN;
	Console->UserOffset = 0;

	/* All done with the pagebuf */
	if (Console->PageBuf != NULL) {
		ffscFreePageBuf(Console->PageBuf);
		Console->PageBuf = NULL;
	}

	/* Release UserIn if it is still held */
	if (Console->Flags & CONF_HOLDUSERIN) {
		Console->Flags &= ~CONF_HOLDUSERIN;
		bufRelease(Console->UserInBuf);
	}

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s leaving PAGER mode", Console->Name);
	}

	return OK;
}
