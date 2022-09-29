/*
 * user.c
 *	Functions for handling FFSC users and their contexts
 */

#include <vxworks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "console.h"
#include "dest.h"
#include "misc.h"
#include "msg.h"
#include "nvram.h"
#include "route.h"
#include "user.h"



/* Constants */
const userinfo_t userDefaultUserInfo = {
	UIF_VALID | UIF_DESTVALID,	/* Control flags */
	-1,				/* NVRAM ID */
	0,				/* Option flags */
	AUTH_SERVICE,			/* Authority level */
	{ 0, 0, 0, 0 },			/* reserved */
	DEST_DEFAULT			/* Default destination */
};


/* Global variables */
passinfo_t userPasswords;	/* System passwords */
user_t	   *users[NUM_USERS];	/* Users of this FFSC */


/* Internal functions */
static void userInitDfltDest(user_t *, const char *);


/*
 * userAcquireRemote
 *	Acquires a (the) remote console and returns a user_t that has
 *	been assigned to it, bearing the same user context as the
 *	specified user_t. Returns NULL if no remote console task is
 *	available.
 */
user_t *
userAcquireRemote(user_t *RemUser)
{
	/* Make sure the remote console task on our side */
	/* is available for use.			 */
	if (!(userRemote->Flags & UF_AVAILABLE)) {
		ffscMsg("Rejected remote connection request - "
			    "no console available");
		return NULL;
	}

	/* Mark the remote console as unavailable */
	/* XXX Does this need lock protection? */
	userRemote->Flags &= ~UF_AVAILABLE;
	userRemote->Flags |= UF_ACTIVE;

	/* Tell the router to consider waiting on this user's console */
	routeReEvalFDs = 1;

	/* Copy over the juicy parts of the user's context */
	userRemote->Options   = RemUser->Options;
	userRemote->Authority = RemUser->Authority;
	userInitDfltDest(userRemote, RemUser->DfltDest.String);

	return userRemote;
}


/*
 * userChangeInputMode
 *	Force an arbitrary user out of its current input mode and
 *	into a different one. Typically used when trying to steal
 *	a single-user resource (e.g. an ELSC, the IO6) or for
 *	manipulating a console task not associated with a terminal
 *	(e.g. consDaemon, consRemote).
 */
STATUS
userChangeInputMode(user_t *User, int NewMode, int NewFlags)
{
	char Command[MAX_CONS_CMD_LEN + 1];
	struct timespec Delay;

	/* Make sure the user actually has an associated console */
	if (User->Console == NULL) {
		ffscMsg("User %s has no console", User->Name);
		return ERROR;
	}

	/* Tell the user's console task to switch to the new mode */
	sprintf(Command, "STATE FS%d BS%d M%d", 
		getSystemPortFD(),
		NewFlags, 
		NewMode);

	if (userSendCommand(User, Command, NULL) != OK) {
		ffscMsg("Unable to change mode of %s to %d",
			User->Name, NewMode);
		return ERROR;
	}

	/* Give it time to do so */
	Delay.tv_sec  = ffscTune[TUNE_CONS_MODE_CHANGE] / 1000000;
	Delay.tv_nsec = ((ffscTune[TUNE_CONS_MODE_CHANGE] % 1000000)
			 * 1000);
	nanosleep(&Delay, NULL);

	return OK;
}


/*
 * userExtractUserInfo
 *	Extract the user parameters from the specified user_t and
 *	store them in the specified userinfo_t.
 */
void
userExtractUserInfo(const user_t *User, userinfo_t *UserInfo)
{
	bzero((char *) UserInfo, sizeof(userinfo_t));

	UserInfo->Flags	    = UIF_VALID;
	UserInfo->NVRAMID   = User->NVRAMID;
	UserInfo->Options   = User->Options;
	UserInfo->Authority = User->Authority;

	if (!(User->DfltDest.Flags & DESTF_NONE)) {
		strcpy(UserInfo->DfltDest, User->DfltDest.String);
		UserInfo->Flags |= UIF_DESTVALID;
	}
}


/*
 * userInit
 *	Initialize the users array
 */
STATUS
userInit(void)
{
	int i;
	userinfo_t UserInfo;

	/* Obtain the system passwords */
	if (nvramRead(NVRAM_PASSINFO, &userPasswords) != OK) {

		/* Password info was not found in NVRAM, so set up */
		/* default info instead.			   */
		bzero((char *) &userPasswords, sizeof(passinfo_t));

		/* Try to write the console info out (but ignore */
		/* any errors that may occur)			 */
		nvramWrite(NVRAM_PASSINFO, &userPasswords);
	}

	/* The local users are each subtly different, so we initialize	*/
	/* them individually rather than in a loop.			*/

	/* User "DAEMON" (i.e. the pseudo-console on the Base I/O port) */
	users[USER_DAEMON] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_DAEMON] == NULL) {
		ffscMsgS("Unable to allocate storage for user DAEMON");
		return ERROR;
	}

	strcpy(users[USER_DAEMON]->Name, "DAEMON");
	users[USER_DAEMON]->Type      = UT_SYSTEM;
	users[USER_DAEMON]->Flags     = 0;
	users[USER_DAEMON]->NVRAMID   = -1;
	users[USER_DAEMON]->Options   = 0;
	users[USER_DAEMON]->Authority = AUTH_SERVICE;	/* XXX Gfx only? */

	destClear(&users[USER_DAEMON]->DfltDest);

	users[USER_DAEMON]->InReq   = msgNew(MAX_FFSC_CMD_LEN,
					     C2RReqFDs[CONS_DAEMON],
					     RT_SYSTEM,
					     "CB2Rr");
	users[USER_DAEMON]->InAck   = msgNew(MAX_FFSC_RESP_LEN,
					     C2RAckFDs[CONS_DAEMON],
					     RT_SYSTEM,
					     "CB2Ra");
	users[USER_DAEMON]->OutReq  = msgNew(MAX_CONS_CMD_LEN,
					     R2CReqFDs[CONS_DAEMON],
					     RT_SYSTEM,
					     "R2CBr");
	users[USER_DAEMON]->OutAck  = NULL;
	users[USER_DAEMON]->Console = consDaemon;

	consDaemon->User = users[USER_DAEMON];


	/* User "TERMINAL" (i.e. the console on the TTY port) */
	if (nvramRead(NVRAM_USERTERM, &UserInfo) != OK  ||
	    !(UserInfo.Flags & UIF_VALID))
	{
		bcopy((char *) &userDefaultUserInfo,
		      (char *) &UserInfo,
		      sizeof(userinfo_t));
		UserInfo.NVRAMID = NVRAM_USERTERM;
		nvramWrite(UserInfo.NVRAMID, &UserInfo);
	}



	users[USER_TERMINAL] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_TERMINAL] == NULL) {
		ffscMsgS("Unable to allocate storage for user TERMINAL");
		return ERROR;
	}

	strcpy(users[USER_TERMINAL]->Name, "TERMINAL");
	users[USER_TERMINAL]->Type      = UT_CONSOLE;
	users[USER_TERMINAL]->Flags     = UF_ACTIVE;

	userInsertUserInfo(users[USER_TERMINAL], &UserInfo);


	users[USER_TERMINAL]->InReq   = msgNew(MAX_FFSC_CMD_LEN,
					       C2RReqFDs[CONS_TERMINAL],
					       RT_CONSOLE,
					       "CT2Rr");
	users[USER_TERMINAL]->InAck   = msgNew(MAX_FFSC_RESP_LEN,
					       C2RAckFDs[CONS_TERMINAL],
					       RT_CONSOLE,
					       "CT2Ra");
	users[USER_TERMINAL]->OutReq  = msgNew(MAX_CONS_CMD_LEN,
					       R2CReqFDs[CONS_TERMINAL],
					       RT_CONSOLE,
					       "R2CTr");
	users[USER_TERMINAL]->OutAck  = NULL;
	users[USER_TERMINAL]->Console = consTerminal;

	consTerminal->User = users[USER_TERMINAL];


	/* User "ALTERNATE" (i.e. the console on the modem port) */
	if (nvramRead(NVRAM_USERALT, &UserInfo) != OK  ||
	    !(UserInfo.Flags & UIF_VALID))
	{
		bcopy((char *) &userDefaultUserInfo,
		      (char *) &UserInfo,
		      sizeof(userinfo_t));
		UserInfo.NVRAMID = NVRAM_USERALT;
		nvramWrite(UserInfo.NVRAMID, &UserInfo);
	}

	users[USER_ALTERNATE] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_ALTERNATE] == NULL) {
		ffscMsgS("Unable to allocate storage for user ALTERNATE");
		return ERROR;
	}

	strcpy(users[USER_ALTERNATE]->Name, "ALTERNATE");
	users[USER_ALTERNATE]->Type      = UT_CONSOLE;
	users[USER_ALTERNATE]->Flags     = UF_ACTIVE;

	userInsertUserInfo(users[USER_ALTERNATE], &UserInfo);

	users[USER_ALTERNATE]->InReq   = msgNew(MAX_FFSC_CMD_LEN,
						C2RReqFDs[CONS_ALTERNATE],
						RT_CONSOLE,
						"CA2Rr");
	users[USER_ALTERNATE]->InAck   = msgNew(MAX_FFSC_RESP_LEN,
						C2RAckFDs[CONS_ALTERNATE],
						RT_CONSOLE,
						"CA2Ra");
	users[USER_ALTERNATE]->OutReq  = msgNew(MAX_CONS_CMD_LEN,
						R2CReqFDs[CONS_ALTERNATE],
						RT_CONSOLE,
						"R2CAr");
	users[USER_ALTERNATE]->OutAck  = NULL;
	users[USER_ALTERNATE]->Console = consAlternate;

	consAlternate->User = users[USER_ALTERNATE];


	/* User "REMOTE" (the current owner of the remote console, as	*/
	/* opposed to a remote FFSC)					*/
	users[USER_REMOTE] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_REMOTE] == NULL) {
		ffscMsgS("Unable to allocate storage for user REMOTE");
		return ERROR;
	}

	strcpy(users[USER_REMOTE]->Name, "REMOTE");
	users[USER_REMOTE]->Type      = UT_REMOTE;
	users[USER_REMOTE]->Flags     = UF_AVAILABLE;
	users[USER_REMOTE]->NVRAMID   = -1;
	users[USER_REMOTE]->Options   = 0;
	users[USER_REMOTE]->Authority = AUTH_NONE;

	destClear(&users[USER_REMOTE]->DfltDest);

	users[USER_REMOTE]->InReq   = msgNew(MAX_FFSC_CMD_LEN,
					     C2RReqFDs[CONS_REMOTE],
					     RT_CONSOLE,
					     "CR2Rr");
	users[USER_REMOTE]->InAck   = msgNew(MAX_FFSC_RESP_LEN,
					     C2RAckFDs[CONS_REMOTE],
					     RT_CONSOLE,
					     "CR2Ra");
	users[USER_REMOTE]->OutReq  = msgNew(MAX_CONS_CMD_LEN,
					     R2CReqFDs[CONS_REMOTE],
					     RT_CONSOLE,
					     "R2CRr");
	users[USER_REMOTE]->OutAck  = NULL;
	users[USER_REMOTE]->Console = consRemote;

	consRemote->User = users[USER_REMOTE];


	/* User "DISPLAY" (i.e. the console on the modem port) */
	users[USER_DISPLAY] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_DISPLAY] == NULL) {
		ffscMsgS("Unable to allocate storage for user DISPLAY");
		return ERROR;
	}

	strcpy(users[USER_DISPLAY]->Name, "DISPLAY");
	users[USER_DISPLAY]->Type    = UT_DISPLAY;
	users[USER_DISPLAY]->Flags   = UF_ACTIVE;

	userInsertUserInfo(users[USER_DISPLAY], &userDefaultUserInfo);

	users[USER_DISPLAY]->InReq   = msgNew(MAX_FFSC_CMD_LEN,
					      D2RReqFD,
					      RT_DISPLAY,
					      "D2Rr");
	users[USER_DISPLAY]->InAck   = NULL;
	users[USER_DISPLAY]->OutReq  = msgNew(MAX_DISP_CMD_LEN,
					      DispReqFD,
					      RT_DISPLAY,
					      "DispReq");
	users[USER_DISPLAY]->OutAck  = msgNew(MAX_DISP_RESP_LEN,
					      D2RReqFD,
					      RT_DISPLAY,
					      "D2Ra");
	users[USER_DISPLAY]->Console = NULL;


	/* User "ADMIN" (dummy user for adminstrative commands) */
	users[USER_ADMIN] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_ADMIN] == NULL) {
		ffscMsgS("Unable to allocate storage for user ADMIN");
		return ERROR;
	}

	strcpy(users[USER_ADMIN]->Name, "ADMIN");
	users[USER_ADMIN]->Type      = UT_ADMIN;
	users[USER_ADMIN]->Flags     = 0;
	users[USER_ADMIN]->NVRAMID   = -1;
	users[USER_ADMIN]->Options   = 0;
	users[USER_ADMIN]->Authority = AUTH_SERVICE;

	destClear(&users[USER_ADMIN]->DfltDest);

	users[USER_ADMIN]->InReq   = NULL;
	users[USER_ADMIN]->InAck   = NULL;
	users[USER_ADMIN]->OutReq  = NULL;
	users[USER_ADMIN]->OutAck  = msgNew(MAX_LOCAL_RESP_LEN,
					    -1,
					    RT_LOCAL,
					    "LOCAL");
	users[USER_ADMIN]->Console = NULL;


	/* User "LOCAL" (dummy user for the local rack in multi-rack cmds) */
	users[USER_LOCAL_FFSC] = (user_t *) malloc(sizeof(user_t));
	if (users[USER_LOCAL_FFSC] == NULL) {
		ffscMsgS("Unable to allocate storage for user LOCAL");
		return ERROR;
	}

	strcpy(users[USER_LOCAL_FFSC]->Name, "LOCAL");
	users[USER_LOCAL_FFSC]->Type      = UT_LOCAL;
	users[USER_LOCAL_FFSC]->Flags	  = 0;
	users[USER_LOCAL_FFSC]->NVRAMID   = -1;
	users[USER_LOCAL_FFSC]->Options   = 0;
	users[USER_LOCAL_FFSC]->Authority = AUTH_NONE;

	destClear(&users[USER_LOCAL_FFSC]->DfltDest);

	users[USER_LOCAL_FFSC]->InReq   = NULL;
	users[USER_LOCAL_FFSC]->InAck   = NULL;
	users[USER_LOCAL_FFSC]->InReq   = NULL;
	users[USER_LOCAL_FFSC]->OutAck  = msgNew(MAX_LOCAL_RESP_LEN,
						 -1,
						 RT_LOCAL,
						 "LOCAL");
	users[USER_LOCAL_FFSC]->Console = NULL;


	/* Set up a remote user for each possible rack. Note that we even  */
	/* set one up for the local rack (even though it doesn't get used) */
	/* so that it will be there in case our rack ID is changed.	   */
	for (i = 0;  i < MAX_RACKS;  ++i) {
		char   BufName[MI_NAME_LEN+1];
		char   UserName[USER_NAME_LEN+1];
		user_t *CurrUser;

		/* Generate an appropriate name */
		sprintf(UserName, "RACK_%d", i);

		/* Allocate storage for the current user_t */
		CurrUser = (user_t *) malloc(sizeof(user_t));
		if (CurrUser == NULL) {
			ffscMsgS("Unable to allocate storage for user %s",
				 UserName);
			return ERROR;
		}
		users[USER_FFSC + i] = CurrUser;

		/* Initialize the easy parts */
		strcpy(CurrUser->Name, UserName);
		CurrUser->Type      = UT_FFSC;
		CurrUser->Flags	    = 0;
		CurrUser->NVRAMID   = -1;
		CurrUser->Options   = 0;
		CurrUser->Authority = AUTH_NONE;
		CurrUser->Console   = NULL;

		/* Initialize the default destination */
		destClear(&CurrUser->DfltDest);

		/* Set up the various message buffers */
		sprintf(BufName, "F%d2Rr", i);
		CurrUser->InReq   = msgNew(MAX_FFSC_CMD_LEN,
					   -1,
					   RT_REMOTE,
					   BufName);

		sprintf(BufName, "F%d2Ra", i);
		CurrUser->InAck   = msgNew(MAX_FFSC_RESP_LEN,
					   -1,
					   RT_REMOTE,
					   BufName);

		sprintf(BufName, "R2F%dr", i);
		CurrUser->OutReq  = msgNew(MAX_FFSC_CMD_LEN,
					   -1,
					   RT_REMOTE,
					   BufName);

		sprintf(BufName, "R2F%da", i);
		CurrUser->OutAck = msgNew(MAX_FFSC_RESP_LEN,
					  -1,
					  RT_REMOTE,
					  BufName);
	}

	return OK;
}


/*
 * userInsertUserInfo
 *	Install the user parameters from the specified userinfo_t into
 *	the specified user_t.
 */
void
userInsertUserInfo(user_t *User, const userinfo_t *UserInfo)
{
	if (!(UserInfo->Flags & UIF_VALID)) {
		return;
	}

	User->NVRAMID   = UserInfo->NVRAMID;
	User->Options   = UserInfo->Options;
	User->Authority = UserInfo->Authority;

	if (UserInfo->Flags & UIF_DESTVALID) {
		userInitDfltDest(User, UserInfo->DfltDest);

	}
	else {

		destClear(&User->DfltDest);
	}
}


/*
 * userReleaseRemote
 *	Releases the remote console task & user for use by another
 *	remote FFSC.
 */
void
userReleaseRemote(user_t *User)
{
	/* At this stage, there is only one remote user. Make sure */
	/* that's who is being released.			   */
	if (User != userRemote) {
		ffscMsg("userReleaseRemote called with non-remote %p", User);
		return;
	}

	/* Mark the remote user as available */
	User->Flags &= ~UF_ACTIVE;
	User->Flags |= UF_AVAILABLE;

	/* Tell the router to reconsider waiting on this user's console */
	routeReEvalFDs = 1;

	/* Clear out remnants of the remote user's context */
	User->Options   = 0;
	User->Authority = AUTH_NONE;
	destClear(&User->DfltDest);

	return;
}


/*
 * userSendCommand
 *	Send a command to the console task associated with the specified
 *	user. If something goes wrong, a response message is stored in
 *	the specified string (if non-NULL) and ERROR is returned. Otherwise
 *	the response string is left untouched and OK is returned.
 */
STATUS
userSendCommand(user_t *User, const char *Cmd, char *Response)
{
	/* Initialize the outgoing request */
	msgClear(User->OutReq);
	User->OutReq->State = RS_SEND;
	strcpy(User->OutReq->Text, Cmd);

	/* Write the command to its console task */
	if (msgWrite(User->OutReq, ffscTune[TUNE_RO_CONS]) != OK) {
		/* msgWrite should have logged the error */
		if (Response != NULL) {
			sprintf(Response, "ERROR WRITE: userSendCommand");
		}
		return ERROR;
	}

	/* Make sure everything got written */
	if (User->OutReq->State != RS_DONE) {
		ffscMsg("Unable to send message to %s - S=%d",
			User->Name, User->OutReq->State);

		if (Response != NULL) {
			sprintf(Response, "ERROR CONSOLE");
		}

		return ERROR;
	}

	return OK;
}


/*
 * userUpdatePasswords
 *	Save the system passwords from userPasswords in NVRAM. Useful
 *	after they have been modified in some way.
 */
STATUS
userUpdatePasswords(void)
{
	return nvramWrite(NVRAM_PASSINFO, &userPasswords);
}


/*
 * userUpdateSaveInfo
 *	Save any persistent information about the specified user_t in
 *	NVRAM (if this user keeps persistent info)
 */
STATUS
userUpdateSaveInfo(user_t *User)
{
	userinfo_t UserInfo;

	/* Don't bother if this user doesn't have an NVRAM ID */
	if (User->NVRAMID < 0) {
		return OK;
	}

	/* Build a userinfo_t from the user_t */
	bzero((char *) &UserInfo, sizeof(userinfo_t));

	UserInfo.Flags     = UIF_VALID;
	UserInfo.NVRAMID   = User->NVRAMID;
	UserInfo.Options   = User->Options;
	UserInfo.Authority = User->Authority;

	if (!(User->DfltDest.Flags & DESTF_NONE)) {
		strcpy(UserInfo.DfltDest, User->DfltDest.String);
		UserInfo.Flags |= UIF_DESTVALID;
	}

	/* Write the resulting userinfo_t to NVRAM */
	return nvramWrite(UserInfo.NVRAMID, &UserInfo);
}




/*----------------------------------------------------------------------*/
/*									*/
/*			  PRIVATE FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * userInitDfltDest
 *	Initialize the DfltDest field of a new user_t
 */
void
userInitDfltDest(user_t *User, const char *DestString)
{
	destClear(&User->DfltDest);
	destParse(DestString, &User->DfltDest, 0);
	User->DfltDest.Flags |= DESTF_DEFAULT;
}



#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

#include "elsc.h"


/*
 * passwords
 *	Display the known passwords
 */
STATUS
passwords(void)
{
	bayid_t CurrBay;
	char PassBuf[MAX_PASSWORD_LEN + 1];

	ffscMsg("System passwords:");

	if (strlen(userPasswords.Supervisor) > 0) {
		strncpy(PassBuf, userPasswords.Supervisor, MAX_PASSWORD_LEN);
		PassBuf[MAX_PASSWORD_LEN] = '\0';

		ffscMsg("    Supervisor: \"%s\"", PassBuf);
	}
	else {
		ffscMsg("    No supervisor password.");
	}

	if (strlen(userPasswords.Service) > 0) {
		strncpy(PassBuf, userPasswords.Service, MAX_PASSWORD_LEN);
		PassBuf[MAX_PASSWORD_LEN] = '\0';

		ffscMsg("    Service: \"%s\"", PassBuf);
	}
	else {
		ffscMsg("    No service password.");
	}

	ffscMsg("");

	ffscMsg("ELSC passwords:");
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		if (strlen(ELSC[CurrBay].Password) > 0) {
			ffscMsg("    Bay %c: \"%s\"",
				ELSC[CurrBay].Name, ELSC[CurrBay].Password);
		}
		else {
			ffscMsg("    No password for bay %c",
				ELSC[CurrBay].Name);
		}
	}

	return OK;
}


/*
 * userList
 *	Dump one or all of the user_t's
 */
STATUS
userList(int Arg)
{
	char Name[20];

	if (Arg >= 0  &&  Arg < NUM_USERS) {
		sprintf(Name, "users[%d]", Arg);
		userShow(users[Arg], 0, Name);
	}
	else if (Arg == -1) {
		int i;

		for (i = 0;  i < NUM_USERS;  ++i) {
			sprintf(Name, "users[%d]", i);
			userShow(users[i], 0, Name);
			ffscMsg("");
		}
	}
	else {
		ffscMsg("Specify value from 0 to %d, or -1 to show all",
			NUM_USERS);
	}

	return OK;
}


/*
 * userShow
 *	Show information about the specified user_t
 */
STATUS
userShow(user_t *User, int Offset, char *Title)
{
	char Indent[80];

	if (User == NULL) {
		ffscMsg("Usage: userShow <addr> [<offset> [<title>]]");
		return OK;
	}

	sprintf(Indent, "%*s", Offset, "");

	if (Title == NULL) {
		ffscMsg("%suser_t %s (%p):", Indent, User->Name, User);
	}
	else {
		ffscMsg("%s%s - user_t %s (%p):",
			Indent,
			Title, User->Name, User);
	}

	ffscMsg("%s    Type %d   Flags 0x%08x   Options 0x%08x",
		Indent,
		User->Type, User->Flags, User->Options);

	if (User->Console != NULL) {
		ffscMsg("%s    Console @%p \"%s\"",
			Indent,
			User->Console, User->Console->Name);
	}

	if (User->InReq != NULL) {
		msgShow(User->InReq, Offset + 4, "InReq");
	}

	if (User->InAck != NULL) {
		msgShow(User->InAck, Offset + 4, "InAck");
	}

	if (User->OutReq != NULL) {
		msgShow(User->OutReq, Offset + 4, "OutReq");
	}

	if (User->OutAck != NULL) {
		msgShow(User->OutAck, Offset + 4, "OutAck");
	}

	return OK;
}

#endif  /* !PRODUCTION */
