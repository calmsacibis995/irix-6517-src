/*
 * remote.c
 *	Functions for handling connections to and from remote FFSCs
 */

#include <vxworks.h>

#include <inetlib.h>
#include <iolib.h>
#include <signal.h>
#include <socklib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tasklib.h>

#include "ffsc.h"

#include "dest.h"
#include "identity.h"
#include "misc.h"
#include "msg.h"
#include "remote.h"
#include "route.h"
#include "timeout.h"
#include "user.h"


/* Internal functions */
int remListener(void);


/*
 * remAcceptConnection
 *	Accepts a remote connection provided by the specified listener pipe
 *	and returns an FD for it, or <0 if something is wrong. The caller's
 *	credentials are stored in the specified userinfo_t if available,
 *	otherwise the default credentials are stored there.
 */
int
remAcceptConnection(int ListenerFD, rackid_t *RemoteRack, userinfo_t *UserInfo)
{
	char IPAddrStr[INET_ADDR_LEN];
	identity_t *Ident;
	int SockAddrSize;
	int ConnFD;
	int Result;
	struct sockaddr_in ClientAddr;

	/* In the trivial case of having no FD, we're done already */
	if (ListenerFD < 0) {
		return -1;
	}

	/* It's also an error to omit the UserInfo pointer */
	if (UserInfo == NULL) {
		ffscMsg("remAcceptConnection called with no userinfo_t ptr");
		return -1;
	}

	/* Read the next message from the listener, which should be */
	/* the actual FD of an open remote connection		    */
	Result = timeoutRead(ListenerFD,
			     &ConnFD,
			     sizeof(ConnFD),
			     ffscTune[TUNE_RI_LISTENER]);
	if (Result < 0) {
		if (errno == EINTR) {
			ffscMsg("Timed out waiting for message from listener");
		}
		else {
			ffscMsgS("Error reading from listener");
		}
		return -1;
	}

	if (Result != sizeof(ConnFD)) {
		ffscMsg("Short read from listener (%d bytes)", Result);
		return -1;
	}

	/* Get the "name" of the remote side */
	SockAddrSize = sizeof(ClientAddr);
	if (getpeername(ConnFD,
			(struct sockaddr *) &ClientAddr,
			&SockAddrSize) != OK)
	{
		ffscMsgS("Unable to get name of remote client");
		close(ConnFD);
		return -1;
	}

	/* Find out the remote side's identity. Ignore anyone who hasn't */
	/* been properly introduced to us (via addrd).			 */
	Ident = identFindByIPAddr(&ClientAddr.sin_addr);
	if (Ident == NULL) {
		inet_ntoa_b(ClientAddr.sin_addr, IPAddrStr);
		ffscMsg("Rejecting connection from %s - unknown machine");
		close(ConnFD);
		return -1;
	}

	/* Save the remote side's rack ID if desired */
	if (RemoteRack != NULL) {
		*RemoteRack = Ident->rackid;
	}

	/* If the remote side is capable of sending over user info, */
	/* read it over as the first item of data on the connection */
	if (Ident->capabilities & CAP_USERINFO) {
		Result = timeoutRead(ConnFD,
				     UserInfo,
				     sizeof(userinfo_t),
				     ffscTune[TUNE_RI_CONNECT]);
		if (Result < 0) {
			if (errno == EINTR) {
				ffscMsg("Timed out waiting for user info "
					    "from remote side");
			}
			else {
				ffscMsgS("Error reading user info from "
					     "remote side");
			}
			return -1;
		}

		if (Result != sizeof(userinfo_t)) {
			ffscMsg("Short read of user info (%d bytes)", Result);
			return -1;
		}
	}

	/* Otherwise, just copy over default stuff */
	else {
		bcopy((char *) &userDefaultUserInfo,
		      (char *) UserInfo,
		      sizeof(userinfo_t));
	}

	/* The remote side's NVRAM ID should be censored */
	UserInfo->NVRAMID = -1;

	/* Say what we have done if desired */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		char RackIDStr[RACKID_STRLEN];

		/* Print a nice message */
		inet_ntoa_b(ClientAddr.sin_addr, IPAddrStr);
		ffscRackIDToString(Ident->rackid, RackIDStr);
		ffscMsg("Accepted connection from rack %s (%s) on FD %d",
			RackIDStr, IPAddrStr, ConnFD);
	}

	/* That's all */
	return ConnFD;
}


/*
 * remInit
 *	Initialize the remote request listener
 */
STATUS
remInit(void)
{
	if (taskSpawn("tListen", 100, 0, 20000, remListener,
 		      0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
	{
		ffscMsg("Unable to spawn remote request listener task");
		return ERROR;
	}

	return OK;
}


/*
 * remListener
 *	The remote request listener: waits for connections from remote
 *	FFSCs, then wakes the router process with a pipe message when
 *	one comes in. This runs as a separate task, so it "never" exits.
 */
int
remListener(void)
{
	int ConnFD;
	int ListenFD;
	int SockAddrSize;
	struct sockaddr_in ClientAddr;
	struct sockaddr_in ServerAddr;

	/* Set up our address */
	bzero((char *) &ServerAddr, sizeof(ServerAddr));

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port   = htons((short) ffscTune[TUNE_LISTEN_PORT]);
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Create a socket */
	ListenFD = socket(AF_INET, SOCK_STREAM, 0);
	if (ListenFD < 0) {
		ffscMsgS("Unable to create listen socket");
		return ERROR;
	}

	/* Bind an address to the socket */
	if (bind(ListenFD, (struct sockaddr *) &ServerAddr, sizeof(ServerAddr))
	    < 0)
	{
		ffscMsgS("Unable to bind address to listen socket");
		return ERROR;
	}

	/* Prepare to listen for requests */
	if (listen(ListenFD, ffscTune[TUNE_LISTEN_MAX_CONN]) < 0) {
		ffscMsgS("Listen socket unable to listen");
		return ERROR;
	}

	/* Say what we are doing if desired */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		ffscMsg("Listening for remote requests on FD %d", ListenFD);
	}

/*BLM*/
#if 0
	/* Enable this task for timeouts */
	if (timeoutInit() != OK) {
		ffscMsg("Listener cannot continue without timeout timers");
		return ERROR;
	}

	/* If a timeout occurs, we will longjmp back to here */
	if (timeoutOccurred() != 0) {
		ffscMsg("Timed out writing to router - listener restarting");
		timeoutCancel();
	}
#endif
	/* Accept incoming connections "forever" */
	while (ListenFD >= 0) {
		int Result;

		/* Wait for the next connection request */
		SockAddrSize = sizeof(ClientAddr);
		ConnFD = accept(ListenFD,
				(struct sockaddr *) &ClientAddr,
				&SockAddrSize);
		if (ConnFD < 0) {
			ffscMsgS("Unable to accept remote connection");
			continue;
		}
		else if (ffscDebug.Flags & FDBF_REMOTE) 
			ffscMsg("Accepted connection on FD %d", ListenFD);

/*BLM*/
#if 0
		/* Arrange for the timeout signal */
		if (timeoutSet(ffscTune[TUNE_LO_ROUTER], "posting router")
		    != OK)
		{
			/* timeoutSet should have logged the error already */
			return ERROR;
		}
#endif

		/* Send the resulting file descriptor # to the router */
/*BLM*/
#if 0
		timeoutEnable();
#endif
		Result = write(L2RMsgFD, (char *) &ConnFD, sizeof(ConnFD));
#if 0
		timeoutDisable();
		timeoutCancel();
#endif

		/* Complain if we couldn't talk to the router */
		if (Result != sizeof(ConnFD)) {
			ffscMsgS("Listener unable to send message to router");
			continue;
		}
	}

	/* No way to get here! */
	ffscMsg("Listener task dying!");
	return -1;
}


/*
 * remOpenConnection
 *	Opens a remote connection to the FFSC on the specified rack and
 *	returns an FD to the resulting FD, or <0 if something is wrong.
 */
int
remOpenConnection(rackid_t RackID, const userinfo_t *LclUserInfo)
{
	identity_t *ServerIdent;
	int ConnFD;
	int Result;
	int SockAddrSize;
	struct sockaddr_in ServerAddr;
	struct timeval connect_timeout;

	/* In the trivial case of having no rack, we're done already */
	if (RackID == RACKID_UNASSIGNED) {
		ffscMsg("Attempted to open connection to RACKID_UNASSIGNED");
		return -1;
	}

	/* Obtain the remote FFSC's identity info */
	ServerIdent = identFindByRackID(RackID);
	if (ServerIdent == NULL) {
		ffscMsg("Don't know of any rack with rack ID %ld", RackID);
		return -1;
	}

	/* Create a socket */

	/* This is where losage occurs, for some reason cannot
	   create more than X number of sockets.  X seems to vary.
	   */

	ConnFD = socket(AF_INET, SOCK_STREAM, 0);
	if (ConnFD < 0) {
		ffscMsgS("Unable to create socket for remote request");
		return -1;
	}

	/* Set up the address of the remote FFSC */
	SockAddrSize = sizeof(ServerAddr);
	bzero((char *) &ServerAddr, SockAddrSize);

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port   = htons((short) ffscTune[TUNE_LISTEN_PORT]);
	ServerAddr.sin_addr   = ServerIdent->ipaddr;

/*BLM*/
#if 0
	/* If a timeout occurs, we will longjmp back to here */
	if (timeoutOccurred() != 0) {
		ffscMsg("Timed out waiting to connect to remote MMSC");
		timeoutCancel();
		close(ConnFD);
		return -1;
	}

	/* Arrange for the timeout signal */
	if (timeoutSet(ffscTune[TUNE_CONNECT_TIMEOUT], "connect") != OK) {
		/* timeoutSet should have logged the error already */
		close(ConnFD);
		return -1;
	}

#endif
	/* Connect to the remote FFSC */
/*BLM*/
#if 0
	timeoutEnable();
#endif

	connect_timeout.tv_sec = ffscTune[TUNE_CONNECT_TIMEOUT] / 1000000;
	connect_timeout.tv_usec = ffscTune[TUNE_CONNECT_TIMEOUT] % 1000000;	
	Result = connectWithTimeout(ConnFD,
			 (struct sockaddr *) &ServerAddr,
			 SockAddrSize,
			 &connect_timeout);
/*BLM*/
#if 0
	timeoutDisable();
	timeoutCancel();
#endif

	if (Result < 0) {
		ffscMsgS("Unable to connect to remote MMSC");
		close(ConnFD);
		return -1;
	}

	/* If the remote side is capable of receiving user info,    */
	/* send it over as the first item of data on the connection */
	if (ServerIdent->capabilities & CAP_USERINFO) {
		Result = timeoutWrite(ConnFD,
				      LclUserInfo,
				      sizeof(userinfo_t),
				      ffscTune[TUNE_RO_REMOTE]);
		if (Result < 0) {
			if (errno == EINTR) {
				ffscMsg("Timed out sending user info "
					    "to remote side");
			}
			else {
				ffscMsgS("Error sending user info to "
					     "remote side");
			}
			close(ConnFD);
			return -1;
		}

		if (Result != sizeof(userinfo_t)) {
			ffscMsg("Short write of user info (%d bytes)", Result);
			close(ConnFD);
			return -1;
		}
	}

	/* Say what we did if desired */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		char IPAddrStr[INET_ADDR_LEN];

		inet_ntoa_b(ServerAddr.sin_addr, IPAddrStr);
		ffscMsg("Opened connection to rack %ld at address %s on FD %d",
			RackID, IPAddrStr, ConnFD);
	}

	return ConnFD;
}


/*
 * remReceiveData
 *	Receives data from a remote FFSC over the specified FD into
 *	the specified buffer for the specified length. Essentially
 *	a glorified read call. If something goes wrong, the local
 *	response string is updated and ERROR is returned.
 */
STATUS
remReceiveData(int RemoteFD, char *BufAddr, int Length)
{
	int BytesRemaining;
	int Offset;
	int ReadLen;

	/* Read the data. This may indeed take more than one */
	/* read operation, so prepare accordingly.	     */
	Offset = 0;
	BytesRemaining = Length;
	while (BytesRemaining > 0) {
		ReadLen = timeoutRead(RemoteFD,
				      BufAddr + Offset,
				      BytesRemaining,
				      ffscTune[TUNE_RI_RESP]);
		if (ReadLen < 0) {
			if (errno == EINTR) {
				ffscMsg("Timed out receiving data on FD %d",
					RemoteFD);
			}
			else {
				ffscMsgS("Failed receiving data from FD %d",
					 RemoteFD);
			}

			close(RemoteFD);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR READ (remReceiveData)");
			return ERROR;
		}

		Offset += ReadLen;
		BytesRemaining -= ReadLen;
	}

	/* Say what happened if desired */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		ffscMsg("Obtained %d bytes from FD %d", Length, RemoteFD);
	}

	return OK;
}


/*
 * remSendData
 *	Sends the data in the specified buffer to the specified user,
 *	which is assumed to be on a remote FFSC.
 */
STATUS
remSendData(user_t *User, char *Data, int Length)
{
	int BytesRemaining;
	int Offset;
	int WriteLen;

	/* Say what's happening if desired */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		ffscMsg("Sending %d bytes of data to FD %d",
			Length, User->InReq->FD);
	}

	/* Tell the other side that we are ready to transmit data */
	sprintf(User->InAck->Text, "READY %d" FFSC_ACK_END, Length);
	User->InAck->State = RS_SEND;
	if (msgWrite(User->InAck, ffscTune[TUNE_RO_RESP]) != OK ||
	    User->InAck->State != RS_DONE)
	{
		ffscMsg("Unable to complete connection to %s - S=%d",
			User->InAck->Name, User->InAck->State);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR CONNECT (remSendData)");
		return ERROR;
	}

	/* At this stage, we start transferring binary data, */
	/* so we can no longer use msgWrite.		     */

	/* Now write the data. This may indeed take more than one */
	/* write operation, so prepare accordingly.		  */
	Offset  = 0;
	BytesRemaining = Length;
	while (BytesRemaining > 0) {
		WriteLen = timeoutWrite(User->InAck->FD,
					Data + Offset,
					BytesRemaining,
					ffscTune[TUNE_RO_REMOTE]);
		if (WriteLen < 0) {
			if (errno == EINTR) {
				ffscMsg("Timed out writing data to FD %d",
					User->InAck->FD);
			}
			else {
				ffscMsgS("Data write to FD %d for failed",
					 User->InAck->FD);
			}

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR WRITE: remSendData");
			return ERROR;
		}

		Offset += WriteLen;
		BytesRemaining -= WriteLen;
	}

	return OK;
}


/*
 * remStartConnectCommand
 *	Initiates a command that is intended to start a persistent connection
 *	with the specified rack/bay and returns the FD of the resulting
 *	socket if successful. If unsuccessful, -1 is returned and the
 *	response is stored in the appropriate Responses slot.
 */
int
remStartConnectCommand(const user_t *LclUser,
		       const char *Command,
		       rackid_t RackID,
		       dest_t *Dest)
{
	char *Ptr;
	int RemoteFD;
	user_t *RemUser = users[USER_FFSC + RackID];

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_ROUTER) {
		char RackIDStr[RACKID_STRLEN];

		ffscRackIDToString(ffscLocal.rackid, RackIDStr);
		ffscMsg("About to start connection command \"%s\" to rack %s",
			Command, RackIDStr);
	}

	/* Initialize the response buffer */
	msgClear(RemUser->OutAck);
	RemUser->OutAck->State = RS_NONE;

	/* Go start up the remote command */
	remStartRemoteCommand(LclUser, Command, RackID, Dest);

	/* If something has gone wrong already, bail out */
	if (RemUser->OutAck->State == RS_DONE  ||
	    RemUser->OutAck->State == RS_ERROR ||
	    RemUser->OutAck->FD < 0)
	{
		return -1;
	}

	/* Wait for the response */
	if (ffscDebug.Flags & FDBF_ROUTER) {
		ffscMsg("Waiting for connection completion...");
	}

	if (msgRead(RemUser->OutAck,
		    ffscTune[TUNE_RI_CONNECT]) != OK  ||
	    RemUser->OutAck->State != RS_DONE)
	{
		char *Error = ((RemUser->OutAck->State == RS_ERROR)
			       ? "ERROR BADRESP (remStartConnectCommand)"
			       : "ERROR TIMEOUT (remStartConnectCommand)");
		char RackIDStr[RACKID_STRLEN];

		sprintf(RemUser->OutAck->Text, "R%ld:%s", RackID, Error);

		ffscRackIDToString(RackID, RackIDStr);
		ffscMsg("Failed connect to rack %s, state=%d: %s",
			RackIDStr, RemUser->OutAck->State, Error);

		close(RemUser->OutAck->FD);
		RemUser->OutAck->FD = -1;
		return -1;
	}
	else if (ffscDebug.Flags & FDBF_ROUTER) {
		ffscMsg("Connection response on FD %d: %s",
			RemUser->OutAck->FD, RemUser->OutAck->Text);
	}

	/* See if the connection request was successful */
	Ptr = strchr(RemUser->OutAck->Text, ':');
	if (Ptr == NULL) {
		Ptr = RemUser->OutAck->Text;
	}
	else {
		++Ptr;
	}

	/* Losage here */

	if (strncmp(Ptr, "READY", 5) != 0) {
		ffscMsg("Connection request rejected: %s", Ptr);

		close(RemUser->OutAck->FD);
		RemUser->OutAck->FD = -1;
		return -1;
	}

	/* We were apparently successful. Don't let anyone close the FD. */
	RemoteFD = RemUser->OutAck->FD;
	RemUser->OutAck->FD = -1;
	return RemoteFD;
}


/*
 * remStartDataRequest
 *	Initiate a request to a remote FFSC for some kind of data.
 *	A connection is opened and an FD for it is returned (or <0
 *	if something goes wrong). If a pointer to an integer is
 *	specified, the requested data's length will be stored in it.
 */
int
remStartDataRequest(const user_t *LclUser,
		    const char *Command,
		    rackid_t Rack,
		    dest_t *Dest,
		    int *DataLen)
{
	int RemoteFD;

	/* Open up a long-term connection */
	RemoteFD = remStartConnectCommand(LclUser, Command, Rack, Dest);
	if (RemoteFD < 0) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR CONNECT (remStartDataRequest)" FFSC_ACK_SEPARATOR "%s",
			users[USER_FFSC + Rack]->OutAck->Text);
		return -1;
	}

	/* If the data length is not needed, we are finished */
	if (DataLen == NULL) {
		return RemoteFD;
	}

	/* Parse the data length from the response */
	if (sscanf(users[USER_FFSC + Rack]->OutAck->Text,
		   "READY %d",
		   DataLen) != 1)
	{
		ffscMsg("Invalid response to data request: %s",
			users[USER_FFSC + Rack]->OutAck->Text);

		close(RemoteFD);

		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR CONNECT (remStartDataRequest)");
		return -1;
	}

	return RemoteFD;
}


/*
 * remStartRemoteCommand
 *	Initiate a command bound for a remote FFSC. The appropriate user
 *	will be updated with the (possibly pending) results of the command.
 */
void
remStartRemoteCommand(const user_t *LclUser,
		      const char *Command,
		      rackid_t RackID,
		      dest_t *Dest)
{
	char BayList[MAX_BAYS + 1];
	int  ConnFD;
	bayid_t CurrBay;
	user_t *RemUser = users[USER_FFSC + RackID];
	userinfo_t LclUserInfo;

	int i;




	/* Set up a userinfo with the caller's credentials */
	userExtractUserInfo(LclUser, &LclUserInfo);

	/* Set up whether or not a response is expected */
	if (LclUser->InAck == NULL)
		LclUser->Options |= UO_NORESPONSE;

	/* Prepare a connection to the remote FFSC */
	ConnFD = remOpenConnection(RackID, &LclUserInfo);
	if (ConnFD < 0) {
		sprintf(RemUser->OutAck->Text, "R%ld:ERROR CONNECT (remStartRemoteCommand)", RackID);
		RemUser->OutAck->State = RS_DONE;

		/* Return */
		goto Done;
	}
	RemUser->OutReq->FD = ConnFD;
	RemUser->OutAck->FD = ConnFD;

	/* Build a list of bays */
	if (destAllBaysSelected(Dest, RackID)) {
		strcpy(BayList, "*");
	}
	else {
		int index;

		index = 0;
		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
			if (destBayIsSelected(Dest, RackID, CurrBay)) {
				BayList[index] = ffscBayName[CurrBay];
				++index;
			}
		}
		BayList[index] = '\0';
	}

	/* Prepare our outgoing request */
	msgClear(RemUser->OutReq);
	sprintf(RemUser->OutReq->Text, "R . B %s %s", BayList, Command);
	RemUser->OutReq->State = RS_SEND;

	/* Say what we are about to do */
	if (ffscDebug.Flags & FDBF_REMOTE) {
		ffscMsg("About to send command \"%s\" to rack %ld",
			RemUser->OutReq->Text, RackID);
	}

	/* Write the command to the Term task */
	if (msgWrite(RemUser->OutReq, ffscTune[TUNE_RO_REMOTE]) != OK) {
		/* msgWrite should have logged the error already */
		sprintf(RemUser->OutAck->Text, "R%ld:ERROR SEND", RackID);
		goto Error;
	}

	/* Make sure everything got written */
	if (RemUser->OutReq->State != RS_DONE) {
		ffscMsg("Unable to send message to MMSC %ld - State=%d",
			RackID, RemUser->OutReq->State);
		sprintf(RemUser->OutAck->Text, "R%ld:ERROR SEND", RackID);
		goto Error;
	}

	/* Prepare for a response from the remote FFSC */
	if (!(LclUser->Options & UO_NORESPONSE))
		RemUser->OutAck->State = RS_NEED_FFSCACK;
	else
		RemUser->OutAck->State = RS_NONE;
	goto Done;

	/* Come here if we are done with the connection already */
    Error:
	RemUser->OutAck->State = RS_DONE;

	close(RemUser->OutReq->FD);
	RemUser->OutReq->FD = -1;
	RemUser->OutAck->FD = -1;

     Done:

	/* For a full explanation of what the purpose of the 2 semaphores
	   and outConns variable please see the comments in route.c, around
	   the code that calls this functions
	   */


	/* First lock the outConns variable */
	if (semTake(outConnsSem, WAIT_FOREVER) != OK) {
	  ffscMsg("failed to take outConnsSem semaphore");
	}

	/* Now decrease the count */
        outConns--;

	/* If this was the last thread, release the other semaphore */
	if (!outConns) {
	  if (semGive(outConnsDoneSem) != OK) {
	    ffscMsg("failed to release outConnsDone semaphore");
	  }
	}

	/* And finally release the outConns variable */
	if(semGive(outConnsSem) != OK) {
	  ffscMsg("failed to release outConnsSem semaphore");
	}

	return;
}

