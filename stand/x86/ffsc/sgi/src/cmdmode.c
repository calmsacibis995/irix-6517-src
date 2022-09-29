/*
 * cmdmode.c
 *	User command handling functions that deal with the console input mode
 */

#include <vxworks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "addrd.h"
#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "msg.h"
#include "remote.h"
#include "route.h"
#include "user.h"


extern console_t* saveConsole;
extern int saveConsoleIdx; 

/*
 * cmdCONSOLE
 *	Processes a "CONSOLE" command. If arguments are present,
 *	they are passed along to the system input. Otherwise, enter
 *	console mode. Any destination is ignored (for now, anyway;
 *	it might be nice to support switching into console mode, etc.
 *	on a different rack once partitions are supported).
 */
int
cmdCONSOLE(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Args = *CmdP;

	/* Skip over leading white space */
	if (Args != NULL) {
		Args += strspn(Args, " \t");
	}

	/* If any arguments are present, simply generate a SYS command	*/
	/* to the appropriate console task				*/
	if (Args != NULL) {
		if (User->Console == getSystemConsole() ) {

			/* The caller owns the system (making this command */
			/* kind of silly) so just bundle SYS command with  */
			/* the response.				   */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;SYS %s",
				Args);
		}
		else if (getSystemConsole() == NULL) {

			/* Nobody owns the IO6 yet - "try again later" */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR BUSY");
		}
		else if ((getSystemConsole())->Mode != CONM_WATCHSYS) {

			/* Somebody else owns the console - hands off */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR INUSE");
		}
		else {

			/* The Base I/O is owned by a monitor task. Ask */
			/* it to handle this for us.			*/
			char Command[MAX_CONS_CMD_LEN + 1];

			sprintf(Command, "SYS %s", Args);
			if (userSendCommand((getSystemConsole())->User,
					    Command,
					    Responses[ffscLocal.rackid]->Text)
			    == OK)
			{
				sprintf(Responses[ffscLocal.rackid]->Text,
					"OK");
			}
		}
	}

	/* Otherwise, arrange for console mode */
	else {

		/* Make sure this type of user is allowed to change modes */
		if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
			ffscMsg("User %s tried to enter CONSOLE mode",
				User->Name);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR SOURCE");
			return PFC_DONE;
		}

		/* Make sure the IO6 is available */
		if (getSystemConsole() == NULL  ||  
		    (getSystemConsole())->Mode == CONM_WATCHSYS)
		{
		  ffscMsg("getSystemConsole == NULL || Mode == Watchsys!");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE FS%d M%d",
				getSystemPortFD(),CONM_CONSOLE);
		}
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR INUSE");
		}
	}



	return PFC_DONE;
}


/*
 * cmdDIRECT
 *	Processes a "DIRECT" command. Attaches the current console
 *	directly to the "other" console (COM5 if we are COM1 & vice versa).
 *	The other console is considered to be the "system" for our purposes.
 *	Useful for tinkering with modems. Any destination is ignored (for
 *	now, anyway; it might be nice to support switching into direct
 *	mode, etc. on a different rack once partitions are supported).
 */
int
cmdDIRECT(char **CmdP, user_t *User, dest_t *Dest)
{
	user_t *OtherUser;

	/* Make sure the command was issued from a legitimate user */
	if (User == userTerminal) {
		OtherUser = userAlternate;
	}
	else if (User == userAlternate) {
		OtherUser = userTerminal;
	}
	else {
		/* Not valid from this origin */
		ffscMsg("User %s attempted to use DIRECT command",
			User->Name);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR SOURCE");
		return PFC_DONE;
	}

	/* Make sure the user is authorized */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Force the other console into IDLE mode */
	if (userChangeInputMode(OtherUser, CONM_IDLE, 0) != OK) {
		/* userChangeInputMode has logged the error */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR INTERNAL");
		return PFC_DONE;
	}
	/* Other user "should" be in IDLE state after userChangeInputMode */

	/* If the other console still isn't idle, give up */
	if (OtherUser->Console->Mode != CONM_IDLE) {
		ffscMsg("Timed out waiting for %s to idle",
			OtherUser->Name);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR BUSY");
		return PFC_DONE;
	}

	/* Go ahead and switch to MODEM mode */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE BS%d FS%d M%d",
		CONF_WELCOMEMSG,
		OtherUser->Console->FDs[CCFD_USER],
		CONM_MODEM);

	/* Tell the router to fire up consBaseIO if necessary */
	routeCheckBaseIO = 1;

	return PFC_DONE;
}


/*
 * cmdELSC
 *	Processes an MSC/ELSC command. If arguments are present, they
 *	are passed as a command to the local ELSC (if it is selected).
 *	Otherwise, arrange to enter ELSC mode on the console.
 */
int
cmdELSC(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Args = *CmdP;
	int  Result;

	/* If any arguments are present, send them along as an ELSC */
	/* command (if our rack was selected)			    */
	if (Args != NULL  &&  *(Args + strspn(Args, " \t")) != '\0') {
		routeProcessELSCCommand(Args, User, Dest);
		Result = PFC_FORWARD;
	}

	/* Otherwise, arrange for ELSC mode */
	else {
		bayid_t  Bay = BAYID_UNASSIGNED;
		int NumBays;
		rackid_t Rack = RACKID_UNASSIGNED;

		/* Make sure this type of user is allowed to change modes */
		if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
			ffscMsg("User %s tried to enter MSC mode",
				User->Name);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR SOURCE");
			return PFC_DONE;
		}

		/* Make sure only one bay has been specified */
		NumBays = destNumBays(Dest, &Rack, &Bay);
		if (NumBays != 1) {
			ffscMsg("Attempted to enter MSC mode on %d bays",
				NumBays);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* If the request originated on a remote machine, */
		/* arrange to use its terminal for user I/O	  */
		if (User->Type == UT_FFSC) {
			char   Command[MAX_CONS_CMD_LEN + 1];
			user_t *LocalUser;

			/* The request better be for this rack */
			if (Rack != ffscLocal.rackid) {
				ffscMsg("Remote MMSC tried to enter MSC mode "
					    "on remote bay");
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR DEST");
				return PFC_DONE;
			}

			/* Say what's happening if desired */
			if (ffscDebug.Flags & FDBF_ROUTER) {
				ffscMsg("Remote MMSC has requested MSC mode "
					    "on FD %d",
					User->InReq->FD);
			}

			/* Make sure the ELSC is available for use */
			if (ELSC[Bay].Owner != EO_ROUTER) {
				ffscMsg("MSC already in use by code %d",
					ELSC[Bay].Owner);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld%c:ERROR INUSE",
					ffscLocal.rackid,
					ELSC[Bay].Name);
				Responses[ffscLocal.rackid]->State = RS_DONE;

				/* We return PFC_FORWARD so the router	*/
				/* doesn't try to prepend a header to	*/
				/* our response. It's harmless since we	*/
				/* were the only selected rack/bay.	*/
				return PFC_FORWARD;
			}

			/* Acquire the remote console task */
			LocalUser = userAcquireRemote(User);
			if (LocalUser == NULL) {
				/* Error should already be logged */
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR BUSY");
				return PFC_DONE;
			}

			/* Generate the actual command that we will send to */
			/* the console and send it to the remote console.   */
			sprintf(Command,
				"STATE FE%ld FU%d M%d",
				Bay, User->InReq->FD, CONM_ELSC);
			if (userSendCommand(LocalUser,
					    Command,
					    Responses[ffscLocal.rackid]->Text)
			    != OK)
			{
				/* userSendCommand has logged the error */
				userReleaseRemote(LocalUser);
				return PFC_DONE;
			}

			/* Make sure we don't close the remote FD */
			User->InReq->FD = -1;

			/* Consider the operation a success. Note that */
			/* the console task will take care of sending  */
			/* a response to the remote side.	       */
			Responses[ffscLocal.rackid]->Text[0] = '\0';
			Result = PFC_DONE;
		}

		/* Apparently the request originated locally. If the */
		/* request is *for* a remote machine, arrange for a  */
		/* connection to it to be used for system I/O.	     */
		else if (Rack != ffscLocal.rackid) {
			int RemoteFD;

			/* If the user is already remote, don't let them */
			/* get even more remote by routing them to yet	 */
			/* another rack, the bookkeeping is too hard.	 */
			if (User->Type == UT_REMOTE) {
				ffscMsg("Trying to connect TO remote machine "
					    "FROM a remote machine");
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR TOOREMOTE");
				return PFC_DONE;
			}

			/* Set up the remote connection */
			RemoteFD = remStartConnectCommand(User,
							  "ELSC",
							  Rack,
							  Dest);
			if (RemoteFD < 0) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR CONNECT"
					    FFSC_ACK_SEPARATOR
					    "%s",
					users[USER_FFSC + Rack]->OutAck->Text);
				return PFC_DONE;
			}
			else {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"OK;STATE BS%d FS%d M%d",
					CONF_WELCOMEMSG,
					RemoteFD,
					CONM_COPYUSER);
			}

			Result = PFC_DONE;
		}

		/* Otherwise, we have a local request to enter ELSC */
		/* mode on the local rack. That's easy.		    */
		else {
			if (ELSC[Bay].Owner != EO_ROUTER) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld%c:ERROR INUSE",
					ffscLocal.rackid,
					ELSC[Bay].Name);
				Responses[ffscLocal.rackid]->State = RS_DONE;

				/* We return PFC_FORWARD so the router	*/
				/* doesn't try to prepend a header to	*/
				/* our response. It's harmless since we	*/
				/* were the only selected rack/bay.	*/
				Result = PFC_FORWARD;
			}
			else {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"OK;STATE BS%d FE%ld M%d",
					CONF_WELCOMEMSG, Bay, CONM_ELSC);
				Result = PFC_DONE;
			}
		}

		/* Tell the router to fire up consBaseIO if necessary */
		routeCheckBaseIO = 1;
	}

	return Result;
}


/*
 * cmdFFSC
 *	Processes an "MMSC" command. If arguments are present, they
 *	are processed locally (if our rack was selected). Otherwise,
 *	arrange to enter FFSC mode on the addressed rack.
 */
int
cmdFFSC(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Args = *CmdP;
	int  Result;

	/* If any arguments are present, process them as an FFSC */
	/* command recursively (if our rack was selected). In	 */
	/* this case, ELSC commands are illegal.		 */
	if (Args != NULL  &&  *(Args + strspn(Args, " \t")) != '\0') {
		if (destRackIsSelected(Dest, ffscLocal.rackid)) {
			Result = cmdProcessFFSCCommand(*CmdP, User, Dest);
			if (Result == PFC_NOTFFSC) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR CMD");
				Result = PFC_DONE;
			}
		}
		else {
			/* Actually, let some other rack(s) do it */
			return PFC_FORWARD;
		}
	}

	/* Otherwise, arrange for FFSC mode */
	else {
		int NumRacks;
		rackid_t Rack = -1;

		/* Make sure this type of user is allowed to change modes */
		if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
			ffscMsg("User %s tried to enter MMSC mode",
				User->Name);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR SOURCE");
			return PFC_DONE;
		}

		/* Make sure only one rack is specified in the destination */
		NumRacks = destNumRacks(Dest, &Rack);
		if (NumRacks != 1) {
			ffscMsg("Attempted to enter MMSC mode on %d racks",
				NumRacks);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* If the request originated on a remote machine, */
		/* arrange to use its terminal for user I/O	  */
		if (User->Type == UT_FFSC) {
			char   Command[MAX_CONS_CMD_LEN + 1];
			user_t *LocalUser;

			/* The request better be for this rack */
			if (Rack != ffscLocal.rackid) {
				ffscMsg("Remote MMSC tried to enter MMSC mode "
					    "on remote rack");
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR DEST");
				return PFC_DONE;
			}

			/* Say what's happening if desired */
			if (ffscDebug.Flags & FDBF_ROUTER) {
				ffscMsg("Remote MMSC has requested MMSC mode "
					    "on FD %d",
					User->InReq->FD);
			}

			/* Acquire the remote console task */
			LocalUser = userAcquireRemote(User);
			if (LocalUser == NULL) {
				/* Error should already be logged */
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR BUSY");
				return PFC_DONE;
			}

			/* Generate the actual command that we will send to */
			/* the console then send it.			    */
			sprintf(Command,
				"STATE FU%d M%d",
				User->InReq->FD, CONM_FFSC);
			if (userSendCommand(LocalUser,
					    Command,
					    Responses[ffscLocal.rackid]->Text)
			    != OK)
			{
				/* userSendCommand has logged the error */
				userReleaseRemote(LocalUser);
				return PFC_DONE;
			}

			/* Make sure we don't close the remote FD */
			User->InReq->FD = -1;

			/* Consider the operation a success. Note that */
			/* the console task will take care of sending  */
			/* a response to the remote side.	       */
			Responses[ffscLocal.rackid]->Text[0] = '\0';
			Result = PFC_DONE;
		}

		/* Apparently the request originated locally. If the */
		/* request is *for* a remote machine, arrange for a  */
		/* connection to it to be used for system I/O.	     */
		else if (Rack != ffscLocal.rackid) {
			int RemoteFD;
			char *respPtr = NULL;

			/* If the user is already remote, don't let them */
			/* get even more remote by routing them to yet	 */
			/* another rack, the bookkeeping is too hard.	 */
			if (User->Type == UT_REMOTE) {
				ffscMsg("Trying to connect TO remote machine "
					    "FROM a remote machine");
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR TOOREMOTE");
				return PFC_DONE;
			}

			/* Set up the remote connection */
			RemoteFD = remStartConnectCommand(User,
							  "FFSC",
							  Rack,
							  Dest);

			    /* This is a silly hack.  Basically, if the actual
			       connection succeded, but the remote MMSC was busy
			       a message that says "R1: ERROR CONNECT" can be
			       somewhat misleading.  Therefore we will just save
			       that message for the cases where we actually could
			       not establish a TCP/IP connection. */

			respPtr=strchr(users[USER_FFSC + Rack]->OutAck->Text, ':');
			
			if (RemoteFD < 0) {

			  /* If the reponse message wasn't "BUSY" */
			  if ((respPtr == NULL) ||
			      (strncmp(respPtr+1, "ERROR BUSY", 10))) {

			    sprintf(Responses[ffscLocal.rackid]->Text,
				    "ERROR CONNECT"
				    FFSC_ACK_SEPARATOR
				    "%s",
				    users[USER_FFSC + Rack]->OutAck->Text);
			  }
			  
			  else {
			    sprintf(Responses[ffscLocal.rackid]->Text,
				    "OK"
				    FFSC_ACK_SEPARATOR
				    "%s",
				    users[USER_FFSC + Rack]->OutAck->Text);

			  }
		
			  return PFC_DONE;
			}
			  
			else {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"OK;STATE BS%d FS%d M%d",
					CONF_WELCOMEMSG,
					RemoteFD,
					CONM_COPYUSER);

				Result = PFC_DONE;
			}
		}

		/* Otherwise, we have a local request to enter FFSC */
		/* mode on the local rack. That's easy.		    */
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE BS%d M%d",
				CONF_WELCOMEMSG, CONM_FFSC);

			Result = PFC_DONE;
		}

		/* Tell the router to fire up consBaseIO if necessary */
		routeCheckBaseIO = 1;
	}

	return Result;
}


/*
 * cmdRAT
 *	Processes the "RAT" command, which enters RAT mode, a variation
 *	of FFSC mode in which echoing is turned off and anything that
 *	precedes an escape character is discarded silently. Used mainly
 *	by the Remote Access Tool.
 */
int
cmdRAT(char **CmdP, user_t *User, dest_t *Dest)
{
	int NumRacks;
	rackid_t Rack = -1;

	/* Make sure this type of user is allowed to change modes */
	if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
		ffscMsg("User %s tried to enter RAT mode",
			User->Name);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR SOURCE");
		return PFC_DONE;
	}

	/* Make sure only one rack is specified in the destination */
	NumRacks = destNumRacks(Dest, &Rack);
	if (NumRacks != 1) {
		ffscMsg("Attempted to enter RAT mode on %d racks", NumRacks);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR DEST");
		return PFC_DONE;
	}

	/* If the request originated on a remote machine, */
	/* arrange to use its terminal for user I/O	  */
	if (User->Type == UT_FFSC) {
		char   Command[MAX_CONS_CMD_LEN + 1];
		user_t *LocalUser;

		/* The request better be for this rack */
		if (Rack != ffscLocal.rackid) {
			ffscMsg("Remote MMSC tried to enter RAT mode "
				    "on remote rack");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Say what's happening if desired */
		if (ffscDebug.Flags & FDBF_ROUTER) {
			ffscMsg("Remote MMSC has requested RAT mode on FD %d",
				User->InReq->FD);
		}

		/* Acquire the remote console task */
		LocalUser = userAcquireRemote(User);
		if (LocalUser == NULL) {
			/* Error should already be logged */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR BUSY");
			return PFC_DONE;
		}

		/* Generate the actual command that we will send to */
		/* the console then send it.			    */
		sprintf(Command,
			"STATE FU%d M%d",
			User->InReq->FD, CONM_RAT);
		if (userSendCommand(LocalUser,
				    Command,
				    Responses[ffscLocal.rackid]->Text)
		    != OK)
		{
			/* userSendCommand has logged the error */
			userReleaseRemote(LocalUser);
			return PFC_DONE;
		}

		/* Make sure we don't close the remote FD */
		User->InReq->FD = -1;

		/* Consider the operation a success. Note that */
		/* the console task will take care of sending  */
		/* a response to the remote side.	       */
		Responses[ffscLocal.rackid]->Text[0] = '\0';
	}

	/* Apparently the request originated locally. If the */
	/* request is *for* a remote machine, arrange for a  */
	/* connection to it to be used for system I/O.	     */
	else if (Rack != ffscLocal.rackid) {
		int RemoteFD;

		/* If the user is already remote, don't let them */
		/* get even more remote by routing them to yet	 */
		/* another rack, the bookkeeping is too hard.	 */
		if (User->Type == UT_REMOTE) {
			ffscMsg("Trying to connect TO remote machine "
				    "FROM a remote machine");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR TOOREMOTE");
			return PFC_DONE;
		}

		/* Set up the remote connection */
		RemoteFD = remStartConnectCommand(User, "RAT", Rack, Dest);
		if (RemoteFD < 0) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR CONNECT" FFSC_ACK_SEPARATOR "%s",
				users[USER_FFSC + Rack]->OutAck->Text);
			return PFC_DONE;
		}
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"OK;STATE FS%d M%d",
				RemoteFD, CONM_COPYUSER);
		}
	}

	/* Otherwise, we have a local request to enter RAT */
	/* mode on the local rack. That's easy.		   */
	else {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE M%d",
			CONM_RAT);
	}

	/* Tell the router to fire up consBaseIO if necessary */
	routeCheckBaseIO = 1;

	return PFC_DONE;
}


/*
 * cmdSTEAL
 *	Processes a "STEAL" command. Whoever owns the system console
 *	on the addressed rack is unceremoniously dumped into FFSC mode,
 *	then the current user is placed into CONSOLE mode.
 */
int
cmdSTEAL(char **CmdP, user_t *User, dest_t *Dest)
{
	/* Make sure this type of user is allowed to change modes */
	if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
		ffscMsg("User %s tried to enter MMSC mode",
			User->Name);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR SOURCE");
		return PFC_DONE;
	}

	/* ... and is authorized for this sort of thing */
	if (User->Authority < AUTH_SUPERVISOR) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* If the system console is already owned, arrange for the */
	/* current owner to be placed into FFSC mode.		   */
	if (getSystemConsole() != NULL) {
		int NewMode, NewFlags;

		/* Make sure we don't already own the system console */
		if (User->Console == getSystemConsole()) {
			ffscMsg("%s already owns the system console",
				User->Name);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR NOP");
			return PFC_DONE;
		}

		/* The other console better have a user associated with it */
		if ((getSystemConsole())->User == NULL) {
		  ffscMsg("System owned by user-less console %s",
			  (getSystemConsole())->Name);
		  sprintf(Responses[ffscLocal.rackid]->Text,
			  "ERROR INTERNAL");
		  return PFC_DONE;
		}

		/* Evict the other user */
		if ((getSystemConsole())->Mode == CONM_WATCHSYS) {
			NewMode  = CONM_IDLE;
			NewFlags = 0;
		}
		else {
			NewMode  = CONM_FFSC;
			NewFlags = CONF_STOLENMSG;
		}
		if (userChangeInputMode((getSystemConsole())->User, 
					NewMode, NewFlags)
		    != OK)
		{
			/* userChangeInputMode should have logged the error */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR INTERNAL");
			return PFC_DONE;
		}
		/* The system console "should" be available now */

		/* If the system console still isn't available, give up */
		if (getSystemConsole() != NULL) {
			ffscMsg("Timed out waiting for %s to release console",
				(getSystemConsole())->Name);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR BUSY");
			return PFC_DONE;
		}
	}

	/* Go ahead and switch to CONSOLE mode */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE FS%d M%d",getSystemPortFD(),
		CONM_CONSOLE);
	return PFC_DONE;
}


/*
 * cmdSWAPCONS:
 * This command will swap the system console from COM4 to COM6 (and vv).
 * On a single-rack partitioned system, we will have an IO6 per module
 * and we need a way to locally address the IO6 from either COM4 or COM6.
 * If debug is 0, then this command will do the necessary fd swapping, or
 * else it will generate a NOP.
 */

int 
cmdSWAPCONS(char **CmdP, user_t *User, dest_t *Dest)
{

  /* Make sure this type of user is allowed to change modes */
  if (User->Type == UT_SYSTEM  ||  User->Type == UT_DISPLAY) {
    ffscMsg("User %s tried to enter MMSC mode", User->Name);
    sprintf(Responses[ffscLocal.rackid]->Text,"ERROR SOURCE");
    Responses[ffscLocal.rackid]->State = RS_DONE;
    return PFC_DONE;
  }
  /* ... and is authorized for this sort of thing */
  if (User->Authority < AUTH_SUPERVISOR) {
    sprintf(Responses[ffscLocal.rackid]->Text,"ERROR AUTHORITY");
    Responses[ffscLocal.rackid]->State = RS_DONE;
    return PFC_DONE;
  }

  if(consDaemon2 == NULL){
    sprintf(Responses[ffscLocal.rackid]->Text,"INTERNAL ERROR: no daemon for COM6!");
    Responses[ffscLocal.rackid]->State = RS_DONE;
    return PFC_DONE;
  }

  /*
   * Figure out which system port we are talking to, the semantics of
   * the statements below is to swap system ports.
   */
  if(getSystemPortFD() == portDebug){
    ffscMsg("User console is attached to IO6 on COM6.");
    cmPrintMsg(getSystemConsole(),"User console is attached to IO6 on COM6.");
    comPortState = SYSIN_LOWER;
    goto portSwapDone;
  }

  if(getSystemPortFD() == portSystem){
    ffscMsg("User console is attached to IO6 on COM4.");
    cmPrintMsg(getSystemConsole(),"User console is attached to IO6 on COM4.");
    comPortState = SYSIN_UPPER;
    goto portSwapDone;
  }

portSwapDone:
  /* 
   * Major error -the above two conditions are invariant 
   */
  if(getSystemPortFD() < 0){
    sprintf(Responses[ffscLocal.rackid]->Text,"ERROR INTERNAL: No syscon");
    Responses[ffscLocal.rackid]->State = RS_DONE;
    ffscMsg("INTERNAL ERROR: system port file descriptor < 0");
    return PFC_DONE;    
  }

  /*
   * Race condition -we check the status bits here since may have another task
   * (the router) which is doing some OOB message processing. It would mean a 
   * lockup of the console if we switched before he was done and cleared his
   * buffers so we sit and spin here until he is complete, ensuring that the
   * swapconsole operation succeeds.
   */
#if OOB_RACE
  if(inOOBMessageProcessing())
    ffscMsg("Another task is processing OOB messages, please standby...");

  while(inOOBMessageProcessing())
    ; /* SPIN */
  ffscMsg("OOB message processing complete, swapping system port descriptors.");
#endif

  /* Go ahead and switch System FDs for console */
  sprintf(Responses[ffscLocal.rackid]->Text, 
	  "OK;STATE BS%d FS%d M%d",
	  CONF_WELCOMEMSG, 
	  getSystemPortFD(), 
	  CONM_CONSOLE);

  Responses[ffscLocal.rackid]->State = RS_DONE;    

  /* Make router fire up consBaseIO if necessary */
  routeCheckBaseIO = 1;   

  /* Re-evaluate our file descriptors */
  routeReEvalFDs = 1;    

  return PFC_DONE;
}

/*
 * cmdUNSTEAL
 *	Processes an "UNSTEAL" command. Forces the "other" console into
 *	console mode, possibly placing the current console into its
 *	base mode if necessary.
 */
int
cmdUNSTEAL(char **CmdP, user_t *User, dest_t *Dest)
{
	user_t *OtherUser;

	/* Make sure the command was issued from a legitimate user */
	if (User == userTerminal) {
		OtherUser = userAlternate;
	}
	else if (User == userAlternate) {
		OtherUser = userTerminal;
	}
	else {
		/* Not valid from this origin */
		ffscMsg("User %s attempted to use UNSTEAL command",
			User->Name);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR SOURCE");
		return PFC_DONE;
	}

	/* ...who is authorized for this sort of thing */
	if (User->Authority < AUTH_SUPERVISOR) {
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR AUTHORITY");
	  return PFC_DONE;
	}

	/* Make sure we actually own the system console */
	if (getSystemConsole() != User->Console) {
	  ffscMsg("User %s does not own the system console",
		  User->Name);
	  sprintf(Responses[ffscLocal.rackid]->Text, "ERROR UNOWNED");
	  return PFC_DONE;
	}

	/* If we own the system console, let go of it. Theoretically,  */
	/* this approach allows two consoles two be The System Console */
	/* for a brief period, so we must promise to be nice.	       */
	if (getSystemConsole() == User->Console) {
	  int idx = getSystemConsoleIndex();
	  if(idx >= 0){
	    saveConsole = consoles[idx];
	    saveConsoleIdx = idx;
	    consoles[idx] = NULL;
	  }
	}

	/* Ask the other user's console to switch to CONSOLE mode */
	if (userChangeInputMode(OtherUser, CONM_CONSOLE, CONF_UNSTEALMSG)
	    != OK)
	{
		/* userChangeInputMode should have logged the error */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR INTERNAL");
		return PFC_DONE;
	}

	/* Officially return the user to its base mode */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE BS%d FS%d M%d",
		CONF_WELCOMEMSG,
		getSystemPortFD(),
		((User->Console->BaseMode == CONM_CONSOLE)
		 ? CONM_FFSC
		 : User->Console->BaseMode));
	return PFC_DONE;
}
