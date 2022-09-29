/*
 * addrd.c
 *	FFSC Address Daemon
 */

#include <vxworks.h>

#include <etherlib.h>
#include <inetlib.h>
#include <iolib.h>
#include <ioslib.h>
#include <lstlib.h>
#include <pipedrv.h>
#include <selectlib.h>
#include <semlib.h>
#include <syslib.h>
#include <tasklib.h>
#include <wdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ffsc.h"

#include "addrd.h"
#include "identity.h"
#include "misc.h"
#include "net.h"
#include "nvram.h"


/* Internal constants */
#define ADDRD_ELECTION_LIST_TIMEOUT (1 * sysClkRateGet())
#define ADDRD_ELECTION_TIME	    (2 * sysClkRateGet())
#define ADDRD_QUERY_DELAY	    3


/* Internal types */
typedef struct election {
	void	    *lstLib1, *lstLib2;	/* For use by lstLib */
	SEM_ID	    notifySem;		/* Semaphore to GIVE when finished */
	timestamp_t startTime;		/* Timestamp of request */
} election_t;


/* Internal variables */
BOOL	addrdElectionDay;	/* TRUE = election is in progress */
LIST	addrdElectionList;	/* List of pending master FFSC elections */
SEM_ID	addrdElectionSem;	/* Guard semaphore for addrdElectionList */
WDOG_ID	addrdElectionTimer;	/* Watchdog timer for elections */
int	addrdEtoAFD = -1;	/* Ethernet to address daemon FD */
timestamp_t addrdLastQuery;	/* Timestamp at last QUERY message */


/* Internal functions */
static int	addrd(void);
static STATUS	addrdAssignNewIdentity(identity_t *);
static STATUS	addrdElectionEnd(void);
static STATUS	addrdElectionStart(void);
static int      addrdElectionTimerPop(int);
static BOOL     addrdEtherInputHook(struct ifnet *, char *, int);
static STATUS   addrdFindUnusedIPAddr(struct in_addr *);
static rackid_t addrdFindUnusedRackID(void);
static STATUS	addrdProcessAssignMsg(addrdmsg_t *);
static STATUS	addrdProcessElectionEndMsg(addrdmsg_t *);
static STATUS	addrdProcessElectionStartMsg(addrdmsg_t *);
static STATUS	addrdProcessIdentityMsg(addrdmsg_t *);
static STATUS	addrdProcessQueryMsg(addrdmsg_t *);
static STATUS	addrdProcessRequestRackIDMsg(addrdmsg_t *);
static STATUS	addrdResolveConflicts(void);
static STATUS	addrdResolveIPAddrConflict(identity_t *, identity_t *);
static STATUS	addrdResolveRackIDConflict(identity_t *, identity_t *);
static STATUS	addrdStartElectionTimer(void);


/*
 * addrdUpdatePartInfo: update our partition information with the new
 * information which we received from another MMSC out on the network.
 * Create standard string to update local (this is what the ELSC will
 * send over from IO6/IP27prom as a DSP message.
 */
STATUS addrdUpdatePartInfo(addrdmsg_t* Msg)
{
  char buf[128];
  memset(buf,0x0,128);

  /* Distributed Update: VERY IMPORTANT!
   * Don't update anything we already have.
   * Check the partition ID and the module number and 
   * if we already have it, bail out and don't do anything
   * with the packet.
   */
  if(contains_module(&partInfo, 
		     Msg->modulenums[1], /* P X */
		     Msg->modulenums[0]  /* M Y */
		     )){
#ifndef PRODUCTION
    sprintf(buf,"Ignoring P %ld M %ld [c=%ld](already have)\r\n",
	    Msg->modulenums[1],
	    Msg->modulenums[0],
	    Msg->reserved[0]);
    ffscMsg(buf);
#endif
    /*    print_part(&partInfo); */
    return OK;
  }
  else{
    if (partInfo != NULL){

      if(Msg->reserved[0])
	sprintf(buf,"P %d M %d C", 
		(int)Msg->modulenums[1],
		(int)Msg->modulenums[0]);
      else
	sprintf(buf,"P %d M %d",
		(int)Msg->modulenums[1],
		(int)Msg->modulenums[0]);

#ifndef PRODUCTION
      ffscDebugWrite(buf,strlen(buf));
#endif
      /* Update local info with what we found*/
      update_partition_info(&partInfo,buf);
      return OK;
    }
  }
  return ERROR;
}

/*
 * Send out our partition information to all other nodes.
 * Note, this will just accomodate our local information; we will
 * rely on others to broadcast their information, allowing us to
 * get a consistent view of all partitions in the system.
 */
STATUS addrdSendPartitionInfo(mod_part_info_t* mpi)
{
  /* Create message in addrdmsg_t format */
  addrdmsg_t msg;
  /* Set version and message type */
  msg.version = ADM_VERSION_CURR;
  msg.type = ADM_TYPE_NEW_PARTINFO;

  /* Send this partitioning information */
  msg.modulenums[0] = mpi->modid; /* Module number */
  msg.modulenums[1] = mpi->partid;/* Partition number */
  msg.reserved[0] = mpi->cons;    /* Whether a console is attached to this*/

  /* Send it to The Outside World */
  if (addrdBroadcastMsg(&msg) != OK) {
    ffscDebugWrite("broadcast failed.\n\r",19);
    return ERROR;
  }
#ifndef PRODUCTION
  ffscDebugWrite("broadcast worked.\n\r",19);
#endif
  return OK;
}


/*
 * Foreach partition ID in the local MMSC's memory, broadcast a 
 * packet on the network to all other MMSC. When MMSC receive this
 * information, they update their local MMSC info.
 */
STATUS broadcastPartitionInfo(part_info_t** pInfo)
{
  int i;
  mod_part_info_t mpi;
  part_info_t* p = *pInfo;
  /* Mutex enter  */
  semTake(part_mutex, WAIT_FOREVER);
  while(p != NULL){
    for(i = 0; i < p->moduleCount;i++){
      mpi.partid = p->partId;
      mpi.modid = p->modules[i];
      mpi.cons = (p->modules[i] == p->consoleModule) ? 1: 0;
      addrdSendPartitionInfo(&mpi);
    }
    p = p->next;
  }
  /* Mutex exit */
  semGive(part_mutex);   
  return OK;
}




/*
 * addrdElectMaster
 *	Starts a new master FFSC election and doesn't return until
 *	that specific election has completed. Useful when it is necessary
 *	to ensure that a recent change is accounted for in an election.
 *	Returns OK/ERROR.
 */
STATUS
addrdElectMaster(void)
{
	election_t MyElection;

	/* Set up an election request */
	MyElection.notifySem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	clock_gettime(CLOCK_REALTIME, &MyElection.startTime);

	/* Add our election request to the election list */
	if (semTake(addrdElectionSem, ADDRD_ELECTION_LIST_TIMEOUT) < 0) {
		ffscMsg("Unable to acquire election list guard semaphore");
		semDelete(MyElection.notifySem);
		return ERROR;
	}
	lstAdd(&addrdElectionList, (NODE *) &MyElection);
	semGive(addrdElectionSem);

	/* Send a "START ELECTION" message to the address daemon */
	if (addrdElectionStart() != OK) {
		ffscMsg("Unable to start election");

		if (semTake(addrdElectionSem, ADDRD_ELECTION_LIST_TIMEOUT) < 0)
		{
			ffscMsg("Unable to retrieve aborted election request");
			return ERROR;
		}
		lstDelete(&addrdElectionList, (NODE *) &MyElection);
		semGive(addrdElectionSem);

		semDelete(MyElection.notifySem);

		return ERROR;
	}

	/* Wait for the address daemon to tell us the election is over */
	if (semTake(MyElection.notifySem, WAIT_FOREVER) != OK) {
		ffscMsg("Election request terminated abnormally");

		if (semTake(addrdElectionSem, ADDRD_ELECTION_LIST_TIMEOUT) < 0)
		{
			ffscMsg("Unable to retrieve aborted election request");
			return ERROR;
		}
		lstDelete(&addrdElectionList, (NODE *) &MyElection);
		semGive(addrdElectionSem);

		semDelete(MyElection.notifySem);

		return ERROR;
	}

	/* The address daemon should have dequeued our request, so all	*/
	/* that is left to do is release our resources and exit.	*/
	semDelete(MyElection.notifySem);

	return OK;
}


/*
 * addrdInit
 *	Initialize the address daemon task
 */
STATUS
addrdInit(void)
{
	int TaskID;

	/* Set up a pipe for addrd ethernet packets. We can ignore the  */
	/* error case where the device already exists, since we may	*/
	/* created it last time we were invoked.			*/
	if (pipeDevCreate(ADDRD_PIPE_NAME,
			  ADDRD_PIPE_SIZE,
			  ADDRD_PIPE_MSGLEN) != OK  &&
	    errno != S_iosLib_DUPLICATE_DEVICE_NAME)
	{
		ffscMsgS("Unable to create addrd pipe");
		return ERROR;
	}

	/* Try to open the addrd pipe */
	addrdEtoAFD = open(ADDRD_PIPE_NAME, O_WRONLY, 0);
	if (addrdEtoAFD < 0) {
		ffscMsgS("Unable to open Ether2AddrD pipe");
		return ERROR;
	}

	/* Set up a watchdog timer for master FFSC elections */
	addrdElectionTimer = wdCreate();
	if (addrdElectionTimer == NULL) {
		ffscMsg("Unable to create addrd election timer");
		return ERROR;
	}

	/* Set up a list to hold pending elections and a semaphore */
	/* to serialize access to it.				   */
	lstInit(&addrdElectionList);
	addrdElectionSem = semMCreate(SEM_Q_PRIORITY);
	if (addrdElectionSem == NULL) {
		ffscMsg("Unable to create election list guard semaphore");
		return ERROR;
	}

	/* Spawn off the address daemon */
	TaskID = taskSpawn("tAddrD", 25, 0, 20000, addrd,
			   0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (TaskID == ERROR) {
		ffscMsg("Unable to spawn address daemon task!");
		return ERROR;
	}

	/* Arrange for interesting ethernet packets to be routed to the */
	/* address daemon task for processing				*/
	etherInputHookAdd(addrdEtherInputHook);

	return OK;
}


/*
 * addrdRequestRackID
 *	Reserves (or possibly even steals) the current rack ID by sending
 *	a REQUEST_RACKID out to The Rest Of The World.
 */
STATUS
addrdRequestRackID(identity_t *NewIdent)
{
	addrdmsg_t Msg;

	/* Change the rack ID locally */
	if (identSetRackID(NewIdent->rackid) != OK) {
		ffscMsg("Unable to set local rack ID");
		return ERROR;
	}

	/* Update the identity table too */
	if (identUpdate(NewIdent) != OK) {
		ffscMsg("WARNING: Unable to update identity table with new "
			    "rack ID");
	}

	/* Build a REQUEST_RACKID message */
	Msg.version  = ADM_VERSION_CURR;
	Msg.type     = ADM_TYPE_REQUEST_RACKID;
	addrdInsertIdentity(&Msg, &ffscLocal);

	/* If we happen to be the master FFSC, send it to ourself first	*/
	/* so we can resolve any external conflicts.			*/
	if (ffscIsMaster  &&  addrdLocalMsg(&Msg) != OK) {
		return ERROR;
	}

	/* Send it to The Outside World */
	if (addrdBroadcastMsg(&Msg) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * addrdUpdateLocalIdentity
 *	Update our local identity information, notifying others if
 *	necessary
 */
STATUS
addrdUpdateLocalIdentity(identity_t *NewIdentity)
{
	addrdmsg_t NewMsg;
	bayid_t    Bay;
	int SomethingChanged = 0;

	/* See if our IP address has changed */
	if (NewIdentity->ipaddr.s_addr != ffscLocal.ipaddr.s_addr) {
		identSetIPAddr(&NewIdentity->ipaddr);
		SomethingChanged = 1;
	}

	/* See if our rack ID has changed */
	if (NewIdentity->rackid != ffscLocal.rackid) {
		identSetRackID(NewIdentity->rackid);
		SomethingChanged = 1;
	}

	/* See if our capability flags have changed */
	if (NewIdentity->capabilities != ffscLocal.capabilities) {
		identSetCapabilities(NewIdentity->capabilities);
		SomethingChanged = 1;
	}

	/* See if any module numbers have changed */
	for (Bay = 0;  Bay < MAX_BAYS;  ++Bay) {
		if (NewIdentity->modulenums[Bay] != ffscLocal.modulenums[Bay])
		{
			identSetModuleNums(NewIdentity->modulenums);
			SomethingChanged = 1;
			break;
		}
	}

	/* If nothing changed, we are done */
	if (!SomethingChanged) {
		return OK;
	}

	/* Update the identity table with our new information */
	if (identUpdate(NewIdentity) < 0) {
		return ERROR;
	}

	/* Send out an IDENTITY message with our updated info */
	NewMsg.version  = ADM_VERSION_CURR;
	NewMsg.type     = ADM_TYPE_IDENTITY;
	addrdInsertIdentity(&NewMsg, NewIdentity);
	if (addrdBroadcastMsg(&NewMsg) != OK) {
		return ERROR;
	}

	return OK;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			     PRIVATE FUNCTIONS				*/
/*		      (Only external user is debug.c)			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * addrdBroadcastMsg
 *	Broadcast an ADDRD message on the network
 */
STATUS
addrdBroadcastMsg(addrdmsg_t *Msg)
{
	struct ether_header Header;

	/* Build an ethernet header */
	bcopy(ffscMACAddr, Header.ether_shost, 6);
	bcopy(etherbroadcastaddr, Header.ether_dhost, 6);
	Header.ether_type = htons(ETHERTYPE_ADDRD);

	/* Send the message out on the interface */
	if (etherOutput(ffscNetIF, &Header, (char *) Msg, sizeof(*Msg)) != OK)
	{
		ffscMsgS("Unable to broadcast ethernet packet");
		return ERROR;
	}

	return OK;
}


/*
 * addrdExtractIdentity
 *	Extracts the identity info from the specified addrdmsg_t and stores
 *	it in the specified identity_t.
 */
void
addrdExtractIdentity(addrdmsg_t *Msg, identity_t *Ident)
{
	int i;

	for (i = 0;  i < MAX_BAYS;  ++i) {
		Ident->modulenums[i] = Msg->modulenums[i];
	}

	bcopy((char *) &Msg->oident.uniqueid,
	      (char *) &Ident->uniqueid,
	      sizeof(uniqueid_t));
	bcopy((char *) &Msg->oident.ipaddr,
	      (char *) &Ident->ipaddr,
	      sizeof(struct in_addr));
	Ident->rackid = Msg->oident.rackid;
	Ident->capabilities = Msg->capabilities;
}


/*
 * addrdInsertIdentity
 *	Copies information from the specified identity_t into the specified
 *	addrdmsg_t.
 */
void
addrdInsertIdentity(addrdmsg_t *Msg, identity_t *Ident)
{
	int i;

	for (i = 0;  i < MAX_BAYS;  ++i) {
		Msg->modulenums[i] = Ident->modulenums[i];
	}

	bcopy((char *) &Ident->uniqueid,
	      (char *) &Msg->oident.uniqueid,
	      sizeof(uniqueid_t));
	bcopy((char *) &Ident->ipaddr,
	      (char *) &Msg->oident.ipaddr,
	      sizeof(struct in_addr));
	Msg->oident.rackid = Ident->rackid;
	Msg->capabilities  = Ident->capabilities;
}


/*
 * addrdLocalMsg
 *	Send an ADDRD message to the local address daemon only.
 */
STATUS
addrdLocalMsg(addrdmsg_t *Msg)
{
	char Buffer[ADM_MSGLEN];
	struct ether_header *Header;

	/* Initialize the buffer */
	bzero(Buffer, sizeof(Buffer));

	/* Build an ethernet header */
	Header = (struct ether_header *) Buffer;
	Header->ether_type = htons(ETHERTYPE_ADDRD);

	/* Copy in the message */
	bcopy((char *) Msg,
	      Buffer + sizeof(struct ether_header),
	      sizeof(addrdmsg_t));

	/* Write the message directly to the address daemon's input pipe */
	if (write(addrdEtoAFD, Buffer, ADM_MSGLEN) != ADM_MSGLEN) {
		ffscMsgS("addrdLocalMsg write to address daemon failed");
		return ERROR;
	}

	return OK;
}



/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * addrd
 *	The address daemon task itself
 */
int
addrd(void)
{
	addrdmsg_t *Msg;
	char Buffer[ADDRD_PIPE_MSGLEN];
	int  AddrDInFD;
	int  MsgLen;
	int  NumFDs;
	fd_set ReadFDs, ReadyReadFDs;
	struct ether_header *Hdr;

	/* Open up a pipe for reading incoming messages */
	AddrDInFD = open(ADDRD_PIPE_NAME, O_RDONLY, 0);
	if (AddrDInFD < 0) {
		ffscMsgS("Unable to open AddrDIn pipe");
		return -1;
	}

	/* Make a note of this particular fd for select */
	FD_ZERO(&ReadFDs);		NumFDs = 0;
	FD_SET(AddrDInFD, &ReadFDs);	++NumFDs;

	/* Loop while there is something to read from */
	while (NumFDs > 0) {

		/* Wait for something interesting */
		ReadyReadFDs = ReadFDs;
		if (select(FD_SETSIZE, &ReadyReadFDs, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				continue;
			}

			ffscMsgS("select failed in addrd");
			close(AddrDInFD);
			--NumFDs;
		}

		/* If there is no message, reiterate */
		if (!FD_ISSET(AddrDInFD, &ReadyReadFDs)) {
			continue;
		}

		/* Read the incoming message */
		MsgLen = read(AddrDInFD, Buffer, ADDRD_PIPE_MSGLEN);
		if (MsgLen < 0) {
			ffscMsgS("addrd failed reading from pipe");
			close(AddrDInFD);
			--NumFDs;
		}
		else if (MsgLen == 0) {
			ffscMsg("addrd got EOF from pipe");
			close(AddrDInFD);
			--NumFDs;
		}
		else if (MsgLen != ADM_MSGLEN) {
			ffscMsg("Received ADDRD message of length %d, "
				    "should be %d",
				MsgLen, ADM_MSGLEN);
			continue;
		}

		/* Set up pointers to the relevant parts of the packet */
		Hdr = (struct ether_header *) Buffer;
		Msg = (addrdmsg_t *) (Buffer + sizeof(struct ether_header));

		/* If we have a version 1 message, convert it to version 2 */
		/* by assigning module #'s of "unassigned"		   */
		if (Msg->version < 2) {
			int i;

			for (i = 0;  i < MAX_BAYS;  ++i) {
				Msg->modulenums[i] = MODULENUM_UNASSIGNED;
			}

			Msg->version = 2;
		}

		/* If we have a version 2 message, convert it to version 3 */
		/* by assigning default capability flags		   */
		if (Msg->version < 3) {
			Msg->capabilities = 0;
			Msg->version      = 3;
		}

		/* Process the incoming message */
		switch (Msg->type) {

		    case ADM_TYPE_IDENTITY:
			addrdProcessIdentityMsg(Msg);
			break;

		    case ADM_TYPE_QUERY:
			addrdProcessQueryMsg(Msg);
			break;

		    case ADM_TYPE_ASSIGN:
			addrdProcessAssignMsg(Msg);
			break;

		    case ADM_TYPE_ELECTION_START:
			addrdProcessElectionStartMsg(Msg);
			break;

		    case ADM_TYPE_ELECTION_END:
			addrdProcessElectionEndMsg(Msg);
			break;

		    case ADM_TYPE_REQUEST_RACKID:
			addrdProcessRequestRackIDMsg(Msg);
			break;

 		    case ADM_TYPE_NEW_PARTINFO:
		        addrdUpdatePartInfo(Msg);
			break;

		    default:
			ffscMsg("Received ADDRD message of unknown type %d",
				Msg->type);
			break;
		}
	}

	return 0;
}


/*
 * addrdAssignNewIdentity
 *	Arranges to notify the appropriate parties of a new identity
 *	assignment. The local identity records are updated, then if
 *	the FFSC being updated is the local FFSC, an IDENTITY message
 *	is sent out, otherwise an ASSIGN message is sent.
 */
STATUS
addrdAssignNewIdentity(identity_t *NewIdent)
{
	identity_t *OldIdent;

	/* Get the old identity info */
	OldIdent = identFindByUniqueID(NewIdent->uniqueid);
	if (OldIdent == NULL) {
		char UniqueIDStr[UNIQUEID_STRLEN];

		ffscUniqueIDToString(NewIdent->uniqueid, UniqueIDStr);
		ffscMsg("Attempted to assign new identity to non-existent "
			    "MMSC:\r\n"
			    "%s",
			UniqueIDStr);
		return ERROR;
	}

	/* If the changing FFSC is us, simply update our info directly */
	if (ffscCompareUniqueIDs(NewIdent->uniqueid, ffscLocal.uniqueid) == 0)
	{
		if (addrdUpdateLocalIdentity(NewIdent) != OK) {
			ffscMsg("WARNING: Unable to set new identity");
			return ERROR;
		}
	}

	/* Otherwise, send the changing FFSC an ASSIGN message */
	else {
		addrdmsg_t NewMsg;

		/* Initialize the ASSIGN message */
		NewMsg.version  = ADM_VERSION_CURR;
		NewMsg.type     = ADM_TYPE_ASSIGN;
		addrdInsertIdentity(&NewMsg, NewIdent);

		/* Broadcast the ASSIGN message */
		if (addrdBroadcastMsg(&NewMsg) != OK) {
			char UniqueIDStr[UNIQUEID_STRLEN];

			ffscUniqueIDToString(OldIdent->uniqueid, UniqueIDStr);
			ffscMsg("WARNING: Unable to assign new identity to %s",
				UniqueIDStr);

			return ERROR;
		}
		else if (ffscDebug.Flags & FDBF_ADDRD) {
			ffscMsg("Assigning new identity to remote MMSC:");
			identPrint(NewIdent);
		}

		/* Update the local info too */
		*OldIdent = *NewIdent;
	}

	return OK;
}


/*
 * addrdElectionEnd
 *	Terminates an election by sending an ELECTION END message to the
 *	address daemon.
 */
STATUS
addrdElectionEnd(void)
{
	addrdmsg_t Msg;

	Msg.version  = ADM_VERSION_CURR;
	Msg.type     = ADM_TYPE_ELECTION_END;
	if (addrdLocalMsg(&Msg) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * addrdElectionStart
 *	Initiates an election by sending an ELECTION START message to the
 *	address daemon.
 */
STATUS
addrdElectionStart(void)
{
	addrdmsg_t Msg;

	Msg.version  = ADM_VERSION_CURR;
	Msg.type     = ADM_TYPE_ELECTION_START;
	if (addrdLocalMsg(&Msg) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * addrdElectionTimerPop
 *	Receives control when the election timer has gone off. Simply
 *	sends an "ELECTION END" message to the address daemon. Note that
 *	this function is called at interrupt level.
 */
int
addrdElectionTimerPop(int Ignored)
{
	addrdElectionEnd();

	return 0;
}


/*
 * addrdEtherInputHook
 *	Hook function that VxWorks will call whenever an ethernet
 *	packet has been received. Returns TRUE if VxWorks should ignore
 *	the packet, or FALSE if it should process it in the usual way.
 */
BOOL
addrdEtherInputHook(struct ifnet *pIf, char *buffer, int length)
{
	struct ether_header *Hdr;

	/* Convenient pointer to the header of the packet */
	Hdr = (struct ether_header *) buffer;

	/* If this is an address daemon protocol packet, give it to */
	/* the address daemon rather than the usual protocol stack. */
	if (Hdr->ether_type == ETHERTYPE_ADDRD) {
		if (write(addrdEtoAFD, buffer, length) != length) {
			ffscMsgS("Write to address daemon failed");
		}

		return TRUE;
	}			 

	return FALSE;
}


/*
 * addrdFindUnusedIPAddr
 *	Finds an unused IP address and stores it in the specified
 *	in_addr. Returns ERROR if one could not be found, OK if successful.
 */
STATUS
addrdFindUnusedIPAddr(struct in_addr *NewAddr)
{
	int TrialHostAddr;

	for (TrialHostAddr = FFSC_MIN_HOSTADDR;
	     TrialHostAddr <= FFSC_MAX_HOSTADDR;
	     ++TrialHostAddr)
	{
		inet_makeaddr_b(ffscNetInfo.NetAddr, TrialHostAddr, NewAddr);
		if (identFindByIPAddr(NewAddr) == NULL) {
			return OK;
		}
	}

	return ERROR;
}


/*
 * addrdFindUnusedRackID
 *	Finds an unused rack ID, or RACKID_UNASSIGNED if none are
 *	available.
 */
rackid_t
addrdFindUnusedRackID(void)
{
	rackid_t CurrRackID;

	for (CurrRackID = 1;  CurrRackID < RACKID_MAX;  ++CurrRackID) {
		if (identFindByRackID(CurrRackID) == NULL) {
			return CurrRackID;
		}
	}

	return RACKID_UNASSIGNED;
}


/*
 * addrdProcessAssignMsg
 *	Process an incoming ADDRD ASSIGN message
 */
STATUS
addrdProcessAssignMsg(addrdmsg_t *Msg)
{
	identity_t Ident;

	/* Extract the identity information from the message */
	addrdExtractIdentity(Msg, &Ident);

	/* Ignore ASSIGN messages for other FFSC's */
	if (ffscCompareUniqueIDs(Msg->oident.uniqueid, ffscLocal.uniqueid)
	    != 0)
	{
		if (ffscDebug.Flags & FDBF_ADDRD) {
			ffscMsg("\r\nIgnored ADDRD ASSIGN:");
			identPrint(&Ident);
		}
		return OK;
	}

	/* Go update our own identity info as needed */
	if (addrdUpdateLocalIdentity(&Ident) != OK) {
		ffscMsg("WARNING: Unable to update local identity");
	}

	/* Say what we have done if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("\r\nADDRD ASSIGN:");
		identPrint(&Ident);
	}

	return OK;
}


/*
 * addrdProcessElectionEndMsg
 *	Process an incoming ADDRD ELECTION END message. Notice that this
 *	message should never be received over the network itself (i.e. it
 *	should only be generated locally) but no check is made for this.
 */
STATUS
addrdProcessElectionEndMsg(addrdmsg_t *Msg)
{
	election_t *CurrElection;

	/* Say what's happening if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("End of Master MMSC election");
	}

	/* Go remove any stale entries from the identity table */
	if (identCleanOldEntries(&addrdLastQuery) != OK) {
		return ERROR;
	}

	/* Go search the identity table for the master FFSC */
	ffscMaster = identFindMaster();
	if (ffscMaster == NULL) {
		ffscMsg("Unable to find master MMSC");
		ffscIsMaster = 0;
	}
	else if (ffscCompareUniqueIDs(ffscLocal.uniqueid,
				      ffscMaster->uniqueid)  == 0)
	{
		ffscIsMaster = 1;
	}
	else {
		ffscIsMaster = 0;
	}

	/* Acquire the solicited election list semaphore */
	if (semTake(addrdElectionSem, ADDRD_ELECTION_LIST_TIMEOUT) < 0) {
		ffscMsg("Unable to acquire election list guard semaphore");
		return ERROR;
	}

	/* Any solicited elections that were started before the last */
	/* QUERY message can be concluded at this time.		     */
	CurrElection = (election_t *) lstFirst(&addrdElectionList);
	while (CurrElection != NULL  &&
	       (ffscCompareTimestamps(&CurrElection->startTime,
				      &addrdLastQuery) <= 0))
	{
		election_t *Next;

		/* Must be careful with pointers since the current */
		/* election_t may go away as soon as we semGive it */
		Next = (election_t *) lstNext((NODE *) CurrElection);

		/* Say what's happening if desired */
		if (ffscDebug.Flags & FDBF_ADDRD) {
			ffscMsg("Concluded Master MMSC election started at %s",
				ffscTimestampToString(&CurrElection->startTime,
						      NULL));
		}

		/* Remove the election entry from the list */
		lstDelete(&addrdElectionList, (NODE *) CurrElection);

		/* Release the task that was waiting for this election */
		semGive(CurrElection->notifySem);

		/* On to the next entry */
		CurrElection = Next;
	}

	/* Done with the election list semaphore */
	semGive(addrdElectionSem);

	/* Election day is over */
	addrdElectionDay = FALSE;

	/* If we were elected master, do a round of conflict resolution */
	if (ffscIsMaster  &&  addrdResolveConflicts() != OK) {
		ffscMsg("Address daemon conflict resolution failed");
	}

	return OK;
}


/*
 * addrdProcessElectionStartMsg
 *	Process an incoming ADDRD ELECTION START message. Notice that this
 *	message should never be received over the network itself (i.e. it
 *	should only be generated locally) but no check is made for this.
 */
STATUS
addrdProcessElectionStartMsg(addrdmsg_t *Msg)
{
	addrdmsg_t NewMsg;

	/* Make a note of the time of this request */
	clock_gettime(CLOCK_REALTIME, &addrdLastQuery);

	/* Say what's going on if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("Starting Master MMSC election at %s",
			ffscTimestampToString(&addrdLastQuery, NULL));
	}

	/* Send out a QUERY message to everyone else */
	NewMsg.version  = ADM_VERSION_CURR;
	NewMsg.type     = ADM_TYPE_QUERY;
	addrdInsertIdentity(&NewMsg, &ffscLocal);
	if (addrdBroadcastMsg(&NewMsg) != OK) {
		return ERROR;
	}

	/* Arrange to check the election results later on */ 
	if (addrdStartElectionTimer() != OK) {
		return ERROR;
	}
	addrdElectionDay = TRUE;

	return OK;
}


/*
 * addrdProcessIdentityMsg
 *	Process an incoming ADDRD IDENTITY message
 */
STATUS
addrdProcessIdentityMsg(addrdmsg_t *Msg)
{
	identity_t NewIdent;
	int Result;

	/* Extract the identity information from the message */
	addrdExtractIdentity(Msg, &NewIdent);

	/* The incoming unique ID should NOT match our own */
	if (ffscCompareUniqueIDs(NewIdent.uniqueid, ffscLocal.uniqueid) == 0) {
		char UniqueIDBuf[UNIQUEID_STRLEN];

		ffscUniqueIDToString(NewIdent.uniqueid, UniqueIDBuf);
		ffscMsg("\r\nWARNING: Received ADDRD IDENTITY message for "
			    "local unique ID\r\n"
			    "         %s",
			UniqueIDBuf);
		return ERROR;
	}

	/* Update the identity table with this new information */
	Result = identUpdate(&NewIdent);
	if (Result < 0) {
		return ERROR;
	}

	/* If an election is in progress, restart the election timer */
	if (addrdElectionDay) {
		if (addrdStartElectionTimer() != OK) {
			return ERROR;
		}
	}

	/* Display the message info if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		char *Action = (Result == 0) ? "update" : "new";

		ffscMsg("\r\nADDRD IDENTITY - %s:", Action);
		identPrint(&NewIdent);
	}

	/* If we are the master FFSC, check for conflicts */
	if (ffscIsMaster  &&  addrdResolveConflicts() != OK) {
		ffscMsg("Address daemon conflict resolution failed");
	}

	return OK;
}


/*
 * addrdProcessQueryMsg
 *	Process an incoming ADDRD QUERY message
 */
STATUS
addrdProcessQueryMsg(addrdmsg_t *Msg)
{
	addrdmsg_t NewMsg;
	identity_t Ident;
	int Result;

	/* Make a note of the time of this request */
	clock_gettime(CLOCK_REALTIME, &addrdLastQuery);

	/* Extract the identity information from the message */
	addrdExtractIdentity(Msg, &Ident);

	/* The incoming unique ID should NOT match our own */
	if (ffscCompareUniqueIDs(Ident.uniqueid, ffscLocal.uniqueid) == 0) {
		char UniqueIDBuf[UNIQUEID_STRLEN];

		ffscUniqueIDToString(Ident.uniqueid, UniqueIDBuf);
		ffscMsg("\r\nWARNING: Received ADDRD QUERY message for local "
			    "unique ID\r\n"
			    "         %s",
			UniqueIDBuf);
		return ERROR;
	}

	/* Update the identity table with this new information */
	Result = identUpdate(&Ident);
	if (Result < 0) {
		return ERROR;
	}

	/* No conflict resolution now, we'll handle that when the */
	/* election has completed if we are elected master FFSC.  */

	/* If an election is in progress, restart the election timer */
	if (addrdElectionDay) {
		if (addrdStartElectionTimer() != OK) {
			return ERROR;
		}
	}

	/* Display the message info if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		char *Action = (Result == 0) ? "update" : "new";

		ffscMsg("\r\nADDRD QUERY - %s:", Action);
		identPrint(&Ident);
	}

	/* This is probably paranoid, but wait for a couple of ticks */
	/* to make sure that everyone has received the QUERY message */
	/* before we broadcast our resulting IDENTITY.		     */
	taskDelay(ADDRD_QUERY_DELAY);

	/* Broadcast our own identity to The World */
	NewMsg.version  = ADM_VERSION_CURR;
	NewMsg.type     = ADM_TYPE_IDENTITY;
	addrdInsertIdentity(&NewMsg, &ffscLocal);
	if (addrdBroadcastMsg(&NewMsg) != OK) {
		return ERROR;
	}

	/* Arrange to check the election results later on */ 
	if (addrdStartElectionTimer() != OK) {
		return ERROR;
	}
	addrdElectionDay = TRUE;

	return OK;
}


/*
 * addrdProcessRequestMsg
 *	Process an incoming ADDRD REQUEST_RACKID message
 */
STATUS
addrdProcessRequestRackIDMsg(addrdmsg_t *Msg)
{
	identity_t ReqIdent;
	int Result;

	/* Extract the identity information from the message */
	addrdExtractIdentity(Msg, &ReqIdent);

	/* Update the identity table with this new information */
	Result = identUpdate(&ReqIdent);
	if (Result < 0) {
		return ERROR;
	}

	/* If an election is in progress, restart the election timer */
	if (addrdElectionDay) {
		if (addrdStartElectionTimer() != OK) {
			return ERROR;
		}
	}

	/* Display the message info if desired */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		char *Action = (Result == 0) ? "update" : "new";

		ffscMsg("\r\nADDRD REQUEST_RACKID - %s:", Action);
		identPrint(&ReqIdent);
	}

	/* If we are the master FFSC, check for any other FFSC with a */
	/* conflicting rack ID and reassign them something different. */
	/* We assume that the requestor is honest and don't bother    */
	/* checking to see if they have changed their IP address.     */
	if (ffscIsMaster) {
		identity_t *Conflictee;

		/* Look for someone with a conflicting rack ID. Note that */
		/* we assume that there is no more than one such rack,    */
		/* since we've presumably resolved any other conflicts.	  */
		Conflictee = identFindConflictingRackID(&ReqIdent);
		if (Conflictee != NULL) {
			identity_t NewIdent;

			/* Start with the original identity_t */
			NewIdent = *Conflictee;

			/* Set up a new rack ID */
			NewIdent.rackid = addrdFindUnusedRackID();
			if (NewIdent.rackid == RACKID_UNASSIGNED) {
				char UniqueIDBuf[UNIQUEID_STRLEN];

				ffscUniqueIDToString(Conflictee->uniqueid,
						     UniqueIDBuf);
				ffscMsg("WARNING: Unable to assign new rack "
					    "ID to %s",
					UniqueIDBuf);
			}

			/* Go assign the new rack ID */
			if (addrdAssignNewIdentity(&NewIdent) != OK) {
				return ERROR;
			}
		}
	}

	return OK;
}


/*
 * addrdResolveConflicts
 *	Scan the identity table searching for any identifier conflicts
 *	and arrange to have them resolved. This should only be called
 *	by the master FFSC!
 */
STATUS
addrdResolveConflicts(void)
{
	if (!ffscIsMaster) {
		ffscMsg("Non-master MMSC attempted to resolve conflicts!");
		return ERROR;
	}

	if (identScanForConflicts(addrdResolveIPAddrConflict,
				  addrdResolveRackIDConflict) != OK)
	{
		ffscMsg("Identifier conflict resolution failed");
		return ERROR;
	}

	return OK;
}


/*
 * addrdResolveIPAddrConflict
 *	Resolve an IP address conflict between two given identity_t's by
 *	assigning an unused IP address to one of them.
 */
int
addrdResolveIPAddrConflict(identity_t *Ident1, identity_t *Ident2)
{
	identity_t NewIdent;

	/* The FFSC with the higher uniqueid always loses - that way	*/
	/* we are assured of never changing our own identity (we are	*/
	/* presumably the master FFSC and so have the lowest uniqueid)	*/
	if (ffscCompareUniqueIDs(Ident1->uniqueid, Ident2->uniqueid) < 0) {
		NewIdent = *Ident2;
	}
	else {
		NewIdent = *Ident1;
	}

	/* Search for an unused IP address */
	if (addrdFindUnusedIPAddr(&NewIdent.ipaddr) != OK) {
		ffscMsg("Unable to resolve conflicting IP address");
		return 1;
	}

	/* Go assign the new IP address */
	return addrdAssignNewIdentity(&NewIdent);
}


/*
 * addrdResolveRackIDConflict
 *	Resolve a rack ID conflict between two given identity_t's by
 *	assigning an unused rack ID to one of them.
 */
STATUS
addrdResolveRackIDConflict(identity_t *Ident1, identity_t *Ident2)
{
	identity_t NewIdent;

	/* The FFSC with the higher uniqueid always loses, for lack of	*/
	/* a better reason						*/
	if (ffscCompareUniqueIDs(Ident1->uniqueid, Ident2->uniqueid) < 0) {
		NewIdent = *Ident2;
	}
	else {
		NewIdent = *Ident1;
	}

	/* Come up with a new rack ID for the loser */
	NewIdent.rackid = addrdFindUnusedRackID();
	if (NewIdent.rackid == RACKID_UNASSIGNED) {
		char UniqueIDStr[UNIQUEID_STRLEN];

		ffscUniqueIDToString(NewIdent.uniqueid, UniqueIDStr);
		ffscMsg("Unable to assign rack ID to %s", UniqueIDStr);
		/* 9/1/98 Sasha
		   It seems like this would be a good place to return 
		   an error
		   */
		return ERROR;
	}

	/* Go assign the new IP address */
	return addrdAssignNewIdentity(&NewIdent);
}


/*
 * addrdStartElectionTimer
 *	Simple function to (re)start the election timer
 */
STATUS
addrdStartElectionTimer(void)
{
	if (wdStart(addrdElectionTimer,
		    ADDRD_ELECTION_TIME,
		    addrdElectionTimerPop,
		    0)
	    < 0)
	{
		ffscMsg("Unable to start/restart election timer");
		return ERROR;
	}

	return OK;
}



