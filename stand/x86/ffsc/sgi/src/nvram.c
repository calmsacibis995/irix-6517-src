/*
 * FFSC NVRAM access functions
 */

#include <vxworks.h>
#include <stdio.h>
#include <string.h>

#include "sram.h"

#include "ffsc.h"

#include "console.h"
#include "elsc.h"
#include "log.h"
#include "net.h"
#include "nvram.h"
#include "tty.h"
#include "user.h"
#include "misc.h"


/*
 * NVRAM Object Table
 *	Any new NVRAM objects should be added to this table as well as
 *	the list of object IDs in ffscnvram.h. Each entry consists of the
 *	object ID and its size.
 *
 *	WARNING: The size of objects in this table MUST NEVER CHANGE.
 *		 If an object shrinks, pad it back to the original size;
 *		 if an object grows, split it into two separate objects.
 */
typedef struct noi {
	nvramid_t	ID;	/* NVRAM object ID (from ffscnvram.h) */
	size_t		Size;	/* Size of object in bytes */
} noi_t;	/* NVRAM Object Info */

noi_t nvramObjectInfo[] = {
       /* OBJECT ID             OBJECT SIZE */
	{ NVRAM_HDR,		sizeof(nvramhdr_t) },
	{ NVRAM_NETINFO,	sizeof(netinfo_t) },
	{ NVRAM_RACKID,		sizeof(rackid_t) },
	{ NVRAM_DEBUGINFO,	sizeof(debuginfo_t) },
	{ NVRAM_TTYINFO,	(sizeof(ttyinfo_t) * FFSC_NUM_TTYS) },
	{ NVRAM_ELSCINFO,	(sizeof(elsc_save_t) * MAX_BAYS) },
	{ NVRAM_CONSTERM,	sizeof(consoleinfo_t) },
	{ NVRAM_CONSALT,	sizeof(consoleinfo_t) },
	{ NVRAM_TUNE,		(sizeof(int) * TUNE_MAX) },
	{ NVRAM_LOGINFO,	(sizeof(loginfo_t) * MAX_LOGS) },
	{ NVRAM_USERTERM,	sizeof(userinfo_t) },
	{ NVRAM_USERALT,	sizeof(userinfo_t) },
	{ NVRAM_PASSINFO,	sizeof(passinfo_t) },
};

#define NOI_NUM_ENTRIES (sizeof(nvramObjectInfo) / sizeof(noi_t))


/* Internal type declarations */
typedef struct nie {
	uint32_t	Offset;		/* Object offset */
	size_t		Size;		/* Object size */
} nie_t;	/* NVRAM Index Entry */


/* Internal global variables */
nie_t		nvramIndex[NOI_NUM_ENTRIES];
nvramhdr_t	nvramHdr;
size_t		nvramSize;


/* Internal functions */
static BOOL   nvramIsValid(nvramid_t);
static STATUS nvramUpgrade1to2(void);
static STATUS nvramUpgrade2to3(void);
static STATUS nvramUpgrade3to4(void);
static STATUS nvramUpgrade4to5(void);
static void   nvramValidate(nvramid_t);
static STATUS nvramWriteHeader(void);


/* Raw SRAM access functions, since CDI didn't provide these */
int sram_read(char *, int, int);
int sram_write(char *, int, int);



/*
 * nvramInit
 *	Initialization for NVRAM access. Build the NVRAM index, then do
 *	several sanity checks on the header info. If the NVRAM has not
 *	been initialized, then initialize it. Note that this is called
 *	before ffscDebugInit, so we cannot use ffscMsg or friends.
 */
STATUS
nvramInit(void)
{
	int i;
	int Result;
	nvramid_t CurrID;
	size_t    CurrSize;
	uint32_t  CurrOffset;

	/* Clear out the in-memory NVRAM header. This way, if anything	*/
	/* fails, all NVRAM objects will appear to be invalid.		*/
	bzero((char *) &nvramHdr, sizeof(nvramhdr_t));

	/* Initialize the NVRAM index */
	bzero((char *) nvramIndex, sizeof(nvramIndex));
	CurrOffset = 0;
	for (i = 0;  i < NOI_NUM_ENTRIES;  ++i) {

		/* Convenient copies */
		CurrID   = nvramObjectInfo[i].ID;
		CurrSize = nvramObjectInfo[i].Size;

		/* Make sure the ID is in range */
		if (CurrID >= NOI_NUM_ENTRIES) {
			fprintf(stderr, "Bogus NVRAM Object ID %d\n", CurrID);
			continue;
		}

		/* Make sure we haven't already seen this ID */
		if (nvramIndex[CurrID].Size > 0) {
			fprintf(stderr,
				"Duplicate NVRAM Object Info for ID %d\n",
				CurrID);
			continue;
		}

		/* Make sure the size is reasonable */
		if (CurrSize == 0) {
			fprintf(stderr,
				"Bogus NVRAM object size of %d for ID %d\n",
				CurrSize, CurrID);
			continue;
		}

		/* Make sure there is enough room for the object */
		if (CurrOffset + CurrSize > BUFF_SIZE) {
			fprintf(stderr,
				"NVRAM object ID %d does not fit in NVRAM\n",
				CurrID);
			continue;
		}

		/* Our paranoia is satisfied for now. Add the index entry. */
		nvramIndex[CurrID].Offset = CurrOffset;
		nvramIndex[CurrID].Size   = CurrSize;

		/* Update the current offset */
		CurrOffset += CurrSize;
	}

	/* At this point, the current offset is the same as the size */
	/* of known NVRAM data.					     */
	nvramSize = CurrOffset;

	/* Read the NVRAM header. Since we are not yet satisfied that */
	/* NVRAM is healthy we call sram_read directly rather than    */
	/* use our normal functions.				      */
	Result = sram_read((char *) &nvramHdr, sizeof(nvramhdr_t), 0);
	if (Result == NOT_INITIALIZED  &&  nvramReset() != OK) {
		return ERROR;
	}
	else if (Result != VALID_READ) {
		fprintf(stderr, "NVRAM error %d\n", Result);
		return ERROR;
	}

	/* If the NVRAM header does not contain a valid signature, */
	/* it is apparently uninitialized, so go initialize it.	   */
	if (nvramHdr.Signature != NH_SIGNATURE) {
		if (nvramReset() != OK) {
			return ERROR;
		}
	}

	/* Make sure we recognize the version number */
	if (nvramHdr.Version != NH_CURR_VERSION) {

		/* Go through the steps needed to upgrade to the latest */
		/* version of NVRAM data.				*/
		switch (nvramHdr.Version) {

		    case 1:
			if (nvramUpgrade1to2() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}
		    case 2:
			if (nvramUpgrade2to3() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}
		    case 3:
			if (nvramUpgrade3to4() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}
		    case 4:
			if (nvramUpgrade4to5() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}
		    case 5:
			if (nvramUpgrade5to6() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}

			/* Fall through... */

		    /* Additional upgrades go here in order, with each	*/
		    /* falling through to the next higher version.	*/

		    case NH_CURR_VERSION:
			if (nvramWriteHeader() != OK) {
				nvramHdr.Signature = 0;
				return ERROR;
			}
			break;

		    default:
			/* Apparently, we just don't grok this version */
			fprintf(stderr,
				"Unrecognized NVRAM version %ld\n",
				nvramHdr.Version);
			nvramHdr.Signature = 0;
			return ERROR;
		}
	}

	/* If the number and size of the NVRAM objects has not changed,	*/
	/* then we are finished for now.				*/
	if (nvramHdr.NumObjs == NOI_NUM_ENTRIES  &&
	    nvramHdr.Size == nvramSize)
	{
		return OK;
	}

	/* If the number of objects has not changed but their size has, */
	/* we cannot go on. (Probably should have rolled the version #) */
	if (nvramHdr.NumObjs == NOI_NUM_ENTRIES  &&
	    nvramHdr.Size != nvramSize)
	{
		fprintf(stderr, "Size of NVRAM data has changed\n");
		nvramHdr.Signature = 0;
		return ERROR;
	}

	/* It is equally lethal if the number of objects has decreased */
	if (nvramHdr.NumObjs > NOI_NUM_ENTRIES) {
		fprintf(stderr, "Number of NVRAM objects has DECREASED\n");
		nvramHdr.Signature = 0;
		return ERROR;
	}

	/* It's starting to look like new objects have been added to	*/
	/* the end of the list. Double check this by making sure that   */
	/* the old size matches the sum we get from adding up the sizes	*/
	/* of the old number of objects.				*/
	CurrSize = 0;
	for (CurrID = 0;  CurrID < nvramHdr.NumObjs;  ++CurrID) {
		CurrSize += nvramIndex[CurrID].Size;
	}
	if (CurrSize != nvramHdr.Size) {
		fprintf(stderr, "Size of old NVRAM objects has changed\n");
		nvramHdr.Signature = 0;
		return ERROR;
	}

	/* It looks like the only thing that has happened was that new	*/
	/* objects have been added. Update the header to account for	*/
	/* this, then move along.					*/
	nvramHdr.Size    = nvramSize;
	nvramHdr.NumObjs = NOI_NUM_ENTRIES;
	if (nvramWriteHeader() != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * nvramPrintInfo
 *	Prints fascinating information about the NVRAM
 */
STATUS
nvramPrintInfo(void)
{
	int i, j, bit;

	if (nvramHdr.Signature != NH_SIGNATURE) {
		ffscMsg("NVRAM is currently uninitialized");
		return OK;
	}

	ffscMsg("MMSC NVRAM Layout Version %d", nvramHdr.Version);
	ffscMsg("%d objects (%d bytes) are defined",
		nvramHdr.NumObjs, nvramHdr.Size);

	ffscMsgN("Valid NVRAM objects:");
	for (i = 0;  i < NH_VALID_WORDS;  ++i) {
		for (j = 0, bit = 1;  j < 32;  ++j, bit <<= 1) {
			if (nvramHdr.Valid[i] & bit) {
				ffscMsgN(" %d", i * 32 + j);
			}
		}
	};
	ffscMsg("");	/* For \n only */

	return OK;
}


/*
 * nvramRead
 *	Read an object from NVRAM and copy it to the specified location.
 *	Note that the buffer is assumed to have the correct length.
 */
STATUS
nvramRead(nvramid_t ID, void *Buffer)
{
	int Result;

	/* Make sure the object ID is legitimate */
	if (ID >= NOI_NUM_ENTRIES  ||  nvramIndex[ID].Size < 1) {
		ffscMsg("Tried to read bogus NVRAM object ID %d", ID);
		return ERROR;
	}

	/* Make sure NVRAM is healthy */
	if (nvramHdr.Signature != NH_SIGNATURE) {
		ffscMsg("Unable to read NVRAM - currently uninitialized");
		return ERROR;
	}

	/* Make sure the object is valid */
	if (!nvramIsValid(ID)) {
		/* No error message since this may be "normal" */
		return ERROR;
	}

	/* Read the object from NVRAM */
	Result = sram_read((char *) Buffer,
			   nvramIndex[ID].Size,
			   nvramIndex[ID].Offset);
	if (Result != VALID_READ) {
		ffscMsg("Unable to read object from NVRAM: RC %d", Result);
		return ERROR;
	}

	return OK;
}


/*
 * nvramReset
 *	Initialize the FFSC NVRAM to a known state. Note that this function
 *	may be called before ffscDebugInit, so it cannot use ffscMsg.
 */
STATUS
nvramReset(void)
{
	/* Set up a clean header */
	bzero((char *) &nvramHdr, sizeof(nvramhdr_t));
	nvramHdr.Signature = NH_SIGNATURE;
	nvramHdr.Version   = NH_CURR_VERSION;
	nvramHdr.NumObjs   = NOI_NUM_ENTRIES;
	nvramHdr.Size	   = nvramSize;
	nvramValidate(NVRAM_HDR);

	/* Write the header manually */
	if (nvramWriteHeader() != OK) {
		return ERROR;
	}

	/* This would be a good place to initialize important objects */

	return OK;
}


/*
 * nvramWrite
 *	Write a specified object into NVRAM and mark it as valid in the
 *	NVRAM header if it is not already valid.
 */
STATUS
nvramWrite(nvramid_t ID, void *Buffer)
{
	int Result;

	/* Make sure the object ID is legitimate */
	if (ID >= NOI_NUM_ENTRIES  ||  nvramIndex[ID].Size < 1) {
		ffscMsg("Tried to write bogus NVRAM object ID %d", ID);
		return ERROR;
	}

	/* Make sure NVRAM is healthy */
	if (nvramHdr.Signature != NH_SIGNATURE) {
		ffscMsg("Unable to write NVRAM - currently uninitialized");
		return ERROR;
	}

	/* Write the data into NVRAM */
	Result = sram_write(Buffer,
			    nvramIndex[ID].Size,
			    nvramIndex[ID].Offset);
	if (Result != VALID_WRITE) {
		ffscMsg("Unable to write object into NVRAM: RC %d", Result);
		return ERROR;
	}

	/* If the object isn't already marked "valid", validate it */
	if (!nvramIsValid(ID)) {
		nvramValidate(ID);
		if (nvramWriteHeader() != OK) {
			return ERROR;
		}
	}

	return OK;
}


/*
 * nvramZap
 *	Clear out the contents of NVRAM (DANGER WILL ROBINSON!)
 */
STATUS
nvramZap(void)
{
	bzero((char *) &nvramHdr, sizeof(nvramHdr));
	return nvramWriteHeader();
}



/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * nvramIsValid
 *	Returns TRUE or FALSE, indicating whether the specified NVRAM
 *	object is currently known to be valid
 */
BOOL
nvramIsValid(nvramid_t ID)
{
	int Word, Bit;

	Word = ID / 32;
	Bit  = ID % 32;

	return (nvramHdr.Valid[Word] & (1 << Bit)) ? TRUE : FALSE;
}


/*
 * nvramUpgrade1to2
 *	Upgrades version 1 NVRAM to version 2. This is simple, it involves
 *	simply adding a new delim character to NVRAM_CONSTERM & NVRAM_CONSALT.
 */
STATUS
nvramUpgrade1to2(void)
{
	consoleinfo_t ConsInfo;

	if (nvramRead(NVRAM_CONSTERM, &ConsInfo) == OK  &&
	    (ConsInfo.PFlags & CIF_VALID))
	{
		ConsInfo.Delim[COND_OBP] = OBP_CHAR;

		if (nvramWrite(NVRAM_CONSTERM, &ConsInfo) != OK) {
			/* nvramWrite should have logged the error */
			return ERROR;
		}
	}

	if (nvramRead(NVRAM_CONSALT, &ConsInfo) == OK  &&
	    (ConsInfo.PFlags & CIF_VALID))
	{
		ConsInfo.Delim[COND_OBP] = OBP_CHAR;

		if (nvramWrite(NVRAM_CONSALT, &ConsInfo) != OK) {
			/* nvramWrite should have logged the error */
			return ERROR;
		}
	}

	nvramHdr.Version = 2;
	return nvramWriteHeader();
}

/*
 * nvramUpgrade2to3
 *	Upgrades version 2 NVRAM to version 3. Tuneable variables are changed
 * for the new default timeout values.
 */
STATUS
nvramUpgrade2to3(void)
{

	int nvramDefault[TUNE_MAX];
	int nvramTuneInit[TUNE_MAX];
	extern tuneinfo_t ffscDefaultTuneInfo[];
	ttyinfo_t ttyInfo[FFSC_NUM_TTYS];
	extern ttyinfo_t ttyDefaultSettings[];
	int ix;

	/* load an array with the default parameter inforamtion in an order
	 * in which they can be delt with
	*/

   for (ix = 0;  ffscDefaultTuneInfo[ix].Name != NULL;  ++ix) {
      int Index;

      Index = ffscDefaultTuneInfo[ix].Index;
      nvramDefault[Index] = ffscDefaultTuneInfo[ix].Default;
   }

	/* now read the TUNE** variables from NVRAM */
   if (nvramRead(NVRAM_TUNE, nvramTuneInit) != OK) 
		return ERROR;

	else {

		/* following 6 entries updated due to communication problems
			between the MSC and the MMSC */

		ix = ffscTuneVarToIndex("CI_ROUTER_RESP");
		nvramTuneInit[ix] = nvramDefault[ix];
		ix = ffscTuneVarToIndex("RI_ELSC_RESP");
		nvramTuneInit[ix] = nvramDefault[ix];
		ix = ffscTuneVarToIndex("RW_RESP_LOCAL");
		nvramTuneInit[ix] = nvramDefault[ix];
		ix = ffscTuneVarToIndex("RW_RESP_REMOTE");
		nvramTuneInit[ix] = nvramDefault[ix];
		ix = ffscTuneVarToIndex("CONNECT_TIMEOUT");
		nvramTuneInit[ix] = nvramDefault[ix];
		ix = ffscTuneVarToIndex("DEBOUNCE_DELAY");
		nvramTuneInit[ix] = nvramDefault[ix];
		if (nvramWrite(NVRAM_TUNE, nvramTuneInit) != OK)
			return ERROR;
	}

	/* now update NVRAM for default OOB messages for com port 4 
	 * array index location 3 */
   if (nvramRead(NVRAM_TTYINFO, ttyInfo) != OK) 
		return ERROR;

	else {

		ttyInfo[3].CtrlFlags = ttyDefaultSettings[3].CtrlFlags;
		if (nvramWrite(NVRAM_TTYINFO, ttyInfo)  != OK)
			return ERROR;

	}

	nvramHdr.Version = 3;
	return nvramWriteHeader();

}


/*
 * nvramUpgrade3to4
 *	Upgrades version 2 NVRAM to version 3. Tuneable variables are changed
 * for the new default timeout values.
 */
STATUS
nvramUpgrade3to4(void)
{

	int nvramDefault[TUNE_MAX];
	int nvramTuneInit[TUNE_MAX];
	extern tuneinfo_t ffscDefaultTuneInfo[];
	int ix;

	/* load an array with the default parameter inforamtion in an order
	 * in which they can be delt with
	*/

   for (ix = 0;  ffscDefaultTuneInfo[ix].Name != NULL;  ++ix) {
      int Index;

      Index = ffscDefaultTuneInfo[ix].Index;
      nvramDefault[Index] = ffscDefaultTuneInfo[ix].Default;
   }

	/* now read the TUNE** variables from NVRAM */
   if (nvramRead(NVRAM_TUNE, nvramTuneInit) != OK) 
		return ERROR;

	else {

		/* following 6 entries updated due to communication problems
			between the MSC and the MMSC */

		ix = ffscTuneVarToIndex("LISTEN_MAX_CONN");
		nvramTuneInit[ix] = nvramDefault[ix];
		if (nvramWrite(NVRAM_TUNE, nvramTuneInit) != OK)
			return ERROR;
	}

	nvramHdr.Version = 4;
	return nvramWriteHeader();
}

/*
 * nvramUpgrade4to5
 *	Upgrades version 4 NVRAM to version 5. Tuneable variables are changed
 * for the new default timeout values.
 */
STATUS
nvramUpgrade4to5(void)
{

	int nvramDefault[TUNE_MAX];
	int nvramTuneInit[TUNE_MAX];
	extern tuneinfo_t ffscDefaultTuneInfo[];
	ttyinfo_t ttyInfo[FFSC_NUM_TTYS];
	extern ttyinfo_t ttyDefaultSettings[];
	int ix;

	/* load an array with the default parameter inforamtion in an order
	 * in which they can be delt with
	*/

   for (ix = 0;  ffscDefaultTuneInfo[ix].Name != NULL;  ++ix) {
      int Index;

      Index = ffscDefaultTuneInfo[ix].Index;
      nvramDefault[Index] = ffscDefaultTuneInfo[ix].Default;
   }

	/* now read the TUNE** variables from NVRAM */
   if (nvramRead(NVRAM_TUNE, nvramTuneInit) != OK) 
		return ERROR;

	else {

		/* following BUF_WRTIE Timeout has been increased to 10 sec. */
		ix = ffscTuneVarToIndex("BUF_WRITE");
		nvramTuneInit[ix] = nvramDefault[ix];

		if (nvramWrite(NVRAM_TUNE, nvramTuneInit) != OK)
			return ERROR;
	}

	/* now update NVRAM for default on com ports 2, and 3. 
	 * The change enables hardware flow control */

   if (nvramRead(NVRAM_TTYINFO, ttyInfo) != OK) 
		return ERROR;

	else {

		for (ix = 1; ix < 3; ix++)
			ttyInfo[ix].CtrlFlags = ttyDefaultSettings[ix].CtrlFlags;

		if (nvramWrite(NVRAM_TTYINFO, ttyInfo)  != OK)
			return ERROR;

	}

	nvramHdr.Version = 5;
	return nvramWriteHeader();

}



int nvramUpgrade5to6(void) {

  int nvramDefault[TUNE_MAX];
  int nvramTuneInit[TUNE_MAX];
  extern tuneinfo_t ffscDefaultTuneInfo[];
  ttyinfo_t ttyInfo[FFSC_NUM_TTYS];
  int ix;

  /* load an array with the default parameter inforamtion in an order
   * in which they can be delt with
   */

  for (ix = 0;  ffscDefaultTuneInfo[ix].Name != NULL;  ++ix) {
    int Index;

    Index = ffscDefaultTuneInfo[ix].Index;
    nvramDefault[Index] = ffscDefaultTuneInfo[ix].Default;
  }

	/* now read the TUNE** variables from NVRAM */
  if (nvramRead(NVRAM_TUNE, nvramTuneInit) != OK) {
    return ERROR;
  } else {

    /* following BUF_WRTIE Timeout has been increased to 10 sec. */
    ix = ffscTuneVarToIndex("CONNECT_TIMEOUT");
    nvramTuneInit[ix] = nvramDefault[ix];

    if (nvramWrite(NVRAM_TUNE, nvramTuneInit) != OK)
      return ERROR;
  }

  nvramHdr.Version = 6;
  return nvramWriteHeader();
}




/*
 * nvramValidate
 *	Validates the specified NVRAM object in the in-memory copy
 *	of the NVRAM header.
 */
void
nvramValidate(nvramid_t ID)
{
	int Word, Bit;

	Word = ID / 32;
	Bit  = ID % 32;

	nvramHdr.Valid[Word] |= (1 << Bit);
}


/*
 * nvramWriteHeader
 *	Writes the in-memory NVRAM header to NVRAM
 */
STATUS
nvramWriteHeader(void)
{
	int Result;

	Result = sram_write((char *) &nvramHdr, sizeof(nvramhdr_t), 0);
	if (Result != VALID_WRITE) {
		ffscMsg("Unable to initialize NVRAM header: RC %d", Result);
		return ERROR;
	}

	return OK;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			    DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

#ifndef PRODUCTION

/*
 * nvramInvalidate
 *	Invalidates the specified NVRAM object in the NVRAM header.
 *	Useful while debugging for deleting an object prior to reformatting.
 */
STATUS
nvramInvalidate(nvramid_t ID)
{
	int Word, Bit;

	Word = ID / 32;
	Bit  = ID % 32;

	nvramHdr.Valid[Word] &= ~(1 << Bit);
	if (nvramWriteHeader() != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * nvramSetVersion
 *	Sets the version ID in the NVRAM header
 */
STATUS
nvramSetVersion(int Version)
{
	nvramHdr.Version = Version;
	if (nvramWriteHeader() != OK) {
		return ERROR;
	}

	return OK;
}

#endif  /* !PRODUCTION */
