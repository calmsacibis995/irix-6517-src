/*
 * misc.c
 *	Miscellaneous functions for the FFSC
 */

#include <vxworks.h>

#include <ctype.h>
#include <inetlib.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ffsc.h"
#include "misc.h"


/* Popular tokens */
tokeninfo_t ffscYesNoTokens[] = {
	{ "ENABLE",	1 },
	{ "ON",		1 },
	{ "YES",	1 },
	{ "Y",		1 },
	{ "DISABLE",	0 },
	{ "OFF",	0 },
	{ "NO",		0 },
	{ "N",		0 },
	{ NULL,	NULL }
};


/* Internal functions */
static void ErrorSignal();


/* Internal variables */
static jmp_buf TestReturn;


/*
 * ffscCompareTimestamps
 *	Compares two timestamp_t's, returning <0, 0, >0 for L<R, L=R, L>R
 */
int
ffscCompareTimestamps(timestamp_t *Left, timestamp_t *Right)
{
	if (Left->tv_sec < Right->tv_sec) {
		return -1;
	}
	else if (Left->tv_sec > Right->tv_sec) {
		return 1;
	}

	/* tv_sec's must be equal, we're down to nanoseconds */
	if (Left->tv_nsec < Right->tv_nsec) {
		return -1;
	}
	else if (Left->tv_nsec > Right->tv_nsec) {
		return 1;
	}

	/* They must be equal */
	return 0;
}


/*
 * ffscCompareUniqueIDs
 *	Compares two unique IDs and returns <0, 0, >0 if L<R, L=R, L>R
 */
int
ffscCompareUniqueIDs(uniqueid_t Left, uniqueid_t Right)
{
	return bcmp((char *) Left, (char *) Right, sizeof(uniqueid_t));
}


/*
 * ffscConvertToLowerCase
 *	Utility function that converts all upper case alphabetic
 *	characters in the specified string to lower case.
 */
void
ffscConvertToLowerCase(char *String)
{
	int i;
	int Len = strlen(String);

	for (i = 0;  i < Len;  ++i) {
		String[i] = tolower(String[i]);
	}
}


/*
 * ffscConvertToUpperCase
 *	Utility function that converts all lower case alphabetic
 *	characters in the specified string to upper case.
 */
void
ffscConvertToUpperCase(char *String)
{
	int i;
	int Len = strlen(String);

	for (i = 0;  i < Len;  ++i) {
		String[i] = toupper(String[i]);
	}
}


/*
 * ffscCtrlCharToString
 *	Converts the specified character into a human-readable string
 *	and stores the results in the specified buffer.
 */
void
ffscCtrlCharToString(char CtrlChar, char *String)
{
	if (CtrlChar > 0  &&  CtrlChar < 32) {
		sprintf(String, "^%c", (CtrlChar + 64));
	}
	else if (CtrlChar == ' ') {
		sprintf(String, "<space>");
	}
	else if (CtrlChar > 33  &&  CtrlChar < 127) {
		sprintf(String, "%c", CtrlChar);
	}
	else {
		sprintf(String, "0%o (0x%x)", CtrlChar, CtrlChar);
	}
}


/*
 * ffscIntToToken
 *	Given a integer and a list of translations, converts the integer
 *	into a token string, storing it in the specified buffer for a
 *	length not to exceed the specified value (including terminating NUL).
 *	If a matching token is not found, "<unknown>" is stored.
 */
void
ffscIntToToken(tokeninfo_t *Info, int Value, char *Buffer, int MaxLen)
{
	char *Token;
	int i;

	if (Info == NULL) {
		Token = "<NO TRANSLATION>";
	}
	else {
		Token = "<unknown>";
		for (i = 0;  Info[i].Name != NULL;  ++i) {
			if (Value == Info[i].Value) {
				Token = Info[i].Name;
				break;
			}
		}
	}

	strncpy(Buffer, Token, MaxLen - 1);
	Buffer[MaxLen-1] = '\0';

	return;
}


/*
 * ffscModuleNumToString
 *	Converts a module number into a string and stores the result in
 *	the specified buffer, or a static buffer if a buffer was not
 *	specified. Returns a pointer to the buffer.
 */
char *
ffscModuleNumToString(modulenum_t Num, char *OutBuf)
{
	static char StaticBuf[MODULENUM_STRLEN];

	if (OutBuf == NULL) {
		OutBuf = StaticBuf;
	}

	if (Num == MODULENUM_UNASSIGNED) {
		strcpy(OutBuf, "UNASSIGNED");
	}
	else {
		sprintf(OutBuf, "%lx", Num);
	}

	return OutBuf;
}


/*
 * ffscParseBayName
 *	"Parse" a single char for a valid bay name and store the
 *	corresponding ID in the specified bayid_t. Returns OK/ERROR.
 */
STATUS
ffscParseBayName(char Name, bayid_t *ID)
{
	int i;

	Name = toupper(Name);
	for (i = 0;  i < MAX_BAYS;  ++i) {
		if (Name == ffscBayName[i]) {
			if (ID != NULL) {
				*ID = (bayid_t) i;
			}
			return OK;
		}
	}

	return ERROR;
}


/*
 * ffscParseIPAddr
 *	Parse a string for an IP address and store the result in the
 *	specified struct in_addr. The VxWorks inet_addr may crash the
 *	system if a bogus IP address is specified, so this does some
 *	additional sanity checking. Returns OK/ERROR.
 */
STATUS
ffscParseIPAddr(char *String, struct in_addr *Addr)
{
	char C;
	unsigned int O1, O2, O3, O4;

	/* Make sure there are 4 numeric fields separated by periods */
	/* and that no field exceeds 255.			     */
	if (sscanf(String, "%u.%u.%u.%u%c", &O1, &O2, &O3, &O4, &C) != 4  ||
	    O1 > 255  ||  O2 > 255  ||  O3 > 255  ||  O4 > 255)
	{
		ffscMsg("Invalid IP address \"%s\"", String);
		return ERROR;
	}

	/* While it would probably be just as easy to cook O1-O4 into	*/
	/* an IP address manually, inet_addr will at least get the byte	*/
	/* ordering correct with minimum fuss.				*/
	Addr->s_addr = inet_addr(String);

	return OK;
}


/*
 * ffscParseModuleNum
 *	Parse a string for a module number and store the result in the
 *	specified modulenum_t. Returns OK/ERROR.
 */
STATUS
ffscParseModuleNum(char *String, modulenum_t *Num)
{
	char C;
	unsigned int Value;

	if (sscanf(String, "%x%c", &Value, &C) != 1  ||
	    Value > MODULENUM_MAX)
	{
		ffscMsg("Invalid module number \"%s\"", String);
		return ERROR;
	}

	if (Num != NULL) {
		*Num = Value;
	}

	return OK;
}


/*
 * ffscParseRackID
 *	Parse a string for a rack ID and store the result in the
 *	specified rackid_t. Returns OK/ERROR.
 */
STATUS
ffscParseRackID(char *String, rackid_t *ID)
{
	char C;
	unsigned int Value;

	if (sscanf(String, "%u%c", &Value, &C) != 1  ||
	    Value > RACKID_MAX || Value < 1)
	{
		ffscMsg("Invalid rack ID \"%s\"", String);
		return ERROR;
	}

	if (ID != NULL) {
		*ID = Value;
	}

	return OK;
}


/*
 * ffscParseUniqueID
 *	Parse a string for a unique ID and store the result in the
 *	specified rackid_t. Returns OK/ERROR.
 */
STATUS
ffscParseUniqueID(char *String, uniqueid_t *ID)
{
	char C;
	unsigned int i1,i2,i3,i4,i5,i6,i7,i8;

	if (sscanf(String,
		   "%x:%x:%x:%x:%x:%x:%x:%x%c",
		   &i1, &i2, &i3, &i4, &i5, &i6, &i7, &i8, &C) != 8  ||
	    i1 > 255  ||  i2 > 255  ||  i3 > 255  ||  i4 > 255   ||
	    i5 > 255  ||  i6 > 255  ||  i7 > 255  ||  i8 > 255)
	{
		ffscMsg("Invalid unique ID \"%s\"", String);
		return ERROR;
	}

	if (ID != NULL) {
		(*ID)[0] = (unsigned char) i1;
		(*ID)[1] = (unsigned char) i2;
		(*ID)[2] = (unsigned char) i3;
		(*ID)[3] = (unsigned char) i4;
		(*ID)[4] = (unsigned char) i5;
		(*ID)[5] = (unsigned char) i6;
		(*ID)[6] = (unsigned char) i7;
		(*ID)[7] = (unsigned char) i8;
	}

	return OK;
}


/*
 * ffscRackIDToString
 *	Converts a rack ID into a string and stores the result in
 *	the specified buffer, or a static buffer if a buffer was not
 *	specified. Returns a pointer to the buffer.
 */
char *
ffscRackIDToString(rackid_t ID, char *OutBuf)
{
	static char StaticBuf[RACKID_STRLEN];

	if (OutBuf == NULL) {
		OutBuf = StaticBuf;
	}

	if (ID == RACKID_UNASSIGNED) {
		strcpy(OutBuf, "UNASSIGNED");
	}
	else {
		sprintf(OutBuf, "%lu", ID);
	}

	return OutBuf;
}


/*
 * ffscTestAddress
 *	Make sure the specified value is a legitimate address for
 *	read access. Returns OK if so, ERROR if not.
 *
 *	NOTE: The setjmp buffer used by this function is global, as
 *	      opposed to being a task variable. Consequently, only
 *	      the router task should use this function for now.
 */
STATUS
ffscTestAddress(void *Ptr)
{
	char Dummy;
	void (*OldIllHandler)();
	void (*OldBusHandler)();

	/* Set up a signal handler to trap SIGILL and SIGBUS */
	OldIllHandler = signal(SIGILL, ErrorSignal);
	if (OldIllHandler == SIG_ERR) {
		ffscMsgS("Unable to set SIGILL handler");
		return ERROR;
	}

	OldBusHandler = signal(SIGBUS, ErrorSignal);
	if (OldBusHandler == SIG_ERR) {
		ffscMsgS("Unable to set SIGBUS handler");
		return ERROR;
	}

	/* Arrange to be revived if the test fails */
	if (setjmp(TestReturn) != 0) {

		/* Apparently the test failed and the signal handler */
		/* bounced us back up here. Restore the original     */
		/* signal handlers, then record our failure.	     */

		signal(SIGILL, OldIllHandler);
		signal(SIGBUS, OldBusHandler);

		return ERROR;
	}

	/* Simply touch the address in question. If something goes */
	/* wrong, we will return to the setjmp above.		   */
	Dummy = *((char *) Ptr);

	/* If we made it to here, the test apparently succeeded. */
	signal(SIGILL, OldIllHandler);
	signal(SIGBUS, OldBusHandler);

	return OK;
}


/*
 * ffscTimestampToString
 *	Converts a timestamp into a human-readable string and stores the
 *	result in the specified buffer, or a static buffer if a buffer was
 *	not specified. Returns a pointer to the buffer.
 */
char *
ffscTimestampToString(timestamp_t *TS, char *OutBuf)
{
	static char StaticBuf[TIMESTAMP_STRLEN];

	if (OutBuf == NULL) {
		OutBuf = StaticBuf;
	}

	if (TS == NULL) {
		strcpy(OutBuf, "<null>");
	}
	else {
		sprintf(OutBuf, "%ld.%09ld", TS->tv_sec, TS->tv_nsec);
	}

	return OutBuf;
}


/*
 * ffscTokenToInt
 *	Given a token and a list of translations, converts the token
 *	into an integer and returns OK, or ERROR if the token is invalid.
 */
STATUS
ffscTokenToInt(tokeninfo_t *Info, char *Token, int *Value)
{
	int i;

	if (Info == NULL) {
		return ERROR;
	}

	ffscConvertToUpperCase(Token);
	for (i = 0;  Info[i].Name != NULL;  ++i) {
		if (strcmp(Token, Info[i].Name) == 0) {
			if (Value != NULL) {
				*Value = Info[i].Value;
			}
			return OK;
		}
	}

	return ERROR;
}


/*
 * ffscTuneVarToIndex
 *	Given a token, finds the corresponding tuneinfo entry and returns
 *	its index, or <0 if unsuccessful.
 */
int
ffscTuneVarToIndex(char *Token)
{
	int i;
	extern tuneinfo_t ffscDefaultTuneInfo[];


	ffscConvertToUpperCase(Token);
	for (i = 0;  i < NUM_TUNE;  ++i) {
		if (strcmp(Token, ffscDefaultTuneInfo[i].Name) == 0) {
			return ffscDefaultTuneInfo[i].Index;
		}
	}

	return -1;
}


/*
 * ffscUniqueIDToString
 *	Converts a unique ID into a string and stores the result in
 *	the specified buffer, or a static buffer if a buffer was not
 *	specified. Returns a pointer to the buffer.
 */
char *
ffscUniqueIDToString(uniqueid_t ID, char *OutBuf)
{
	static char StaticBuf[UNIQUEID_STRLEN];

	if (OutBuf == NULL) {
		OutBuf = StaticBuf;
	}

	sprintf(OutBuf,
		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ID[0], ID[1], ID[2], ID[3], ID[4], ID[5], ID[6], ID[7]);

	return OutBuf;
}



/*----------------------------------------------------------------------*/
/*									*/
/*			    PAGEBUF FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * ffscCreatePageBuf
 *	Creates a pagebuf_t with the specified size and returns its pointer,
 *	or NULL if unsuccessful.
 */
pagebuf_t *
ffscCreatePageBuf(int Size)
{
	pagebuf_t *PageBuf;

	PageBuf = (pagebuf_t *) malloc(sizeof(pagebuf_t));
	if (PageBuf == NULL) {
		ffscMsgS("Unable to allocate storage for pagebuf_t");
		return NULL;
	}

	PageBuf->Buf = (char *) malloc(Size + 1);
	if (PageBuf->Buf == NULL) {
		ffscMsgS("Unable to allocate storage for page buffer");
		free(PageBuf);
		return NULL;
	}

	PageBuf->MaxLen   = Size;
	PageBuf->DataLen  = 0;
	PageBuf->CurrLine = 0;
	PageBuf->CurrPtr  = PageBuf->Buf;

	return PageBuf;
}


/*
 * ffscFreePageBuf
 *	Frees the resources used by a pagebuf_t
 */
void
ffscFreePageBuf(pagebuf_t *PageBuf)
{
	free(PageBuf->Buf);
	free(PageBuf);
}


/*
 * ffscPrintPageBuf
 *	Like sprintf, but appends to the specified pagebuf_t
 */
void
ffscPrintPageBuf(pagebuf_t *PageBuf, const char *Format, ...)
{
	char MsgBuf[1000];
	int  MsgLen;
	va_list Args;

	/* If there isn't even room left for a trailing CR/LF, bail out */
	if (PageBuf->DataLen + 2 > PageBuf->MaxLen) {
		ffscMsg("ffscPrintPageBuf aborted - buffer full");
		return;
	}

	/* Build the actual message to be appended */
	va_start(Args, Format);
	vsprintf(MsgBuf, Format, Args);
	va_end(Args);

	/* Determine its length */
	MsgLen = strlen(MsgBuf);
	if (MsgLen == 0) {
		return;
	}
	MsgLen += 2;	/* Account for the CR/LF we will append */

	/* If the message is too long, arrange for truncation */
	if (PageBuf->DataLen + MsgLen > PageBuf->MaxLen) {
		MsgLen = PageBuf->MaxLen - PageBuf->DataLen;
	}

	/* Append the message (+ trailing CR/LF/NUL) to the buffer */
	bcopy(MsgBuf, PageBuf->CurrPtr, MsgLen - 2);
	PageBuf->CurrPtr[MsgLen - 2] = '\r';
	PageBuf->CurrPtr[MsgLen - 1] = '\n';
	PageBuf->CurrPtr[MsgLen] = '\0';

	PageBuf->CurrPtr += MsgLen;
	PageBuf->DataLen += MsgLen;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * ErrorSignal
 *	signal handler for SIGILL and SIGBUS errors that may occur in
 *	ffscTestAddress.
 */
void
ErrorSignal(int Signal, int Code)
{
	ffscMsg("Trapped signal %d code %d for ffscTestAddress",
		Signal, Code);
	longjmp(TestReturn, 1);
}
