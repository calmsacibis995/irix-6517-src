/*
 * route.c
 *	The FFSC Main Loop.
 *
 *	Routes commands between the switch/display task, the console
 *	and/or modem, the ELSCs and other FFSCs.
 */

#include <vxworks.h>

#include <inetlib.h>
#include <iolib.h>
#include <selectlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <syslib.h>
#include <tasklib.h>

#include "ffsc.h"

#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "log.h"
#include "misc.h"
#include "msg.h"
#include "remote.h"
#include "route.h"
#include "timeout.h"
#include "user.h"


/* Global variables */
int   routeCheckBaseIO;			/* !=0: check if IO6 needs watching */ 
int   routeRebootFFSC;			/* !=0: reboot the FFSC */
int   routeRebuildResponses;		/* !=0: rebuild Responses array */
int   routeReEvalFDs;			/* !=0: re-evaluate select'ed FDs */
msg_t *Responses[MAX_RACKS + MAX_BAYS];	/* All responses to requests */
int   outConns;       /* A number of connect tasks still
					   running */
SEM_ID outConnsSem;   /* A lock for proper updating of outConns */
SEM_ID outConnsDoneSem;  /* A sempaphore which is released when there
				are no more outstanding connect processes */


/* Static global variables - these are only global for the sake of */
/* the debugging functions. External functions should not touch!   */
static fd_set ReqFDs, ReadyFDs;
static int NumReqFDs;
static int stallTime = 0;

/* Internal functions */
static void   ProcessCommand(char *, user_t *);
static STATUS ProcessUserCommand(user_t *);
static void   ShutdownWarning(user_t *);


int
router(void)
{
	bayid_t CurrBay;
	int i;

	/* Initialize timeout timers for this task */
	if (timeoutInit() != OK) {
		ffscMsg("Router unable to proceed without timeout timers");
		return -1;
	}

	/* Arrange to do various housekeepping tasks on startup */
	routeRebuildResponses = 1;
	routeReEvalFDs = 1;

	/* Set up other loop variables */
	routeCheckBaseIO = 0;
	routeRebootFFSC = 0;
	NumReqFDs = 1;		/* Dummy value for now */

	/* Loop until we run out of things to wait for */
	while (NumReqFDs > 0) {

		/* If we have been told to reboot, do the appropriate */
		/* suicidal actions.				      */
		if (routeRebootFFSC) {

			/* Warn the main and alternate terminals */
			ShutdownWarning(userTerminal);
			ShutdownWarning(userAlternate);

			/* If the remote console is in use, give it a */
			/* warning too				      */
			if (!(userRemote->Flags & UF_AVAILABLE)) {
				ShutdownWarning(userRemote);
			}

			/* Wait a moment for things to settle down, */
			/* then proceed with an actual reboot.	    */
			stallTime = 60 * ffscLocal.rackid ;
			ffscMsg("NOTICE: stalltime=%d, rackid=%d\n",
				stallTime,
				ffscLocal.rackid);
			taskDelay(stallTime);
			ffscMsg("NOTICE: rackid=%d: calling sysReboot()...\n",
				ffscLocal.rackid);
			sysReboot();
			/* We should never make it to here */
		}

		/* Recent mode changes may have left the IO6 unattended. */
		/* If so, wake up a "babysitter" whose sole purpose is	 */
		/* to watch for input from the IO6 and deal with it.	 */
		if (routeCheckBaseIO) {
			struct timespec Delay;

			/* Give the console tasks time to settle down */

			/* Sasha 1/12/99  Unfortunatelly the delay used 
			   before, names ffscTune[TUNE_CONS_MODE_CHANGE]
			   is not enough.  Imagine this scenario: you were
			   in FFSC mode.  You typed ^E to get out.  
			   tTerminal tries to enter CONSOLE mode, but 
			   before it does that, it kicks tDaemon out of 
			   the WATCHSYS mode, using userChangeInputMode.
			   This function, after sending the command via
			   a pipe over to tDAEMON also sleeps, exactly for
			   ffscTune[TUNE_CONS_MODE_CHANGE] also.  So for
			   that period of time tTERMINAL goes to sleep,
			   this guy (tROUTER) wakes up, find the console
			   unattended, and forces tDaemon back into
			   WATCHSYS mode.  Then tTerminal wakes up and
			   find the console taken from it, so it just
			   goes back to the MMSC mode.  While it is not
			   clear why this task (tRouter) has enough time
			   to sleep and run getSystemConsole while 
			   tTerminal is sleeping, it is nevertheless so. 
			   To fix this problem we simply make this task
			   (tRouter) sleep for twice as long as tTerminal.
			   */

			Delay.tv_sec  = ((ffscTune[TUNE_CONS_MODE_CHANGE]*2)
					 / 1000000);
			Delay.tv_nsec = (((ffscTune[TUNE_CONS_MODE_CHANGE]*2)
					  % 1000000)
					 * 1000);
			nanosleep(&Delay, NULL);

			/* If there is no console task assigned to the */
			/* IO6, wake up the babysitter task.	       */
			if (getSystemConsole() == NULL) {
			  ffscMsg("DAEMON task needed after mode change");
				if (portDaemon < 0  &&
				    userChangeInputMode(userDaemon,
							CONM_WATCHSYS,
							0) != OK) 
				{
					ffscMsg("WARNING: Unable to start "
						    "daemon monitor task");
				}
			}
			else if (ffscDebug.Flags & FDBF_ROUTER) {
				ffscMsg("No need for DAEMON task after mode "
					    "change");
			}

			/* Don't repeat until asked to */
			routeCheckBaseIO = 0;
		}

		/* Build an array that has all the important ACK msg_t's */
		/* if necessary.					 */
		if (routeRebuildResponses) {
			for (i = 0;  i < MAX_RACKS;  ++i) {
				if (i == ffscLocal.rackid) {
					Responses[i] =
					    users[USER_LOCAL_FFSC]->OutAck;
				}
				else {
					Responses[i] =
					    users[USER_FFSC + i]->OutAck;
				}
			}
			for (i = 0;  i < MAX_BAYS;  ++i) {
				Responses[MAX_RACKS + i] = ELSC[i].OutAck;
			}

			/* Don't repeat until asked to */
			routeRebuildResponses = 0;
		}

		/* See if we need to re-evaluate the select'ed FDs */
		if (routeReEvalFDs) {

			/* Say what's happening if desired */
			if (ffscDebug.Flags & FDBF_ROUTER) {
				ffscMsg("Router re-evaluating selected FDs");
			}

			/* Start with a clean slate */
			FD_ZERO(&ReqFDs);
			NumReqFDs = 0;

			/* Select the FD for the listener task */
			if (L2RMsgFD >= 0) {
				FD_SET(L2RMsgFD, &ReqFDs);
				++NumReqFDs;
			}

			/* Select the FDs for active users */
			for (i = 0;  i < NUM_USERS;  ++i) {
				if (users[i] != NULL  &&
				    (users[i]->Flags & UF_ACTIVE))
				{
					FD_SET(users[i]->InReq->FD, &ReqFDs);
					++NumReqFDs;
				}
			}

			/* Select FDs for the ELSC's, if we own them */
			for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
				if ((ELSC[CurrBay].Owner == EO_ROUTER)  &&
				    ELSC[CurrBay].FD >= 0)
				{
					FD_SET(ELSC[CurrBay].FD, &ReqFDs);
					++NumReqFDs;
				}
			}

			/* Remember we have done this */
			routeReEvalFDs = 0;

			/* If there is nothing left to wait for, bail out */
			if (NumReqFDs < 1) {
				break;
			}
		}

		/* Wait for something interesting to happen */
		ReadyFDs = ReqFDs;
		if (select(FD_SETSIZE, &ReadyFDs, NULL, NULL, NULL) == ERROR){
			ffscMsgS("Error on select in FFSCmain");
			break;
		}

		/* Check for unsolicited data from the ELSCs */
		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {

			/* Only worry about ELSCs ready for reading */
			if (ELSC[CurrBay].FD >= 0  &&
			    FD_ISSET(ELSC[CurrBay].FD, &ReadyFDs))
			{

				/* Make sure we still own the ELSC */
				if (!(ELSC[CurrBay].Owner == EO_ROUTER)) {
					if (ffscDebug.Flags & FDBF_ROUTER) {
						ffscMsg("ROUTER no longer owns"
							    " MSC %c",
							ELSC[CurrBay].Name);
					}
					routeReEvalFDs = 1;
					continue;
				}

				/* Go handle whatever it gives us */
				if (elscProcessInput(&ELSC[CurrBay]) != OK) {
					ffscMsg("No longer listening to MSC "
						    "%c",
						ELSC[CurrBay].Name);
					if (ELSC[CurrBay].Owner == EO_ROUTER) {
						ELSC[CurrBay].Owner = EO_NONE;
					}
					routeReEvalFDs = 1;
				}
			}
		}

		/* Check for commands from the various users */
		for (i = 0;  i < NUM_USERS;  ++i) {
			if (users[i] != NULL  &&
			    (users[i]->Flags & UF_ACTIVE) &&
			    FD_ISSET(users[i]->InReq->FD, &ReadyFDs))
			{
				if (users[i]->InAck != NULL) {
					msgClear(users[i]->InAck);
				}
				if (ProcessUserCommand(users[i]) != OK) {
					routeReEvalFDs = 1;
				}
			}
		}

		/* Check for a remote command request */
		if (L2RMsgFD >= 0  &&  FD_ISSET(L2RMsgFD, &ReadyFDs)) {
			int	 ConnFD;
			rackid_t RemoteRack;
			user_t	 *CurrUser;
			userinfo_t RemUserInfo;

			if (ffscDebug.Flags & FDBF_REMOTE)
				ffscMsg("Calling \"remAcceptConnection\" for pipe L2RMsgFD");

			/* Go accept the incoming connection */
			ConnFD = remAcceptConnection(L2RMsgFD,
						     &RemoteRack,
						     &RemUserInfo);
			if (ConnFD == -2) {
				/* Listen socket is hosed */
				routeReEvalFDs = 1;
				continue;
			}
			else if (ConnFD < 0) {
				/* False alarm */
				continue;
			}

			/* Make sure the other side has a proper rack ID */
			if (RemoteRack > MAX_RACKS) {
				ffscMsg("Rejecting remote request - invalid "
					    "rack ID");
				close(ConnFD);
				continue;
			}

			/* Set up the message buffers with the new FD */
			CurrUser = users[USER_FFSC + RemoteRack];
			CurrUser->Flags |= UF_ACTIVE;
			CurrUser->InReq->FD = ConnFD;
			CurrUser->InAck->FD = ConnFD;
			msgClear(CurrUser->InAck);

			/* Copy over the remote user's credentials */
			userInsertUserInfo(CurrUser, &RemUserInfo);

			/* Process the command. Since we will be closing */
			/* the FD anyway, we can ignore the return code. */
			ProcessUserCommand(CurrUser);

			/* Clean up. Notice that ConnFD may have been	*/
			/* "stolen" from us for long term use, so only	*/
			/* close it if it is still present in InReq	*/
			if (CurrUser->InReq->FD >= 0) {
				close(CurrUser->InReq->FD);
			}
			CurrUser->InReq->FD = -1;
			CurrUser->InAck->FD = -1;
			CurrUser->Flags &= ~UF_ACTIVE;
		}
	}

	/* We should "never" make it to this point */
	ffscMsg("WARNING: route task is exiting");
	return -1;
}


/*
 * routeDoAdminCommand
 *	Processes an administrative (i.e. internally generated, not from
 *	a user) FFSC command. These are done in the context of the ADMIN
 *	user and return no response.
 */
void
routeDoAdminCommand(char *Command)
{
	(void) ProcessCommand(Command, userAdmin);
}


/*
 * routeProcessELSCCommand
 *	Processes a command intended for execution on a local ELSC.
 *	The appropriate ELSC OutAck buffers are updated accordingly.
 */
void
routeProcessELSCCommand(const char *Command, user_t *User, dest_t *Dest)
{
	bayid_t CurrBay;

	/* If the local rack is not selected, we need not continue */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return;
	}

	/* Send the command to each selected bay in the local rack. */
	/* If BAY ALL was specified but the ELSC is not selected,   */
	/* then it must be offline - respond accordingly.	    */
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		if (destLocalBayIsSelected(Dest, CurrBay)) {
		  	(void) elscSendPassword(CurrBay, User->Authority); 
			elscSendCommand(CurrBay, Command);
		}
		else if (destAllBaysSelected(Dest, ffscLocal.rackid)) {
			sprintf(ELSC[CurrBay].OutAck->Text,
				"OFFLINE");
			ELSC[CurrBay].OutAck->State = RS_DONE;
		}
	}
}



/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * ProcessUserCommand
 *	Process an incoming FFSC command from the specified user_t
 *	and return an appropriate response. Returns ERROR if the
 *	FD contained in the input msg_t is no longer valid, OK otherwise.
 */
STATUS
ProcessUserCommand(user_t *User)
{
	/* Prepare the incoming msg_t for a new message */
	msgClear(User->InReq);
	User->InReq->State = RS_NEED_FFSCMSG;

	/* Go read a message from the requestor */
	if (msgRead(User->InReq, ffscTune[TUNE_RI_REQ]) != OK) {
		/* msgRead already logged the error */
		return ERROR;
	}

	/* Make sure we got a complete message */
	if (User->InReq->State == RS_ERROR) {
		ffscMsg("Invalid message from %s: %s",
			User->InReq->Name, User->InReq->Text);
		return OK;
	}
	else if (User->InReq->State != RS_DONE) {
		ffscMsg("Incomplete message from %s: %s",
			User->InReq->Name, User->InReq->Text);
		return OK;
	}

	/* If the user is a display, log the message */
	if (User->Type == UT_DISPLAY) {
		char Message[MAX_FFSC_CMD_LEN + 40];

		sprintf(Message,
			"%s COMMAND:  %s",
			User->Name, User->InReq->Text);
		logWriteLine(logDisplay, Message);
	}

	/* Mention the message text if desired */
	if (ffscDebug.Flags & FDBF_ROUTER) {
		ffscMsg("Data from %s: %s",
			User->InReq->Name, User->InReq->Text);
	}

	/* Go on to the Real Work elsewhere */
	ProcessCommand(User->InReq->Text, User);

	return OK;
}


/*
 * ProcessCommand
 *	Process the specified command and return an appropriate response.
 */
void
ProcessCommand(char *CmdString, user_t *User)
{
	bayid_t    CurrBay;
	char	   *Command;
	char	   *Last;
	char	   FinalResponse[MAX_FFSC_RESP_LEN + 1];
	dest_t	   CmdDest;
	fd_set	   ReadFDs, ReadyFDs;
	int	   CurrResp;
	int	   Outstanding;
	int	   PFCResult;
	int	   RemoteMsgs = 0;
	int	   TimeoutUSecs;
	rackid_t   CurrRack;
	struct  timeval Timeout;



	/* Start off with an empty response */
	FinalResponse[0] = '\0';

	/* If "auto ffsc mode" is enabled, then an empty command is the */
	/* same as typing "rack local ffsc". Otherwise, it's equivalent */
	/* to "rack local nop".						*/
	if (CmdString[0] == '\0') {
		if (User->Options & UO_AUTOFFSCMODE) {
			strcpy(CmdString, "RACK LOCAL MMSC");
		}
		else {
			strcpy(CmdString, "RACK LOCAL NOP");
		}
	}

	/* Parse the destination portion of the command (if any) */
	Command = (char *) destParse(CmdString,
				     &CmdDest,
				     (User->Options & UO_ROBMODE));
	if (Command == NULL) {
		ffscMsg("Invalid destination in command: %s", CmdString);

		sprintf(FinalResponse, "R%ld:ERROR DEST", ffscLocal.rackid);
		goto Done;
	}


	/* If no destination was specified, use the default */
	if (CmdDest.Flags & DESTF_NONE) {
		if (destParse(User->DfltDest.String,
			      &CmdDest,
			      (User->Options & UO_ROBMODE)) == NULL  ||
		    (CmdDest.Flags & DESTF_NONE))
		{
			ffscMsg("No default destination for command: %s",
				CmdString);

			sprintf(FinalResponse,
				"R%ld:ERROR NODEST",
				ffscLocal.rackid);
			goto Done;
		}

		CmdDest.Flags |= DESTF_DEFAULT;

	}

	/* Skip over extraneous white space in the command */
	Command += strspn(Command, " \t");

	/* If there is no command, all we need to do is change the */
	/* default destination. Do this by faking a DEST command.  */
	if (Command[0] == '\0') {
		strcpy(Command, "DEST");
	}

	/* Clear the array of responses */
	for (CurrResp = 0;  CurrResp < (MAX_RACKS + MAX_BAYS);  ++CurrResp)
	{
		if (Responses[CurrResp]->State != RS_NONE) {
			msgClear(Responses[CurrResp]);
			Responses[CurrResp]->State = RS_NONE;
		}
	}

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_ROUTER) {	
		ffscMsg("About to run command \"%s\" on destination %s",
			Command, CmdDest.String);
	}

	/* Try to process the command as an "MMSC" command */

	PFCResult = cmdProcessFFSCCommand(Command, User, &CmdDest);

	if (PFCResult < 0) {
		ffscMsg("Unable to process MMSC command");
		sprintf(FinalResponse, "R%ld:ERROR CMD", ffscLocal.rackid);
		goto Done;
	}

	/* If there is no further processing required, set up a response */
	/* and bail out now.						 */
	if (PFCResult == PFC_DONE) {
   	if (ffscDebug.Flags & FDBF_TMPDEBUG) 
     	 ffscMsg("User Options = %x", User->Options);
		if (Responses[ffscLocal.rackid]->Text[0] == '\0') {
			if ((User->Type == UT_DISPLAY)  || (User->Options & UO_NORESPONSE)){
				char Message[60];

				sprintf(Message,
					"%s RESPONSE: <none>",
					User->Name);
				logWriteLine(logDisplay, Message);
			}

			return;
		}

		sprintf(FinalResponse,
			"R%ld:%s",
			ffscLocal.rackid,
			Responses[ffscLocal.rackid]->Text);
		goto Done;
	}

	/* See if the command needs to be forwarded to any selected */
	/* remote racks.					    */


	/* This part is non-trivial.  The goal is to try to start all
	   the remote connections, which is done in
	   remStartRemoteCommand, in parallel, yet not proceed with
	   this (parent) task, until all the spawned tasks with calls
	   to remStartRemoteCommand have finished.  Essentially, what
	   is needed here is a fork-join primitive, which is available
	   in some advanced languages, and OSes.  Unfortunatelly
	   VxWorks isn't one of those.  This will be done is a
	   somewhat backwards manner here.  The variable outConns will
	   keep track of the number of outstanding  remStartRemoteCommand 
	   tasks.  Each one of these tasks,
	   before exiting, will consult outConns variable to check if it's
	   the last one.  If so, it will then release the outConnsDoneSem
	   semaphore,  which this process will be blocking on.  If the task
	   about to exit is not the last one, indicated by outConns > 0,
	   it will simply decrement outConns.  It's important that the check
	   and update of outConns be done atomically.  Therefore another 
	   semaphore, outConnsSem is used for mutual exclusion.  */


	if (PFCResult != PFC_WAITRESP) {

	  /* Initialize the semaphores if they haven't been before */
	  if (outConnsSem == NULL) {
	    if ((outConnsSem = semBCreate(SEM_Q_FIFO, SEM_FULL)) == NULL) {
	      ffscMsg("Unable to create outConnsSem semaphore");
	    }
	  }

	  if (outConnsDoneSem == NULL) {
	    if ((outConnsDoneSem = semBCreate(SEM_Q_FIFO, SEM_EMPTY)) == NULL) {
	      ffscMsg("Unable to create outConnsDoneSem semaphore");
	    }
	  }

	  /* Just in case this semaphore was released, attempt to grab it
	     but without timeout.  Theoretically this should never succeed,
	     as this semaphored should not be available now.  This is simply
	     defensive code.
	     */

	  semTake(outConnsDoneSem, NO_WAIT); /* since it should never succeed,
						no point in checking for error.
						*/
					     

	  /* First we need to count up how many remote connections we are
	     going to start.  

	     */
	    
	  outConns = 0;
	  
	  for (CurrRack = 0; CurrRack < MAX_RACKS; ++CurrRack) {
	    if ((destRackIsSelected(&CmdDest, CurrRack)) &&
		(CurrRack != ffscLocal.rackid)) {
	      /* At this point this is the only thread messing with
		 this variable, therefore it is not necessar to involve
		 mutexes.
		 */
	      outConns++;
	    }
	  } 

	  /* If we're not going to spawn any tasks, release the outConnsDoneSem
	     now, since vacously we're done! */

	  if (!outConns) {
	    if (semGive(outConnsDoneSem) != OK) {
	      ffscMsg("Failed to release outConnsDoneSem semaphore");
	    }
	  }
	    


	  for (CurrRack = 0;  CurrRack < MAX_RACKS;  ++CurrRack) {
	    if (!destRackIsSelected(&CmdDest, CurrRack)) {
	      continue;
	    }

	    if (CurrRack == ffscLocal.rackid) {

	      /* Only non-FFSC commands need	*/
	      /* to be processed locally	*/
	      if (PFCResult == PFC_NOTFFSC) {
		routeProcessELSCCommand(Command,
					User,
					&CmdDest);
	      }
	    }
	    else {
	      char task_name[10];
			  
	      sprintf(task_name, "tRmtcmd%d", CurrRack);
	      if (taskSpawn(task_name, ROUTER_PRI, 0, 5000,
			     remStartRemoteCommand,
			     (int) User, 
			     (int) Command, 
			     CurrRack, 
			     (int) &CmdDest,
			     0,0,0,0,0,0) == ERROR) {
				     
		ffscMsg("Error spawning remote request as a separate task");
		remStartRemoteCommand(User, Command, CurrRack, &CmdDest);
	      }
	    }
	  }

	  /* This is the Poor Man's join.  Essentially this semaphore will
	     become available only after all the spawned tasks have 
	     finished.
	     */

	  if (semTake(outConnsDoneSem, WAIT_FOREVER) != OK) {
	    ffscMsg("Failed to take outConnsDoneSem semaphore");
	  }

	}

	/* If a response is not expected then close the connections
	 * and jump to the bottom of the function to exit */
	if (User->InAck == NULL) {
		/* Close the various remote connections we have opened */
		for (CurrResp = 0;  CurrResp < MAX_RACKS;  ++CurrResp) {
			msg_t *CurrAck;

			CurrAck = users[USER_FFSC + CurrResp]->OutAck;
			if (CurrAck->FD >= 0) {
				if (ffscDebug.Flags & FDBF_ROUTER) 
					ffscMsg("Closing slot %d, no response expected", CurrResp);
				close(CurrAck->FD);
				CurrAck->FD = -1;
			}
		}
		goto Done;
	}

	/* Determine which responses we need to wait for */
	FD_ZERO(&ReadFDs);
	Outstanding = 0;
	for (CurrResp = 0;  CurrResp < (MAX_RACKS + MAX_BAYS);  ++CurrResp)
	{
		if (Responses[CurrResp]->State != RS_NONE  &&
		    Responses[CurrResp]->State != RS_DONE  &&
		    Responses[CurrResp]->State != RS_ERROR &&
		    Responses[CurrResp]->FD >= 0)
		{
			if (ffscDebug.Flags & FDBF_ROUTER) {
				ffscMsg("Waiting for response in slot %d",
					CurrResp);
			}

			FD_SET(Responses[CurrResp]->FD, &ReadFDs);
			++Outstanding;

			if (Responses[CurrResp]->Type == RT_REMOTE) {
				++RemoteMsgs;
			}
		}
	}

	/* Wait for any outstanding responses to arrive (or timeout) */
	TimeoutUSecs = ((RemoteMsgs > 0)
			? ffscTune[TUNE_RW_RESP_REMOTE]
			: ffscTune[TUNE_RW_RESP_LOCAL]);
	Timeout.tv_sec  = TimeoutUSecs / 1000000;
	Timeout.tv_usec = TimeoutUSecs % 1000000;
	while (Outstanding > 0) {
		int Result;

		/* Wait for something to happen */
		ReadyFDs = ReadFDs;
		Result = select(FD_SETSIZE, &ReadyFDs, NULL, NULL, &Timeout);
		if (Result < 0) {
			ffscMsgS("select failed in ProcessCommand");
			break;
		}
		else if (Result == 0) {
			/* Timed out */
			break;
		}

		/* Look for a ready FD */
		for (CurrResp = 0;
		     CurrResp < (MAX_RACKS + MAX_BAYS);
		     ++CurrResp)
		{
			/* Don't bother checking ineligible or unready	*/
			/* slots					*/
			if (Responses[CurrResp]->State == RS_NONE  ||
			    Responses[CurrResp]->FD < 0  ||
			    !FD_ISSET(Responses[CurrResp]->FD, &ReadyFDs))
			{
				continue;
			}

			/* If the message is complete (or the FD is now */
			/* ineligible) stop waiting for this response.	*/
			if ((msgRead(Responses[CurrResp],
				     ffscTune[TUNE_RI_RESP]) != OK) ||
			    Responses[CurrResp]->State == RS_DONE  ||
			    Responses[CurrResp]->State == RS_ERROR)
			{
				if (Responses[CurrResp]->State == RS_DONE  &&
				    (ffscDebug.Flags & FDBF_ROUTER))
				{
					ffscMsg("Received response in slot %d",
						CurrResp);
				}

				FD_CLR(Responses[CurrResp]->FD, &ReadFDs);
				--Outstanding;
			}
		}
	}

	/* Close the various remote connections we have opened */
	for (CurrResp = 0;  CurrResp < MAX_RACKS;  ++CurrResp) {
		msg_t *CurrAck;

		CurrAck = users[USER_FFSC + CurrResp]->OutAck;
		if (CurrAck->FD >= 0) {
			close(CurrAck->FD);
			CurrAck->FD = -1;
		}
	}

	/* Sift through the responses looking for any invalid or timed	*/
	/* out entries and fake up some appropriate error messages	*/
	for (CurrResp = 0;  CurrResp < (MAX_RACKS + MAX_BAYS);  ++CurrResp) {
		msg_t *Msg;

		/* Set up handy synonym */
		Msg = Responses[CurrResp];

		/* Check for an invalid or timed-out response */
		if (Msg->State != RS_NONE  &&  Msg->State != RS_DONE) {
			char *Error = ((Msg->State == RS_ERROR)
				       ? "ERROR BADRESP (ProcessCommand)"
				       : "ERROR TIMEOUT (ProcessCommand)");

			if (CurrResp < MAX_RACKS) {
				sprintf(Msg->Text, "R%d:%s", CurrResp, Error);
			}
			else {
				strcpy(Msg->Text, Error);
			}

			ffscMsg("Failed response in slot %d, state=%d: %s",
				CurrResp, Msg->State, Error);
		}
	}

	/* Concatenate the various response strings into a single message */
	for (CurrResp = 0;  CurrResp < MAX_RACKS;  ++CurrResp) {

		/* If we happen to be at the slot for the local rack,	*/
		/* look at the bay slots instead (unless we actually	*/
		/* have a local response)				*/
		if (CurrResp == ffscLocal.rackid  &&
		    Responses[CurrResp]->State == RS_NONE)
		{
			for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
				char Header[9];
				char LogMsg[MAX_ELSC_RESP_LEN + 40];
				msg_t *Msg;

				Msg = ELSC[CurrBay].OutAck;

				if (Msg->State != RS_NONE) {
					if (FinalResponse[0] != '\0') {
						strcat(FinalResponse,
						       FFSC_ACK_SEPARATOR);
					}

					elscCheckResponse(&ELSC[CurrBay],
							  Msg->Text);

					Last = strchr(Msg->Text,
						      FFSC_ACK_END_CHAR);
					if (Last != NULL) {
						*Last = '\0';
					}

					if (ELSC[CurrBay].Log != NULL) {
						sprintf(LogMsg,
							"<<RESPONSE: %s>>",
							Msg->Text);
						logWriteLine(ELSC[CurrBay].Log,
							     LogMsg);
					}

					sprintf(Header,
						"R%ld%c:",
						ffscLocal.rackid,
						ELSC[CurrBay].Name);
					strcat(FinalResponse, Header);
					strcat(FinalResponse, Msg->Text);
				}
			}

			continue;
		}

		/* Otherwise, use the remote response if present */
		else if (Responses[CurrResp]->State != RS_NONE) {
			if (FinalResponse[0] != '\0') {
				strcat(FinalResponse, FFSC_ACK_SEPARATOR);
			}
			Last = strrchr(Responses[CurrResp]->Text,
				       FFSC_ACK_END_CHAR);
			if (Last != NULL) {
				*Last = '\0';
			}

			strcat(FinalResponse, Responses[CurrResp]->Text);
		}
	}


	/* Come here when we are finished processing the command */
    Done:

	/* Log the response if necessary */
	if (User->Type == UT_DISPLAY) {
		char Message[MAX_FFSC_RESP_LEN + 40];

		sprintf(Message, "%s RESPONSE: %s", User->Name, FinalResponse);
		logWriteLine(logDisplay, Message);
	}

	/* If no response is required, we are through */
	if (User->InAck == NULL) {
   	if (ffscDebug.Flags & FDBF_TMPDEBUG)
			ffscMsg("No Response expected, User->InAck = NULL");
		return;
	}

	/* Send the acknowledgement to the requestor */
	User->InAck->State = RS_SEND;
	sprintf(User->InAck->Text, "%s%s", FinalResponse, FFSC_ACK_END);
   if (ffscDebug.Flags & FDBF_TMPDEBUG)
		ffscMsg("Response: %s", User->InAck->Text);
	
	if (msgWrite(User->InAck, ffscTune[TUNE_RO_RESP]) != OK) {
		/* msgWrite should have logged the error already */
		;
	}
	else if (User->InAck->State == RS_SENDING) {
		ffscMsg("Incomplete message written to %s",
			User->InAck->Name);
	}
	else if (User->InAck->State != RS_DONE) {
		ffscMsg("Unable to send message to %s - S=%d",
			User->InAck->Name, User->InAck->State);
	}
}


/*
 * ShutdownWarning
 *	Tells the console associated with the specified user that the
 *	FFSC is about to go down, so it should make the proper arrangements.
 */
void
ShutdownWarning(user_t *User)
{
	userSendCommand(User, "SHUTDOWN", NULL);
}


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * routerResponses
 *	Dump one or all of the response msg_t's
 */
STATUS
routerResponses(int Arg)
{
	char Name[20];

	if (Arg >= 0  &&  Arg < (MAX_RACKS + MAX_BAYS)) {
		sprintf(Name, "Responses[%d]", Arg);
		msgShow(Responses[Arg], 0, Name);
	}
	else if (Arg == -1) {
		int i;

		for (i = 0;  i < (MAX_RACKS + MAX_BAYS);  ++i) {
			sprintf(Name, "Responses[%d]", i);
			msgShow(Responses[i], 0, Name);
			ffscMsg("");
		}
	}
	else {
		ffscMsg("Specify value from 0 to %d, or -1 to show all",
			MAX_RACKS + MAX_BAYS - 1);
	}

	return OK;
}


/*
 * routerStat
 *	Show what we know about the router task
 */
STATUS
routerStat(void)
{
	int i;

	ffscMsg("Router status:");
	ffscMsg("    CheckBaseIO      %d", routeCheckBaseIO);
	ffscMsg("    RebootFFSC       %d", routeRebootFFSC);
	ffscMsg("    RebuildResponses %d", routeRebuildResponses);
	ffscMsg("    ReEvalFDs        %d", routeReEvalFDs);

	ffscMsgN("    %d ReqFDs @%p:", NumReqFDs, &ReqFDs);
	for (i = 0;  i < FD_SETSIZE;  ++i) {
		if (FD_ISSET(i, &ReqFDs)) {
			ffscMsgN(" %d", i);
		}
	}
	ffscMsg("");

	ffscMsgN("    ReadyFDs @%p:", &ReadyFDs);
	for (i = 0;  i < FD_SETSIZE;  ++i) {
		if (FD_ISSET(i, &ReadyFDs)) {
			ffscMsgN(" %d", i);
		}
	}
	ffscMsg("");

	return OK;
}

#endif  /* !PRODUCTION */












