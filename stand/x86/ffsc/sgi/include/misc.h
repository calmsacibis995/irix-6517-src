/*
 * Miscellaneous FFSC functions and constants
 */
#ifndef _MISC_H_
#define _MISC_H_

#include <time.h>

struct in_addr;


/* Tokens - strings that can be converted to integers by ffscTokenToInt */
typedef struct tokeninfo {
	char	*Name;
	int	Value;
} tokeninfo_t;

STATUS ffscTokenToInt(tokeninfo_t *, char *, int *);
void   ffscIntToToken(tokeninfo_t *, int, char *, int);


/* Popular tokens */
extern tokeninfo_t ffscYesNoTokens[];


/* Paged output buffer */
typedef struct pagebuf {
	char	*Buf;		/* Data buffer */
	int	MaxLen;		/* Maximum size of buffer */
	int	DataLen;	/* Size of data in buffer */
	int	CurrLine;	/* Current line position in buffer */
	char	*CurrPtr;	/* Current char position in buffer */
	int	NumPrint;	/* # of lines to print */
} pagebuf_t;

pagebuf_t *ffscCreatePageBuf(int);
void	  ffscFreePageBuf(pagebuf_t *);
void      ffscPrintPageBuf(pagebuf_t *, const char *, ...);


/* Miscellaneous global variables */
extern const char ffscVersion[];


/* Function prototypes */
int    ffscCompareTimestamps(timestamp_t *, timestamp_t *);
int    ffscCompareUniqueIDs(uniqueid_t, uniqueid_t);
void   ffscConvertToLowerCase(char *);
void   ffscConvertToUpperCase(char *);
void   ffscCtrlCharToString(char, char *);
char   *ffscModuleNumToString(modulenum_t, char *);
STATUS ffscParseBayName(char, bayid_t *);
STATUS ffscParseIPAddr(char *, struct in_addr *);
STATUS ffscParseModuleNum(char *, modulenum_t *);
STATUS ffscParseRackID(char *, rackid_t *);
STATUS ffscParseUniqueID(char *, uniqueid_t *);
char   *ffscRackIDToString(rackid_t, char *);
STATUS ffscTestAddress(void *);
char   *ffscTimestampToString(timestamp_t *, char *);
int    ffscTuneVarToIndex(char *);
char   *ffscUniqueIDToString(uniqueid_t, char *);

#endif
