/*
 * debug.c
 *	Debugging functions for FFSC
 */

#include <vxworks.h>

#include <fiolib.h>
#include <inetlib.h>
#include <iolib.h>
#include <siolib.h>
#include <socklib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslib.h>

#include "ffsc.h"

#include "addrd.h"
#include "identity.h"
#include "log.h"
#include "misc.h"
#include "nvram.h"
#include "tty.h"


/* Constants */
#define FFSC_DEBUG_INIT_CONSOLE	FDBC_STDOUT

#ifdef PRODUCTION
#define FFSC_DEBUG_INIT_FLAGS	0
#else
#define FFSC_DEBUG_INIT_FLAGS	(FDBF_INIT | FDBF_CONSOLE | FDBF_ELSC | \
				 FDBF_ROUTER | FDBF_REMOTE | FDBF_BASEIO)
#endif /* !PRODUCTION */


/* Global variables */
int	    ffscDebugFD;
debuginfo_t ffscDebug;


/* A handy buffer for shell functions to use */
char DebugBuf[128];


/*
 * ffscDebugInit
 *	Top-level debugging initialization
 */
STATUS
ffscDebugInit(void)
{
	int RC;

	/* No console (yet) */
	ffscDebugFD = -1;

	/* Try to read debugging flags from NVRAM */
	if (nvramRead(NVRAM_DEBUGINFO, &ffscDebug) != OK) {

		/* Debugging info is not available in NVRAM, so build */
		/* some default debugging info.			      */
		bzero((char *) &ffscDebug, sizeof(ffscDebug));
		ffscDebug.Flags   = FFSC_DEBUG_INIT_FLAGS;
		ffscDebug.Console = FFSC_DEBUG_INIT_CONSOLE;
		ffscDebug.MsgMon  = 0;

		/* Try to write the debugging info out (but ignore */
		/* any errors that may occur)			   */
		nvramWrite(NVRAM_DEBUGINFO, &ffscDebug);
	}

	/* 
	 * When debug flags are zero, we use the debug port for 
	 * a second console, and the alternate console port for debugging.
	 * This means that COM6 will be attached to a second IO6 and COM5
	 * will be spitting out debug stuff. Now, if debug is zero, on the
	 * other hand, we will spawn a second or "alternate" console task
	 * on COM5 instead. This way we can debug and support remote access.
	 * We CANNOT debug with Remote-access but since that would be an
	 * exercise in futility there is no point in having debugging output
	 * if you have an alternate console.
	 */
	if(ffscDebug.Flags != 0){
	  RC = ffscSetDebugConsole(FDBC_COM5);
	  if (RC != 0) {
	    fprintf(stderr,
		    "Unable to open debug console %d: RC %d\r\n",
		    ffscDebug.Console, RC);
	    return ERROR;
	  }
	} 
	else {

#ifdef OLDCODE 
	  /* I left this in as an example for future revisions, if any.*/
	  /* Go initialize the debugging console (if any) */
	  RC = ffscSetDebugConsole(ffscDebug.Console);
	  if (RC != 0) {
	    fprintf(stderr,
		    "Unable to open debug console %d: RC %d\r\n",
		    ffscDebug.Console, RC);
	    return ERROR;
	  }
	  fdprintf(ffscDebugFD, "\033[H\033[2J");
	  return OK;
#else
	  /* 
	   *  Setup FD for no debug output. The moral equivalent of /dev/null.
	   */
	  RC = ffscSetDebugConsole( FDBC_NONE );
	  if (RC != 0) {
	    fprintf(stderr,
		    "Unable to open debug console %d: RC %d\r\n",
		    ffscDebug.Console, RC);
	    return ERROR;
	  }
	  return OK;
#endif
	}

}


/*
 * ffscDebugWrite
 *	Like write, but goes directly to the debug port without logging.
 */
int
ffscDebugWrite(char *Buf, int Length)
{
	if (ffscDebugFD < 0) {
		return 0;
	}

	return write(ffscDebugFD, Buf, Length);
}


/*
 * ffscMsg
 *	Like printf, but prints to the debugging console (if active).
 *	The message is followed by a newline sequence.
 */
void
ffscMsg(const char *Format, ...)
{
	char MsgBuf[1000];
	va_list Args;

	va_start(Args, Format);
	vsprintf(MsgBuf, Format, Args);
	va_end(Args);

	logWriteLine(logDebug, MsgBuf);

	if (ffscDebugFD >= 0) {
		if (ffscDebug.Flags & FDBF_PTASKID) 
			fdprintf(ffscDebugFD, "(%s) %s\r\n", taskName(taskIdSelf()), MsgBuf);
		else
			fdprintf(ffscDebugFD, "%s\r\n", MsgBuf);
	}
}


/*
 * ffscMsgN
 *	Like ffscMsg, but doesn't include the trailing newline
 */
void
ffscMsgN(const char *Format, ...)
{
	char MsgBuf[1000];
	va_list Args;

	va_start(Args, Format);
	vsprintf(MsgBuf, Format, Args);
	va_end(Args);

	logWrite(logDebug, MsgBuf, strlen(MsgBuf));

	if (ffscDebugFD >= 0) {
		if (ffscDebug.Flags & FDBF_PTASKID) 
			fdprintf(ffscDebugFD, "(%s) %s", taskName(taskIdSelf()), MsgBuf);
		else
			fdprintf(ffscDebugFD, "%s", MsgBuf);
	}
}


/*
 * ffscMsgS
 *	Like ffscMsg, but appends the current strerror.
 */
void
ffscMsgS(const char *Format, ...)
{
	char MsgBuf[1000];
	va_list Args;

	if (Format != NULL) {
		va_start(Args, Format);
		vsprintf(MsgBuf, Format, Args);
		va_end(Args);

		strcat(MsgBuf, ": ");
		strcat(MsgBuf, strerror(errno));
	}
	else {
		strcpy(MsgBuf, strerror(errno));
	}

	logWriteLine(logDebug, MsgBuf);

	if (ffscDebugFD >= 0) {
		if (ffscDebug.Flags & FDBF_PTASKID) 
			fdprintf(ffscDebugFD, "(%s) %s\r\n", taskName(taskIdSelf()), MsgBuf);
		else
			fdprintf(ffscDebugFD, "%s\r\n", MsgBuf);
	}
}


/*
 * ffscSetDebugConsole
 *	Activate the debugging console on the specified device. If a debug
 *	console is already active, it is closed after the new device has
 *	been successfully activated.
 *
 *	Returns:
 *		0: successful
 *		1: invalid device number
 *		2: console device could not be opened
 *		3: unable to set TTY baud rate
 *		4: unable to set TTY options
 */
int
ffscSetDebugConsole(uint16_t DevNum)
{
	/* If no console is desired, all we need to do is close the old one */
	if (DevNum == FDBC_NONE) {
	  /* @@ TODO: verify that this is ok when we use the debug port.
	   */
	  /*		if (ffscDebugFD >= 0) { */
	  ffscDebugFD = -1;
	  /*		} */
	  
	  return 0;
	}

	/* Select the new device */
	switch (DevNum) {

	    case FDBC_COM1:
	    case FDBC_COM2:
	    case FDBC_COM3:
	    case FDBC_COM4:
	    case FDBC_COM5:
	    case FDBC_COM6:
		ffscDebugFD = TTYfds[DevNum - 1];
		break;

	    case FDBC_DISPLAY:
		ffscDebugFD = DISPLAYfd;
		break;

	    case FDBC_STDOUT:
		ffscDebugFD = 1;
		break;

	    case FDBC_STDERR:
		ffscDebugFD = 2;
		break;

	    default:
		fprintf(stderr,
			"Invalid debug console device number: %d\r\n",
			DevNum);
		return 1;
	}

	/* That is all */
	return 0;
}



#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*		       ADDRESS DAEMON SHELL FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * admsg
 *	Send an ADDRD message
 */
STATUS
admsg(int Local, int Type, char *Args)
{
	addrdmsg_t Msg;
	identity_t Identity;
	STATUS	   Result;

	/* Make sure this command is enabled */
	if (!(ffscDebug.Flags & FDBF_ADCLIENTS)) {
		ffscMsg("admsg command disabled");
		return ERROR;
	}

	/* Parse input arguments as needed */
	if (identParse(Args, &Identity) != OK) {
		return ERROR;
	}

	/* Build an ADDRD message */ 
	Msg.version  = ADM_VERSION_CURR;
	Msg.type     = (Type == 0) ? ADM_TYPE_IDENTITY : Type;
	addrdInsertIdentity(&Msg, &Identity);

	/* Send it to the appropriate destination */
	if (Local) {
		Result = addrdLocalMsg(&Msg);
	}
	else {
		Result = addrdBroadcastMsg(&Msg);
	}

	return Result;
}


/*
 * adb, adl
 *	Simplified variants of admsg to send a broadcast (adb) or local (adl)
 *	ADDRD message
 */
STATUS
adb(int Type, char *Args)
{
	if (!(ffscDebug.Flags & FDBF_ADCLIENTS)) {
		ffscMsg("adb command disabled");
		return ERROR;
	}

	return admsg(0, Type, Args);
}

STATUS
adl(int Type, char *Args)
{
	if (!(ffscDebug.Flags & FDBF_ADCLIENTS)) {
		ffscMsg("adl command disabled");
		return ERROR;
	}

	return admsg(1, Type, Args);
}


/*
 * election
 *	Provoke an election for master FFSC
 */
STATUS
election(void)
{
	if (addrdElectMaster() != OK) {
		return ERROR;
	}

	if (ffscIsMaster) {
		ffscMsg("This is the Master MMSC");
	}
	else {
		ffscMsg("Another machine is the Master MMSC");
	}

	return OK;
}

#endif  /* !PRODUCTION */


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*		     DEBUGGING CONSOLE SHELL FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * cls
 *	Clear the debugging console (or any other TTY) screen.
 */
STATUS
cls(int FD)
{
	if (FD == 0) {
		FD = ioTaskStdGet(0, 1);
	}

	if (FD < 0) {
		return ERROR;
	}

	fdprintf(FD, "\033[H\033[2J");

	return OK;
}


/*
 * debug
 *	Set the debugging flags
 */
STATUS
debug(int flags)
{
	/* Set the flags */
	ffscDebug.Flags = flags;

	/* Update the NVRAM debugging info with the new info */
	if (nvramWrite(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * fdbdisplay
 *	Activate the debugging console on the display.
 */
STATUS
fdbdisplay(void)
{
	/* Formally change the debugging console if possible */
	if (ffscSetDebugConsole(FDBC_DISPLAY) != 0) {
		return ERROR;
	}

	/* Update the NVRAM debugging info with the new info */
	ffscDebug.Console = FDBC_DISPLAY;
	if (nvramWrite(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
		return ERROR;
	}

	/* Clear the screen for neatness */
	cls(ffscDebugFD);

	return OK;
}


/*
 * fdboff
 *	Deactivate the FFSC debugging console.
 */
STATUS
fdboff(void)
{
	/* Disable debugging console */
	if (ffscSetDebugConsole(FDBC_NONE) != 0) {
		return ERROR;	/* No excuse for this, really */
	}

	/* Update the NVRAM debugging info with the new info */
	ffscDebug.Console = FDBC_NONE;
	if (nvramWrite(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * fdbshell
 *	Activate the debugging console on the current shell (i.e. via stdout).
 */
STATUS
fdbshell(void)
{


	/* Formally change the debugging console if possible */
	if (ffscSetDebugConsole(FDBC_STDOUT) != 0) {
		return ERROR;
	}

	/* Update the NVRAM debugging info with the new info */
	ffscDebug.Console = FDBC_STDOUT;
	if (nvramWrite(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * fdbtty
 *	Activate the debugging console on a TTY.
 */
STATUS
fdbtty(int Num)
{
	/* Make sure a reasonable TTY number was specified. Note that	*/
	/* we assume that FDBC_* constants are the same as TTY numbers.	*/
	if (!FDBC_ISCOM(Num)) {
		fprintf(stderr, "Invalid TTY number: %d\r\n", Num);
		return ERROR;
	}

	/* Set the appropriate console number */
	if (ffscSetDebugConsole(Num) != 0) {
		return ERROR;
	}

	/* Update the NVRAM debugging info with the new info */
	ffscDebug.Console = Num;
	if (nvramWrite(NVRAM_DEBUGINFO, &ffscDebug) != OK) {
		return ERROR;
	}

	/* Clear the screen for neatness */
	cls(ffscDebugFD);

	return OK;
}


/*
 * msgmon
 *	Monitor all messages that occur on the specified File Descriptor
 */
STATUS
msgmon(int FD)
{
	ffscDebug.MsgMon = FD;

	return OK;
}

#endif  /* !PRODUCTION */


/*----------------------------------------------------------------------*/
/*									*/
/*		    IDENTITY SERVICES SHELL FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * identity
 *	Print an identity_t in human readable format
 */
STATUS
identity(identity_t *Identity)
{
	if (Identity == NULL) {
		Identity = &ffscLocal;
	}

	return identPrint(Identity);
}


#ifndef PRODUCTION

/*
 * itadd
 *	Add an entry to the identity table
 */
STATUS
itadd(char *Info)
{
	identity_t Identity;
	int Result;

	/* Make sure it's OK to do this */
	if (!(ffscDebug.Flags & FDBF_ITMANIP)) {
		ffscMsg("itadd command disabled");
		return ERROR;
	}

	/* Parse the specified identity info */
	if (identParse(Info, &Identity) != OK) {
		return ERROR;
	}

	/* Add/update the identity table */
	Result = identUpdate(&Identity);
	if (Result < 0) {
		ffscMsg("ERROR: Unable to update identity table entry");
		return ERROR;
	}

	/* Say what happened */
	if (Result == 0) {
		ffscMsg("Updated identity table entry:");
	}
	else {
		ffscMsg("Added new identity table entry:");
	}
	identity(&Identity);

	return OK;
}


/*
 * itclean
 *	Clean out any identity table entries that have a timestamp
 *	preceding the specified value (which is assumed to be a string
 *	representing the seconds portion of a timestamp)
 */
STATUS
itclean(char *SecsString)
{
	struct timespec StaleTime;

	if (!(ffscDebug.Flags & FDBF_ITMANIP)) {
		ffscMsg("itclean command disabled");
		return ERROR;
	}

	if (SecsString == NULL  ||
	    sscanf(SecsString, "%ld", &StaleTime.tv_sec) != 1)
	{
		ffscMsg("Invalid time \"%s\"", SecsString);
		return ERROR;
	}

	return identCleanOldEntries(&StaleTime);
}


/*
 * itclear
 *	Clear the contents of the identity table
 */
STATUS
itclear(void)
{
	if (!(ffscDebug.Flags & FDBF_ITMANIP)) {
		ffscMsg("itclear command disabled");
		return ERROR;
	}

	return identClearTable();
}


/*
 * itdump
 *	Dump the contents of the identity table.
 */
STATUS
itdump(void)
{
	(void) identPrintTable();

	return OK;
}


/*
 * iti
 *	Retrieve and display the first identity table entry with the matching
 *	IP address
 */
STATUS
iti(char *IPAddrStr)
{
	struct in_addr IPAddr;
	identity_t *Identity;

	if (ffscParseIPAddr(IPAddrStr, &IPAddr) != OK) {
		return ERROR;
	}

	Identity = identFindByIPAddr(&IPAddr);
	if (Identity == NULL) {
		ffscMsg("No identity table entry with IP address %s",
			IPAddrStr);
		return ERROR;
	}

	return identity(Identity);
}


/*
 * itm
 *	Retrieve and display the first identity table entry with the matching
 *	module number
 */
STATUS
itm(char *ModuleNumStr)
{
	modulenum_t  ModuleNum;
	identity_t *Identity;

	if (ffscParseModuleNum(ModuleNumStr, &ModuleNum) != OK) {
		return ERROR;
	}

	Identity = identFindByModuleNum(ModuleNum, NULL);
	if (Identity == NULL) {
		ffscMsg("No identity table entry with module # %s",
			ModuleNumStr);
		return ERROR;
	}

	return identity(Identity);
}


/*
 * itr
 *	Retrieve and display the first identity table entry with the matching
 *	rack ID
 */
STATUS
itr(char *RackIDStr)
{
	rackid_t   RackID;
	identity_t *Identity;

	if (ffscParseRackID(RackIDStr, &RackID) != OK) {
		return ERROR;
	}

	Identity = identFindByRackID(RackID);
	if (Identity == NULL) {
		ffscMsg("No identity table entry with rack ID %s", RackIDStr);
		return ERROR;
	}

	return identity(Identity);
}


/*
 * itu
 *	Retrieve and display the first identity table entry with the matching
 *	uniqueID
 */
STATUS
itu(char *UniqueIDStr)
{
	uniqueid_t UniqueID;
	identity_t *Identity;

	if (ffscParseUniqueID(UniqueIDStr, &UniqueID) != OK) {
		return ERROR;
	}

	Identity = identFindByUniqueID(UniqueID);
	if (Identity == NULL) {
		ffscMsg("No identity table entry with unique ID %s",
			UniqueIDStr);
		return ERROR;
	}

	return identity(Identity);
}


/*
 * master
 *	Print identity info for the master FFSC
 */
STATUS
master(void)
{
	if (ffscMaster == NULL) {
		ffscMsg("There is no master MMSC");
		return ERROR;
	}

	return identity(ffscMaster);
}


/*
 * rackid
 *	Set the rackid for this FFSC
 */
STATUS
rackid(int NewRackID)
{
	identity_t NewIdent;

	/* Make sure the rack ID is OK */
	if ((NewRackID < 0  &&  NewRackID != RACKID_UNASSIGNED)  ||
	    NewRackID >= MAX_RACKS)
	{
		ffscMsg("Invalid rack ID %d", NewRackID);
		return ERROR;
	}

	/* Set up a proposed new identity */
	NewIdent = ffscLocal;
	NewIdent.rackid = NewRackID;

	/* Ask the address daemon to reserve (and possibly steal) this	*/
	/* rack ID for us.						*/
	if (addrdRequestRackID(&NewIdent) != OK) {
		ffscMsg("WARNING: Rack ID reservation failed - conflicts "
			    "may exist");
	}

	return OK;
}

#endif  /* !PRODUCTION */


/*----------------------------------------------------------------------*/
/*									*/
/*			   NVRAM SHELL FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

#ifndef PRODUCTION

/*
 * nvinfo
 *	Prints fascinating information about the current contents of NVRAM,
 *	according to the in-memory NVRAM header.
 */
STATUS
nvinfo(void)
{
	return nvramPrintInfo();
}


/*
 * reset_nvram
 *	Resets NVRAM to its initial state
 */
STATUS
reset_nvram(void)
{
	return nvramReset();
}


/*
 * update_netinfo
 *	Updates the current network parameters in NVRAM
 */
STATUS
update_netinfo(void)
{
	return nvramWrite(NVRAM_NETINFO, &ffscNetInfo);
}


/*
 * update_rackid
 *	Updates the current rack ID in NVRAM
 */
STATUS
update_rackid(void)
{
	return nvramWrite(NVRAM_RACKID, &ffscLocal.rackid);
}

#endif  /* !PRODUCTION */




/*----------------------------------------------------------------------*/
/*									*/
/*			 TUNEABLE VARIABLE FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

#ifndef PRODUCTION

/*
 * tune_reset
 *	Reset the value of a tuneable variable to its default
 */
STATUS
tune_reset(int Index)
{
	/* Make sure the index is OK */
	if (Index < 0  ||  Index >= NUM_TUNE) {
		ffscMsg("Invalid index: %d", Index);
		return ERROR;
	}

	/* Show the old value for posterity */
	ffscMsg("Original value of tuneable %d: %d", Index, ffscTune[Index]);

	/* Set the new value and show it too */
	ffscTune[Index] = ffscTuneInfo[Index].Default;
	ffscMsg("Reset value of tuneable %d: %d", Index, ffscTune[Index]);

	/* Update NVRAM */
	if (nvramWrite(NVRAM_TUNE, ffscTune) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * tune_set
 *	Set the value of a tuneable variable
 */
STATUS
tune_set(int Index, int Value)
{
	/* Make sure the index is OK */
	if (Index < 0  ||  Index >= NUM_TUNE) {
		ffscMsg("Invalid index: %d", Index);
		return ERROR;
	}

	/* Show the old value for posterity */
	ffscMsg("Original value of tuneable %d: %d", Index, ffscTune[Index]);

	/* Set the new value and show it too */
	ffscTune[Index] = Value;
	ffscMsg("New value of tuneable %d: %d", Index, ffscTune[Index]);

	/* Update NVRAM */
	if (nvramWrite(NVRAM_TUNE, ffscTune) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * tune_show
 *	Show the value of a tuneable variable
 */
STATUS
tune_show(int Index)
{
	/* Make sure the index is OK */
	if (Index < 0  ||  Index >= NUM_TUNE) {
		ffscMsg("Invalid index: %d", Index);
		return ERROR;
	}

	/* Show the old value for posterity */
	ffscMsg("Value of tuneable %d: %d", Index, ffscTune[Index]);

	return OK;
}


/*
 * tune_showall
 *	Show the value of all tuneable variables
 */
STATUS
tune_showall(void)
{
	int i;

	ffscMsg("Tuneable variables:");
	for (i = 0;  i < NUM_TUNE;  ++i) {
		ffscMsg("%2d: %d", i, ffscTune[i]);
	}

	return OK;
}

#endif  /* !PRODUCTION */



/*----------------------------------------------------------------------*/
/*									*/
/*			  OTHER SHELL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

#ifndef PRODUCTION

/*
 * integer
 *	Prints the value of the specified integer. Useful in the shell
 *	when all you have is a variable name.
 */
STATUS
integer(int Value)
{
	ffscMsg("Value = %d", Value);
	return OK;
}


/*
 * pointer
 *	Prints the value of the specified integer in hex. Useful in the shell
 *	when all you have is a variable name.
 */
STATUS
pointer(int Value)
{
	ffscMsg("Value = %p", Value);
	return OK;
}


/*
 * string
 *	Prints the string at the specified address.
 */
STATUS
string(char *Ptr)
{
	if (Ptr == NULL) {
		ffscMsg("<NULL>");
	}
	else {
		ffscMsg(Ptr);
	}

	return OK;
}


/*
 * showpeer
 *	Displays info about the other end of the specified socket FD
 */
STATUS
showpeer(int SD)
{
	char IPAddrStr[INET_ADDR_LEN];
	int SockAddrLen;
	struct sockaddr_in SockAddr;

	SockAddrLen = sizeof(SockAddr);
	if (getpeername(SD, (struct sockaddr *) &SockAddr, &SockAddrLen) != OK)
	{
		ffscMsgS("Unable to get info about peer to SD %d", SD);
		return ERROR;
	}

	inet_ntoa_b(SockAddr.sin_addr, IPAddrStr);
	ffscMsg("SD %d   IP %s   Port %d", SD, IPAddrStr, SockAddr.sin_port);

	return OK;
}


/*
 * netinfo
 *	Show info about networking
 */
STATUS
netinfo(netinfo_t *NetInfo)
{
	if (NetInfo == NULL) {
		NetInfo = &ffscNetInfo;
	}

	ffscMsg("NETWORK INFORMATION\r\n"
		    "    Host Address  0x%08x\r\n"
		    "    Network Addr  0x%x\r\n"
		    "    Netmask       0x%08x",
		NetInfo->HostAddr,
		NetInfo->NetAddr,
		NetInfo->NetMask);

	return OK;
}

#endif /* !PRODUCTION */
