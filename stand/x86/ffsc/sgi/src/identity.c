/*
 * identity.c
 *	Functions for handling the FFSC identity table
 */
#include "vxWorks.h"
#include "inetLib.h"
#include "lstLib.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ffsc.h"

#include "addrd.h"
#include "identity.h"
#include "misc.h"
#include "net.h"
#include "nvram.h"
#include "route.h"


/* Internal type declarations */
typedef struct identity_node {
	void		*lstlib1, *lstlib2;	/* Used by lstLib */
	timestamp_t	lastUpdate;		/* Timestamp of last update */
	identity_t	identity;		/* Identity info */
} identity_node_t;


/* Handy defines */
#define AddNode(L, N)	 lstAdd(L, (NODE *) N)
#define DeleteNode(L, N) lstDelete(L, (NODE *) N)
#define FirstNode(L)	 ((identity_node_t *) lstFirst(L))
#define NextNode(N)	 ((identity_node_t *) lstNext((NODE *) N))


/* Global variables */
static LIST identTable;		/* System identity table list descriptor */


/* Internal functions */
static identity_node_t *identFindIdentityNodeByUniqueID(uniqueid_t);


/*----------------------------------------------------------------------*/
/*									*/
/*			IDENTITY TABLE FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * identCleanOldEntries
 *	Remove any entries in the identity table that are older than
 *	the specified timestamp. Returns OK/ERROR.
 */
STATUS
identCleanOldEntries(timestamp_t *StaleTime)
{
	identity_node_t *Curr, *Next;

	Curr = FirstNode(&identTable);
	while (Curr != NULL) {
		Next = NextNode(Curr);
		if (ffscCompareTimestamps(&Curr->lastUpdate, StaleTime) < 0  &&
		    ffscCompareUniqueIDs(Curr->identity.uniqueid,
					 ffscLocal.uniqueid) != 0)
		{
			if (ffscCompareUniqueIDs(Curr->identity.uniqueid,
						 ffscMaster->uniqueid) == 0)
			{
				ffscMaster = NULL;
				ffscIsMaster = 0;
			}
			DeleteNode(&identTable, Curr);
			free(Curr);
		}
		Curr = Next;
	}

	return OK;
}


/*
 * identClearTable
 *	Purge all entries from the specified identity table
 */
STATUS
identClearTable(void)
{
	identity_node_t *CurrNode;

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = FirstNode(&identTable))
	{
		DeleteNode(&identTable, CurrNode);
		free(CurrNode);
	}

	return OK;
}


/*
 * identEnumerateModules
 *	Stores a sorted list of all currently known module #'s in the
 *	specified array (not to exceed a specified maximum number of
 *	values) and returns the number actually stored.
 */
int
identEnumerateModules(modulenum_t *List, int MaxEntries)
{
	identity_node_t *CurrNode;
	int i, j;
	int NumEntries;
	modulenum_t Swap;

	for (NumEntries = 0, CurrNode = FirstNode(&identTable);
	     NumEntries <= MaxEntries  &&  CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		for (i = 0;  i < MAX_BAYS  &&  NumEntries <= MaxEntries;  ++i)
		{
			if (CurrNode->identity.modulenums[i]
			    != MODULENUM_UNASSIGNED)
			{
				List[NumEntries] =
				    CurrNode->identity.modulenums[i];
				++NumEntries;
			}
		}
	}

	for (i = 0;  i < NumEntries - 1;  ++i) {
		for (j = i + 1;  j < NumEntries;  ++j) {
			if (List[j] < List[i]) {
				Swap = List[i];
				List[i] = List[j];
				List[j] = Swap;
			}
		}
	}

	return NumEntries;
}


/*
 * identEnumerateRacks
 *	Stores a sorted list of all currently known rack IDs in the
 *	specified array (not to exceed a specified maximum number of
 *	values) and returns the number actually stored.
 */
int
identEnumerateRacks(rackid_t *List, int MaxEntries)
{
	identity_node_t *CurrNode;
	int i, j;
	int NumEntries;
	rackid_t Swap;

	for (NumEntries = 0, CurrNode = FirstNode(&identTable);
	     NumEntries <= MaxEntries  &&  CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (CurrNode->identity.rackid != RACKID_UNASSIGNED) {
			List[NumEntries] = CurrNode->identity.rackid;
			++NumEntries;
		}
	}

	for (i = 0;  i < NumEntries - 1;  ++i) {
		for (j = i + 1;  j < NumEntries;  ++j) {
			if (List[j] < List[i]) {
				Swap = List[i];
				List[i] = List[j];
				List[j] = Swap;
			}
		}
	}

	return NumEntries;
}


/*
 * identFindByIPAddr
 *	Searches the identity table for the specified IP address and
 *	returns a pointer to a matching identity_t, or NULL if none
 *	was found.
 */
identity_t *
identFindByIPAddr(struct in_addr *Addr)
{
	identity_node_t *CurrNode;

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (CurrNode->identity.ipaddr.s_addr == Addr->s_addr) {
			return &CurrNode->identity;
		}
	}

	return NULL;
}


/*
 * identFindByModuleNum
 *	Searches the identity table for the specified module # and
 *	returns a pointer to a matching identity_t, or NULL if none
 *	was found. If a bayid_t pointer is passed in, then the bay ID
 *	corresponding to the matching module # is stored in it. Note
 *	that you cannot match on MODULENUM_UNASSIGNED.
 */
identity_t *
identFindByModuleNum(modulenum_t ModuleNum, bayid_t *MatchingBayID)
{
	identity_node_t *CurrNode;

	if (ModuleNum == MODULENUM_UNASSIGNED) {
		return NULL;
	}

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		int i;

		for (i = 0;  i < MAX_BAYS;  ++i) {
			if (CurrNode->identity.modulenums[i] == ModuleNum) {
				if (MatchingBayID != NULL) {
					*MatchingBayID = i;
				}

				return &CurrNode->identity;
			}
		}
	}

	return NULL;
}


/*
 * identFindByRackID
 *	Searches the identity table for the specified rack ID and
 *	returns a pointer to a matching identity_t, or NULL if none
 *	was found. Note that you cannot match on the rack ID
 *	RACKID_UNASSIGNED.
 */
identity_t *
identFindByRackID(rackid_t RackID)
{
	identity_node_t *CurrNode;

	if (RackID == RACKID_UNASSIGNED) {
		return NULL;
	}

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (CurrNode->identity.rackid == RackID) {
			return &CurrNode->identity;
		}
	}

	return NULL;
}


/*
 * identFindByUniqueID
 *	Searches the identity table for the specified unique ID and
 *	returns a pointer to a matching identity_t, or NULL if none
 *	was found.
 */
identity_t *
identFindByUniqueID(uniqueid_t UniqueID)
{
	identity_node_t *Node;

	Node = identFindIdentityNodeByUniqueID(UniqueID);

	return (Node == NULL) ? NULL : &Node->identity;
}


/*
 * identFindConflictingIPAddr
 *	Searches the identity table for an entry other than the specified
 *	one that has a matching IP address. Returns a pointer to the entry
 *	or NULL if none was found.
 */
identity_t *
identFindConflictingIPAddr(identity_t *Ident)
{
	identity_node_t *CurrNode;

	if (Ident == NULL) {
		return NULL;
	}

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (ffscCompareUniqueIDs(CurrNode->identity.uniqueid,
					 Ident->uniqueid) == 0)
		{
			continue;
		}

		if (CurrNode->identity.ipaddr.s_addr == Ident->ipaddr.s_addr) {
			return &CurrNode->identity;
		}
	}

	return NULL;
}


/*
 * identFindConflictingRackID
 *	Searches the identity table for an entry other than the specified
 *	one that has a matching rack ID. Returns a pointer to the entry
 *	or NULL if none was found.
 */
identity_t *
identFindConflictingRackID(identity_t *Ident)
{
	identity_node_t *CurrNode;

	if (Ident == NULL  ||  Ident->rackid == RACKID_UNASSIGNED) {
		return NULL;
	}

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (ffscCompareUniqueIDs(CurrNode->identity.uniqueid,
					 Ident->uniqueid) == 0)
		{
			continue;
		}

		if (CurrNode->identity.rackid == Ident->rackid) {
			return &CurrNode->identity;
		}
	}

	return NULL;
}


/*
 * identFindMaster
 *	Searches the identity table for the lowest unique ID and
 *	returns a pointer to the corresponding identity_t, or NULL
 *	if the table is empty. Note that this completely ignores
 *	the "ffscMaster" variable.
 */
identity_t *
identFindMaster(void)
{
	identity_node_t *CurrNode, *Lowest;

	/* Start off by assuming the first entry is the master */
	Lowest = FirstNode(&identTable);
	if (Lowest == NULL) {
		return NULL;
	}

	/* Search the remaining entries for a lower unique ID */
	for (CurrNode = NextNode(Lowest);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (ffscCompareUniqueIDs(CurrNode->identity.uniqueid,
					 Lowest->identity.uniqueid)
		    < 0)
		{
			Lowest = CurrNode;
		}
	}

	return &Lowest->identity;
}


/*
 * identInit
 *	Initialize our own identity and the identity table
 */
STATUS
identInit(void)
{
	int i;

	/* Start with no identity info at all */
	bzero((char *) &ffscLocal, sizeof(ffscLocal));
	ffscMaster   = NULL;
	ffscIsMaster = 0;

	/* Generate a unique ID for this system from the MAC address */
	ffscLocal.uniqueid[0] = '\000';
	ffscLocal.uniqueid[1] = '\000';
	for (i = 0;  i < 6;  ++i) {
		ffscLocal.uniqueid[i+2] = ffscMACAddr[i];
	}

	/* For now, none of our bays have module #'s */
	for (i = 0;  i < MAX_BAYS;  ++i) {
		ffscLocal.modulenums[i] = MODULENUM_UNASSIGNED;
	}

	/* Indicate what we are capable of doing */
	ffscLocal.capabilities = CAP_USERINFO;

	/* Try to obtain the network parameters from NVRAM */
	if (nvramRead(NVRAM_NETINFO, &ffscNetInfo) != OK) {

		/* If that didn't work (probably because this is a new, */
		/* uninitialized FFSC), generate new parameters from	*/
		/* our unique ID. If it just so happens to exceed the	*/
		/* allowable range of IP host addresses, use a special  */
		/* "unassigned" address for now and it will be resolved */
		/* into something better later on.			*/
		if (ffscDebug.Flags & FDBF_IDENT) {
			ffscMsg("Using default network parameters");
		}

		ffscNetInfo.HostAddr = ((ffscLocal.uniqueid[5] * 256 * 256)
					+ (ffscLocal.uniqueid[6] * 256)
					+ ffscLocal.uniqueid[7]);
		if (ffscNetInfo.HostAddr < FFSC_MIN_HOSTADDR  ||
		    ffscNetInfo.HostAddr > FFSC_MAX_HOSTADDR)
		{
			ffscNetInfo.HostAddr = FFSC_NO_HOSTADDR;
		}

		ffscNetInfo.NetAddr = FFSC_NETWORK_ADDR;
		ffscNetInfo.NetMask = FFSC_NETMASK;

		(void) nvramWrite(NVRAM_NETINFO, &ffscNetInfo);
	}

	/* Build a proper IP address */
	inet_makeaddr_b(ffscNetInfo.NetAddr,
			ffscNetInfo.HostAddr,
			&ffscLocal.ipaddr);

	/* Obtain our rack ID from NVRAM if possible */
	if (nvramRead(NVRAM_RACKID, &ffscLocal.rackid) != OK) {
		ffscLocal.rackid = RACKID_UNASSIGNED;
	}
	if (ffscLocal.rackid > (MAX_RACKS-1) )
	  ffscLocal.rackid = 1;

	/* Initialize the identity table structure */
	lstInit(&identTable);

	/* Start the identity table off with our own identity */
	if (identUpdate(&ffscLocal) < 0) {
		return ERROR;
	}

	return OK;
}


/*
 * identPrintTable
 *	Print the entries in the system identity table to the
 *	debugging console. Returns the number of entries.
 */
int
identPrintTable(void)
{
	identity_node_t *CurrNode;

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		ffscMsg("\r\nTIMESTAMP %d.%09d",
			CurrNode->lastUpdate.tv_sec,
			CurrNode->lastUpdate.tv_nsec);
		identPrint(&CurrNode->identity);
	}

	return lstCount(&identTable);
}


/*
 * identScanForConflicts
 *	Searches each entry in the identity table looking for conflicting
 *	identifiers. For each pair of entries with a conflicting identifier,
 *	a specified resolution function is called. The Conflict Resolution
 *	Function ("CRF") should return < 0 if an error has occurred (meaning
 *	the scan should stop completely), 0 if the scan should be restarted
 *	from the beginning of the table (appropriate if an entry was changed)
 *	or 1 if the scan should resume with the next table entry (usually
 *	only appropriate if no change was made).
 */
STATUS
identScanForConflicts(CRFPtr IPAddrCRF, CRFPtr RackIDCRF)
{
	identity_node_t *Left, *Right;
	int Result;
	int Restart;

	/* Start by looking for conflicts IP addresses */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("Searching for conflicting IP addresses...");
	}
	do {
		Restart = 0;
		for (Left = FirstNode(&identTable);
		     Left != NULL  &&  !Restart;
		     Left = NextNode(Left))
		{
			/* If this entry has a bogus IP address, force it to */
			/* get a proper IP address by pretending that it is  */
			/* in conflict with itself.			     */
			if (inet_lnaof(Left->identity.ipaddr.s_addr)
			    < FFSC_MIN_HOSTADDR)
			{
				Result = (*IPAddrCRF)(&Left->identity,
						      &Left->identity);
				if (Result < 0) {
					return ERROR;
				}
				else if (Result == 0) {
					Restart = 1;
				}

				continue;
			}

			/* Sift through the other entries looking for */
			/* matching IP addresses		      */
			for (Right = NextNode(Left);
			     Right != NULL  &&  !Restart;
			     Right = NextNode(Right))
			{
				/* Don't bother with bogus addresses */
				if (inet_lnaof(Right->identity.ipaddr.s_addr)
				    < FFSC_MIN_HOSTADDR)
				{
					continue;
				}

				/* Conflict if the IP addresses match */
				if (Left->identity.ipaddr.s_addr ==
				    Right->identity.ipaddr.s_addr)
				{
					Result = (*IPAddrCRF)(&Left->identity,
							     &Right->identity);
					if (Result < 0) {
						return ERROR;
					}
					else if (Result == 0) {
						Restart = 1;
					}
				}
			}
		}
	} while (Restart);

	/* Now search for conflicting rack IDs */
	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("Searching for conflicting rack IDs...");
	}
	do {
		Restart = 0;
		for (Left = FirstNode(&identTable);
		     Left != NULL  &&  !Restart;
		     Left = NextNode(Left))
		{
			/* If this entry has an unassigned rack ID, force */
			/* it to get a new one by pretending it conflicts */
			/* with itself.					  */
			if (Left->identity.rackid == RACKID_UNASSIGNED) {
				Result = (*RackIDCRF)(&Left->identity,
						      &Left->identity);
				if (Result < 0) {
					return ERROR;
				}
				else if (Result == 0) {
					Restart = 1;
				}

				continue;
			}

			/* Sift through the other entries looking for */
			/* matching rack IDs			      */
			for (Right = NextNode(Left);
			     Right != NULL  &&  !Restart;
			     Right = NextNode(Right))
			{
				/* Conflict if the rack IDs match */
				if (Left->identity.rackid ==
				    Right->identity.rackid)
				{
					Result = (*RackIDCRF)(&Left->identity,
							     &Right->identity);
					if (Result < 0) {
						return ERROR;
					}
					else if (Result == 0) {
						Restart = 1;
					}
				}
			}
		}
	} while (Restart);

	if (ffscDebug.Flags & FDBF_ADDRD) {
		ffscMsg("Conflict resolution completed.");
	}

	return OK;
}


/*
 * identUpdate
 *	Searches the identity table for an entry with a unique ID matching
 *	the one in the specified identity_t, then updates the remaining
 *	identifiers in that entry. If no entry is found with a matching
 *	unique ID, then a new entry is created.
 *
 *	Returns:
 *		-1: error
 *		0:  updated existing entry
 *		1:  added new entry
 */
int
identUpdate(identity_t *NewIdentity)
{
	identity_node_t *Node;
	int Result;

	/* See if a matching entry already exists */
	Node = identFindIdentityNodeByUniqueID(NewIdentity->uniqueid);

	/* If a matching node already exists, update the identifiers */
	if (Node != NULL) {
		if (clock_gettime(CLOCK_REALTIME, &Node->lastUpdate) != OK)
		{
			ffscMsgS("Unable to update timestamp in identity "
				     "table entry");
			return -1;
		}

		Node->identity = *NewIdentity;

		Result = 0;
	}

	/* Otherwise add a new entry */
	else {
		Node = malloc(sizeof(identity_node_t));
		if (Node == NULL) {
			ffscMsgS("Unable to allocate memory for identity "
				     "table entry");
			return -1;
		}

		if (clock_gettime(CLOCK_REALTIME, &Node->lastUpdate) != OK) {
			ffscMsgS("Unable to set timestamp for new identity "
				     "table entry");
			free(Node);
			return -1;
		}

		Node->identity = *NewIdentity;

		AddNode(&identTable, Node);

		Result = 1;
	}

	return Result;
}


/*----------------------------------------------------------------------*/
/*									*/
/*			    IDENTIFIER FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/


#ifndef PRODUCTION
/*
 * identParse
 *	Parses identity information from the given string and stores it
 *	in the specified identity_t. Unspecified fields are set to the
 *	current values for this FFSC. Returns ERROR if any field could
 *	not be parsed, or OK if successful.
 *
 *	The format of an identity string is:
 *		r [iii.iii.iii.iii [uu:uu:uu:uu:uu:uu:uu:uu [m1 m2 [ccc]]]]
 */
STATUS
identParse(char *Args, identity_t *Identity)
{
	char IPAddr[INET_ADDR_LEN], RackID[16], UniqueID[UNIQUEID_STRLEN];
	char ModuleNum1[MODULENUM_STRLEN], ModuleNum2[MODULENUM_STRLEN];
	int  NumFound;
	unsigned int CapFlags;

	/* Assume nothing is here */
	*Identity = ffscLocal;

	/* Bail out now if no args were specified */
	if (Args == NULL) {
		return OK;
	}

	/* Isolate individual fields */
	NumFound = sscanf(Args, "%s %s %s %s %s %u",
			  RackID, IPAddr, UniqueID,
			  ModuleNum1, ModuleNum2, &CapFlags);

	/* If there is at least one argument, then we have a rack ID */
	if (NumFound >= 1) {
		if (ffscParseRackID(RackID, &Identity->rackid) != OK) {
			return ERROR;
		}
	}

	/* If there are at least two args, then an IP address is present */
	if (NumFound >= 2) {
		if (ffscParseIPAddr(IPAddr, &Identity->ipaddr) != OK) {
			return ERROR;
		}
	}

	/* If there are three args, then a unique ID is present */
	if (NumFound >= 3) {
		if (ffscParseUniqueID(UniqueID, &Identity->uniqueid) != OK) {
			return ERROR;
		}
	}

	/* If there are five args, then module #'s are present. Since  */
	/* this is a debugging function, we won't bother with the more */
	/* general case of an arbitrary number of bays/rack.	       */
	if (NumFound >= 5) {
		if (ffscParseModuleNum(ModuleNum1, &Identity->modulenums[0])
		    != OK)
		{
			return ERROR;
		}

		if (ffscParseModuleNum(ModuleNum2, &Identity->modulenums[1])
		    != OK)
		{
			return ERROR;
		}
	}

	/* If there is a sixth arg, then it is the capability flags */
	if (NumFound >= 6) {
		Identity->capabilities = CapFlags;
	}

	return OK;
}
#endif /* !PRODUCTION */


/*
 * identPrint
 *	Print the specified identity_t to the FFSC debug console
 */
STATUS
identPrint(identity_t *Identity)
{
	char AllModuleNums[(MODULENUM_STRLEN + 1) * MAX_BAYS];
	char ModuleNumBuf[MODULENUM_STRLEN];
	char NtoABuf[INET_ADDR_LEN];
	char RackIDBuf[RACKID_STRLEN];
	char UniqueBuf[UNIQUEID_STRLEN];
	int  i;
	int  IsMaster = 0;

	if (ffscMaster != NULL &&
	    ffscCompareUniqueIDs(ffscMaster->uniqueid,
				 Identity->uniqueid)   == 0)
	{
		IsMaster = 1;
	}

	ffscUniqueIDToString(Identity->uniqueid, UniqueBuf);
	inet_ntoa_b(Identity->ipaddr, NtoABuf);
	ffscRackIDToString(Identity->rackid, RackIDBuf);

	AllModuleNums[0] = '\0';
	for (i = 0;  i < MAX_BAYS;  ++i) {
		ffscModuleNumToString(Identity->modulenums[i], ModuleNumBuf);
		strcat(AllModuleNums, " ");
		strcat(AllModuleNums, ModuleNumBuf);
	}

	ffscMsg("%s%s\r\n"
	            "    Rack ID:  %s\r\n"
		    "    IP Addr:  %s\r\n"
		    "    Modules: %s\r\n"
		    "    CapFlags: 0x%08x",
		UniqueBuf,
		(IsMaster ? " MASTER" : ""),
		RackIDBuf,
		NtoABuf,
		AllModuleNums,
		Identity->capabilities);

	return OK;
}


/*
 * identSetCapabilities
 *	Set the capability flags for this FFSC.
 *	Notice that this does NOT update any other FFSCs.
 */
STATUS
identSetCapabilities(unsigned int NewCaps)
{
	ffscLocal.capabilities = NewCaps;

	if (ffscDebug.Flags & FDBF_IDENT) {
		ffscMsg("Local capability flags changed to 0x%08x", NewCaps);
	}

	return OK;
}


/*
 * identSetIPAddr
 *	Set the IP address of this FFSC and update NVRAM accordingly.
 *	Notice that neither the identity table nor other FFSCs are updated.
 */
STATUS
identSetIPAddr(struct in_addr *NewAddr)
{
	char NtoABuf[INET_ADDR_LEN];
	int  NewHostAddr, NewNetAddr;

	/* Isolate the new host & network addresses */
	NewHostAddr = inet_lnaof(NewAddr->s_addr);
	NewNetAddr  = inet_netof(*NewAddr);

	/* Warn about a changing network address (should "never" happen) */
	if (NewNetAddr != ffscNetInfo.NetAddr) {
		ffscMsg("WARNING: MMSC network address changed");
	}

	/* Update the interface itself. Note that we assume the */
	/* netmask is unchanged, at least for now.		*/
	inet_ntoa_b(*NewAddr, NtoABuf);
	if (ifAddrSet(FFSC_NET_INTERFACE, NtoABuf) != OK) {
		ffscMsgS("Unable to change IP address on "
			 FFSC_NET_INTERFACE " to %s",
			 NtoABuf);
		return ERROR;
	}

	/* Reset the global variables */
	ffscLocal.ipaddr     = *NewAddr;
	ffscNetInfo.NetAddr  = NewNetAddr;
	ffscNetInfo.HostAddr = NewHostAddr;

	/* Update the NVRAM settings */
	if (nvramWrite(NVRAM_NETINFO, &ffscNetInfo) != OK) {
		ffscMsg("WARNING: network information changed but not "
			"updated in NVRAM");
	}

	/* Say what has happened */
	if (ffscDebug.Flags & FDBF_IDENT) {
		ffscMsg("IP address has been changed to %s", NtoABuf);
	}

	return OK;
}


/*
 * identSetModuleNums
 *	Set the module numbers of the ELSCs attached to this FFSC.
 *	Notice that this does NOT update any other FFSCs.
 */
STATUS
identSetModuleNums(modulenum_t NewNums[MAX_BAYS])
{
	bayid_t CurrBay;

	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		ffscLocal.modulenums[CurrBay] = NewNums[CurrBay];
	}

	if (ffscDebug.Flags & FDBF_IDENT) {
		char ModuleNumBuf[MODULENUM_STRLEN];

		ffscMsgN("Module numbers changed to:");
		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
			ffscModuleNumToString(ffscLocal.modulenums[CurrBay],
					      ModuleNumBuf);
			ffscMsgN(" %s", ModuleNumBuf);
		}
		ffscMsg("");
	}

	return OK;
}


/*
 * identSetRackID
 *	Set the rackid of this FFSC and update NVRAM accordingly.
 *	Notice that this does NOT update any other FFSCs.
 */
STATUS
identSetRackID(rackid_t NewID)
{
	/* Update our identity info */
	ffscLocal.rackid = NewID;

	/* Update NVRAM with our new rack ID */
	if (nvramWrite(NVRAM_RACKID, &ffscLocal.rackid) != OK) {
		ffscMsg("WARNING: Rack ID changed but not updated in NVRAM");
	}

	/* Tell the router that our rack ID has changed */
	routeRebuildResponses = 1;

	/* Say what has happened */
	if (ffscDebug.Flags & FDBF_IDENT) {
		char RackIDBuf[RACKID_STRLEN];

		ffscRackIDToString(NewID, RackIDBuf);
		ffscMsg("Rack ID has been changed to %s", RackIDBuf);
	}

	return OK;
}
	


/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * identFindIdentityNodeByUniqueID
 *	Searches the identity table for the specified unique ID and
 *	returns a pointer to a matching identity_node_t, or NULL if none
 *	was found.
 */
identity_node_t *
identFindIdentityNodeByUniqueID(uniqueid_t UniqueID)
{
	identity_node_t *CurrNode;

	for (CurrNode = FirstNode(&identTable);
	     CurrNode != NULL;
	     CurrNode = NextNode(CurrNode))
	{
		if (ffscCompareUniqueIDs(CurrNode->identity.uniqueid, UniqueID)
		    == 0)
		{
			return CurrNode;
		}
	}

	return NULL;
}
