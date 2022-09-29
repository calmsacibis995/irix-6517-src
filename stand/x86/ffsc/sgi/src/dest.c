/*
 * Functions for processing FFSC destination strings
 */
#include <vxworks.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"
#include "addrd.h"

#include "cmd.h"
#include "dest.h"
#include "elsc.h"
#include "identity.h"
#include "misc.h"


/* Lexical definitions */
#define DEST_ABBREV_ALL_CHAR	'*'
#define DEST_ABBREV_ALL		"*"
#define DEST_ABBREV_LOCAL_CHAR  '.'
#define DEST_ABBREV_LOCAL	"."
#define DEST_ABBREV_BAYADDR	"ulUL"
#define DEST_CMD_END		"\n\r"
#define DEST_DIGITS		"0123456789"
#define DEST_FORCE_DELIM_CHAR	'\030'
#define DEST_FORCE_DELIM	"\030"
#define DEST_HEX_DIGITS		"abcdefABCDEF"
#define DEST_ITEM_SEP_CHAR	','
#define DEST_ITEM_SEP		","
#define DEST_RANGE_SEP_CHAR	'-'
#define DEST_RANGE_SEP		"-"
#define DEST_SSD_SEP_CHAR	'/'
#define DEST_SSD_SEP		"/"
#define DEST_WS			" \t"

#define DEST_TOKEN_DELIM	DEST_WS DEST_FORCE_DELIM DEST_CMD_END
#define DEST_VALID_LIST		DEST_DIGITS DEST_ITEM_SEP DEST_RANGE_SEP
#define DEST_VALID_BAYLIST	DEST_ABBREV_BAYADDR DEST_ITEM_SEP
#define DEST_VALID_MODLIST	DEST_VALID_LIST DEST_HEX_DIGITS
#define DEST_VALID_SSD1		DEST_VALID_LIST DEST_SSD_SEP		\
				DEST_ABBREV_ALL DEST_ABBREV_LOCAL
#define DEST_VALID_SSD2		DEST_VALID_LIST DEST_ABBREV_BAYADDR	\
				DEST_ABBREV_ALL


/* Token handling */
#define MAX_TOKEN_LEN	63

#define TOKEN_UNKNOWN		-1
#define TOKEN_EMPTY		0
#define TOKEN_FORCE_DELIM	1
#define TOKEN_RACK		2
#define TOKEN_BAY		3
#define TOKEN_MODULE		4
#define TOKEN_ALL		5
#define TOKEN_LOCAL		6
#define TOKEN_LIST		7
#define TOKEN_BAYLIST		8
#define TOKEN_MODLIST		9 
#define TOKEN_SSD		10	/* Short Syntax Destination */
#define TOKEN_PARTITION         11      /* Set of modules (partition) */

static struct {
	const char *name;
	int value;
} TokenTable[] = {
	{ "",			TOKEN_EMPTY },
	{ DEST_FORCE_DELIM,	TOKEN_FORCE_DELIM },
	{ "RACK",		TOKEN_RACK },
	{ "R",			TOKEN_RACK },
	{ "BAY",		TOKEN_BAY },
	{ "B",			TOKEN_BAY },
	{ "ALL",		TOKEN_ALL },
	{ DEST_ABBREV_ALL,	TOKEN_ALL },
	{ "LOCAL",		TOKEN_LOCAL },
	{ DEST_ABBREV_LOCAL,	TOKEN_LOCAL },
	{ "MODULE",		TOKEN_MODULE },
	{ "M",			TOKEN_MODULE },
	{ "PARTITION",          TOKEN_PARTITION},
	{ "PART",               TOKEN_PARTITION},
	{ "P",                  TOKEN_PARTITION},
	{ NULL },
};


/* Internal functions */
static STATUS AppendDestString(dest_t *, const char *);
static const char *ConsumeWhiteSpace(const char *);
static const char *ExtractNextToken(const char *, char[], int *);
static STATUS MapAllRacks(rackmap_t *);
static STATUS MapRacks(int, int, rackmap_t *);
static STATUS ParseBayList(char *, baymap_t *);
static STATUS ParseModuleList(char *, dest_t *);
static STATUS ParsePartList(char *, dest_t *);
static STATUS ParseRackList(char *, rackmap_t *);
static STATUS ParseShortSyntaxDest(char *, dest_t *);
static void   PrintTokenValue(int);
static STATUS SelectAllModules(dest_t *);
static STATUS SelectModule(dest_t *, modulenum_t);
static STATUS SelectModules(dest_t *, int, int);
static STATUS SelectPartitions(dest_t *Dest, int Start, int End);
static void   SelectRacksBays(dest_t *, rackmap_t, baymap_t);
static int    TranslateToken(const char *);



/*----------------------------------------------------------------------*/
/*									*/
/*			dest_t MANIPULATION FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * destClear
 *	Reset the contents of a dest_t to an all-unselected state.
 */
void
destClear(dest_t *Dest)
{
	bzero((char *) Dest, sizeof(*Dest));
	Dest->Flags |= DESTF_NONE;
}


/*
 * destNumBays
 *	Returns the number of bays selected in the specified dest_t.
 *	If a pointer to a rackid and/or bayid_t is specified, the
 *	corresponding portions of one of the selected bays will be
 *	stored.
 */
int
destNumBays(const dest_t *Dest, rackid_t *RackPtr, bayid_t *BayPtr)
{
	int i, j;
	int NumBays;

	NumBays = 0;
	for (i = 0;  i < MAX_RACKS;  ++i) {
		for (j = 0;  j < MAX_BAYS;  ++j) {
			if (destBayIsSelected(Dest, i, j)) {
				++NumBays;

				if (RackPtr != NULL) {
					*RackPtr = (rackid_t) i;
				}

				if (BayPtr != NULL) {
					*BayPtr = (bayid_t) j;
				}
			}
		}
	}

	return NumBays;
}


/*
 * destNumRacks
 *	Returns the number of racks selected in the specified dest_t.
 *	If a pointer to a rackid_t is specified, one of the selected
 *	rackid's will be stored in it.
 */
int
destNumRacks(const dest_t *Dest, rackid_t *RackPtr)
{
	int i;
	int NumRacks;

	NumRacks = 0;
	for (i = 0;  i < MAX_RACKS;  ++i) {
		if (destRackIsSelected(Dest, i)) {
			++NumRacks;
			if (RackPtr != NULL) {
				*RackPtr = (rackid_t) i;
			}
		}
	}

	return NumRacks;
}



/*----------------------------------------------------------------------*/
/*									*/
/*			   OTHER PUBLIC FUNCTIONS			*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * destParse
 *	Parses an FFSC destination from the given character string and
 *	stores it in the specified dest_t. Returns a pointer to the first
 *	character following the destination specification, or NULL if
 *	something went wrong (e.g. invalid syntax).
 */
const char *
destParse(const char *String, dest_t *Dest, int AllowSSD)
{
	baymap_t BayMap;
	char CurrDest[DEST_STRLEN];
	char Token[MAX_TOKEN_LEN + 1];
	const char *CurrPtr;
	const char *SavePtr;
	int  ConsumedToken;
	int  TokenValue;
	rackmap_t RackMap;

	/* Watch for the trivial error cases */
	if (String == NULL) {
		ffscMsg("Tried to parse non-existent string for destination");
		return NULL;
	}
	if (Dest == NULL) {
		ffscMsg("destParse called with NULL dest_t");
		return NULL;
	}

	/* Start off with an empty destination */
	destClear(Dest);

	/* Examine each token in the line until we reach the end of line */
	/* (or the loop terminates early due to an unrecognized token)	 */
	SavePtr = String;
	CurrPtr = ExtractNextToken(String, Token, &TokenValue);
	while (TokenValue != TOKEN_EMPTY) {

		/* Don't assume we will use the current token yet */
		ConsumedToken = 0;

		/* If the token appears to be a short-syntax destination */
		/* handle it elsewhere.					 */
		if (AllowSSD  &&
		    (TokenValue == TOKEN_SSD		||
		     TokenValue == TOKEN_BAYLIST	||
		     TokenValue == TOKEN_LIST		||
		     TokenValue == TOKEN_ALL		||
		     TokenValue == TOKEN_LOCAL))
		{
			/* Try to parse out a short-syntax destination */
			if (ParseShortSyntaxDest(Token, Dest) < 0) {
				return SavePtr;
			}

			/* We were successful - extract the next token */
			ConsumedToken = 1;
			SavePtr = CurrPtr;
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);

			/* Start over */
			continue;
		}
		/* Check for a PARTITION specification */
		if(TokenValue == TOKEN_PARTITION){

		  /* Get another token */
		  CurrPtr = ExtractNextToken(CurrPtr,Token,&TokenValue);
		  
		  /* Proceed according to its value */
		  switch (TokenValue) {
		    
		    /* Selected all partitions (hence, all modules) */
		  case TOKEN_ALL:
		    if (SelectAllModules(Dest) != OK) {
		      return NULL;
		    }
		    if (AppendDestString(Dest, "P *") != OK) {
		      return NULL;	/* Too long */
		    }
		    break;

		    /* Here the destination is a list of partitions. Map
		     * each module list into partition list.
		     */
		  case TOKEN_LIST:
		  case TOKEN_MODLIST:
		    if (ParsePartList(Token, Dest) < 0) {
		      return NULL;	/* Invalid list */
		    }
		    
		    if (AppendDestString(Dest, "P") != OK  ||
			AppendDestString(Dest, Token) != OK)
		      {
			return NULL;	/* Too long */
		      }
		    break;
		    
		  default:
		    /* Syntax error */
		    return NULL;
		  }
		  
		  /* We've successfully parsed the module(s) */
		  ConsumedToken = 1;
		  SavePtr = CurrPtr;
		  CurrPtr = ExtractNextToken(CurrPtr,
					     Token,
					     &TokenValue);
		  
		  /* Start over */
		  continue;
		}

		/* Check for a MODULE specification */
		if (TokenValue == TOKEN_MODULE) {

			/* Get another token */
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);

			/* Proceed according to its value */
			switch (TokenValue) {

			    case TOKEN_ALL:
				if (SelectAllModules(Dest) != OK) {
					return NULL;
				}

				if (AppendDestString(Dest, "M *") != OK) {
					return NULL;	/* Too long */
				}
				break;

			    case TOKEN_LIST:
			    case TOKEN_MODLIST:
				if (ParseModuleList(Token, Dest) < 0) {
					return NULL;	/* Invalid list */
				}

				if (AppendDestString(Dest, "M") != OK  ||
				    AppendDestString(Dest, Token) != OK)
				{
					return NULL;	/* Too long */
				}
				break;

			    default:
				/* Syntax error */
				return NULL;
			}

			/* We've successfully parsed the module(s) */
			ConsumedToken = 1;
			SavePtr = CurrPtr;
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);

			/* Start over */
			continue;
		}

		/* Apparently we have a long-syntax rack/bay destination */
		BayMap  = 0;
		RackMap = 0;
		CurrDest[0] = '\0';

		/* Look for a RACK specification */
		if (TokenValue == TOKEN_RACK) {

			/* Get another token */
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);

			/* Proceed according to its value */
			switch (TokenValue) {

			    case TOKEN_ALL:
				if (MapAllRacks(&RackMap) != OK) {
					return NULL;	/* Trouble */
				}

				strcat(CurrDest, "R *");
				break;

			    case TOKEN_LOCAL:
				RackMap = (1 << ffscLocal.rackid);
				strcat(CurrDest, "R .");
				break;

			    case TOKEN_LIST:
				if (ParseRackList(Token, &RackMap) != OK) {
					return NULL;	/* Invalid list */
				}

				strcat(CurrDest, "R ");
				strcat(CurrDest, Token);
				break;

			    default:
				/* Syntax error */
				return NULL;
			}

			/* Get the next token */
			ConsumedToken = 1;
			SavePtr = CurrPtr;
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);
		}
		else {
			/* No RACK specified, so use local */
			RackMap = 1 << ffscLocal.rackid;
			strcat(CurrDest, "R .");
		}

		/* Now check for a BAY specification */
		if (TokenValue == TOKEN_BAY) {

			/* Get another token */
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);

			/* Proceed according to its value */
			switch (TokenValue) {

			    case TOKEN_ALL:
				BayMap = BAYMAP_ALL | BAYID_BITMAP_ALL;
				strcat(CurrDest, " B *");
				break;

			    case TOKEN_LIST:
			    case TOKEN_BAYLIST:
				if (ParseBayList(Token, &BayMap) != OK) {
					return NULL;	/* Invalid list */
				}

				strcat(CurrDest, " B ");
				strcat(CurrDest, Token);
				break;

			    default:
				/* Syntax error */
				return NULL;
			}

			/* Get the next token */
			ConsumedToken = 1;
			SavePtr = CurrPtr;
			CurrPtr = ExtractNextToken(CurrPtr,
						   Token,
						   &TokenValue);
		}
		else {
			/* No BAY specified, so default to "ALL" */
			BayMap = BAYMAP_ALL | BAYID_BITMAP_ALL;
			strcat(CurrDest, " B *");
		}

		/* If the current token was not part of a  */
		/* destination specification, we are done. */
		if (!ConsumedToken) {
			break;
		}

		/* Add the selected racks*bays to the dest map */
		SelectRacksBays(Dest, RackMap, BayMap);
		if (AppendDestString(Dest, CurrDest) != OK) {
			return NULL;
		}
	}

	return SavePtr;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * AppendDestString
 *	Append the specified string, followed by a space, to the String
 *	field of the specified dest_t. If this would cause the string to
 *	grow too large, return ERROR, otherwise return OK.
 */
STATUS
AppendDestString(dest_t *Dest, const char *String)
{
	if (strlen(Dest->String) + strlen(String) + 1 >= DEST_STRLEN) {
		ffscMsg("Destination string too long");
		return ERROR;
	}

	strcat(Dest->String, String);
	strcat(Dest->String, " ");

	return OK;
}


/*
 * ConsumeWhiteSpace
 *	Given a string pointer, returns a pointer to the first non-whitespace
 *	character in the string. Note that this may be a null character if
 *	there are no non-whitespace characters in the string.
 */
const char *
ConsumeWhiteSpace(const char *String)
{
	if (String == NULL) {
		return NULL;
	}

	String += strspn(String, DEST_WS);

	return String;
}


/*
 * ExtractNextToken
 *	Given a string, skips over any leading whitespace then copies
 *	the next token in the string into the specified char[], up to
 *	MAX_TOKEN_LEN characters, followed by a null (notice that this
 *	implies that the char[] should be at least MAX_TOKEN_LEN+1 chars
 *	in size). If more than MAX_TOKEN_LEN characters are present in
 *	the token, the remaining characters are discarded and a warning
 *	is printed. A pointer to the first character that follows the
 *	token is returned, or NULL if something goes wrong.
 *
 *	Special cases:
 *	- The FORCE_DELIM char is a token by itself
 */
const char *
ExtractNextToken(const char *String,  char Token[], int *ValuePtr)
{
	const char *CurrPtr;
	int CopyLen;
	int TokenLen;

	/* Handle the trivial cases */
	if (String == NULL) {
		Token[0] = '\0';
		CurrPtr = NULL;
		goto Done;
	}

	/* Skip over any leading whitespace */
	CurrPtr = ConsumeWhiteSpace(String);
	if (CurrPtr == NULL) {
		Token[0] = '\0';
		goto Done;
	}

	/* If the next character is the FORCE_DELIM character, treat it	*/
	/* as a token all by itself.					*/
	if (*CurrPtr == DEST_FORCE_DELIM_CHAR) {
		Token[0] = *CurrPtr;
		Token[1] = '\0';

		++CurrPtr;
		goto Done;
	}

	/* Look for the end of the next token and copy it over */
	TokenLen = strcspn(CurrPtr, DEST_TOKEN_DELIM);
	CopyLen  = (TokenLen > MAX_TOKEN_LEN) ? MAX_TOKEN_LEN : TokenLen;

	if (CopyLen > 0) {
		strncpy(Token, CurrPtr, CopyLen);
	}
	Token[CopyLen] = '\0';

	/* Convert the token to upper case */
	ffscConvertToUpperCase(Token);

	/* Print a warning if we had to truncate the token */
	if (CopyLen < TokenLen) {
		ffscMsg("Warning - truncated oversized token: \"%s\"", Token);
	}

	/* Update the string pointer */
	CurrPtr += TokenLen;

	/* Come here when ready to exit */
    Done:

	/* Translate the token if possible */
	if (ValuePtr != NULL) {
		*ValuePtr = TranslateToken(Token);
	}

	/* Return the current string pointer */
	return CurrPtr;
}


/*
 * MapAllRacks
 *	Sets the specified bitmap to select all currently known racks.
 *	Returns OK/ERROR.
 */
STATUS
MapAllRacks(rackmap_t *RackMap)
{
	int i;
	int NumRacks;
	rackid_t RackList[MAX_RACKS];

	if (RackMap == NULL) {
		return ERROR;
	}

	*RackMap = 0;
	NumRacks = identEnumerateRacks(RackList, MAX_RACKS);
	for (i = 0;  i < NumRacks;  ++i) {
		*RackMap |= (1 << RackList[i]);
	}

	return OK;
}


/*
 * MapRacks
 *	Select the specified contiguous range of racks in the specified
 *	bitmap. If the starting rack is < 0, only the ending rack is
 *	selected. Otherwise, both rack values are checked for appropriate
 *	range values. Returns OK/ERROR.
 */
STATUS
MapRacks(int Start, int End, rackmap_t *Map)
{
	int i;

	/* We boldly assume that Map is non-NULL */

	/* A negative start value is equivalent to start == end */
	if (Start < 0) {
		Start = End;
	}

	/* Make sure Start is not out of range */
	else if (Start >= MAX_RACKS) {
		ffscMsg("Invalid starting rack address: %d", Start);
		return ERROR;
	}

	/* Make sure End is not out of range */
	if (End <= 0  ||  End >= MAX_RACKS) {
		ffscMsg("Invalid rack address: %d", End);
		return ERROR;
	}

	/* Swap start & end if they are backward */
	if (Start > End) {
		int Tmp = End;

		End = Start;
		Start = Tmp;
	}

	/* Select the appropriate racks */
	for (i = Start;  i <= End;  ++i) {
		*Map |= (1 << i);
	}

	return OK;
}


/*
 * ParseBayList
 *	Parses the given string as a list of bays, and updates the
 *	specified bitmap accordingly. Returns 0K/ERROR.
 */
STATUS
ParseBayList(char *String, baymap_t *Map)
{
	int i;

	/* Handle the trivial cases */
	if (String == NULL  ||  String[0] == '\0'  ||  Map == NULL) {
		return ERROR;
	}

	/* Loop through each character in the string */
	i = 0;
	for (i = 0;  String[i] != '\0';  ++i) {
		if (String[i] == DEST_ITEM_SEP_CHAR) {
			/* No action */;
		}
		else {
			bayid_t BayID;

			if (ffscParseBayName(String[i], &BayID) != OK) {
				return ERROR;
			}

			*Map |= (1 << BayID);
		}
	}

	return OK;
}


/*
 * ParseModuleList
 *	Parses the given string as a list of modules, and updates the
 *	specified dest_t accordingly. Returns OK/ERROR.
 */
STATUS
ParseModuleList(char *String, dest_t *Dest)
{
	int  i;
	int  CurrValue = -1;
	int  RangeBegin = -1;

	/* Handle the trivial cases */
	if (String == NULL  ||  String[0] == '\0') {
		return ERROR;
	}

	/* Loop through each character in the string */
	i = 0;
	for (i = 0;  String[i] != '\0';  ++i) {

		if (isdigit(String[i])) {
			int Digit = (String[i] - '0');

			if (CurrValue < 0) {
				CurrValue = Digit;
			}
			else {
				CurrValue = (CurrValue * 16) + Digit;
			}
		}
		else if (String[i] >= 'a'  &&  String[i] <= 'f') {
			int Digit = (String[i] - 'a') + 10;

			if (CurrValue < 0) {
				CurrValue = Digit;
			}
			else {
				CurrValue = (CurrValue * 16) + Digit;
			}
		}
		else if (String[i] >= 'A'  &&  String[i] <= 'F') {
			int Digit = (String[i] - 'A') + 10;

			if (CurrValue < 0) {
				CurrValue = Digit;
			}
			else {
				CurrValue = (CurrValue * 16) + Digit;
			}
		}
		else if (String[i] == DEST_ITEM_SEP_CHAR) {
			if (SelectModules(Dest, RangeBegin, CurrValue) < 0) {
				return ERROR;
			}

			RangeBegin = -1;
			CurrValue  = -1;
		}
		else if (String[i] == DEST_RANGE_SEP_CHAR) {
			RangeBegin = CurrValue;
			CurrValue  = -1;
		}
	}

	/* If there is no current value by the time we make it to here	*/
	/* then there is some kind of syntax error			*/
	if (CurrValue < 0) {
		ffscMsg("Syntax error in module list: \"%s\"", String);
		return ERROR;
	}

	/* Otherwise, set the current value/range as "selected" */
	return SelectModules(Dest, RangeBegin, CurrValue);
}





/*
 * ParsePartList
 *	Parses the given string as a list of partitions, and updates the
 *	specified dest_t accordingly. Returns OK/ERROR.
 */
STATUS 
ParsePartList(char *String, dest_t *Dest)
{
  int  i;
  int  CurrValue = -1;
  int  RangeBegin = -1;
  
  /* Handle the trivial cases */
  if (String == NULL  ||  String[0] == '\0') {
    return ERROR;
  }
  
	/* Loop through each character in the string */
  i = 0;
  for (i = 0;  String[i] != '\0';  ++i) {
    
    if (isdigit(String[i])) {
      int Digit = (String[i] - '0');
      
      if (CurrValue < 0) {
	CurrValue = Digit;
	ffscMsg("ParsePartList: got digit %d\n\r",CurrValue);
      }
      else {
	CurrValue = (CurrValue * 16) + Digit;
      }
    }
    else if (String[i] >= 'a'  &&  String[i] <= 'f') {
      int Digit = (String[i] - 'a') + 10;
      
      if (CurrValue < 0) {
	CurrValue = Digit;
      }
      else {
	CurrValue = (CurrValue * 16) + Digit;
      }
    }
    else if (String[i] >= 'A'  &&  String[i] <= 'F') {
      int Digit = (String[i] - 'A') + 10;
      
      if (CurrValue < 0) {
	CurrValue = Digit;
      }
      else {
	CurrValue = (CurrValue * 16) + Digit;
      }
    }
    else if (String[i] == DEST_ITEM_SEP_CHAR) {
      if (SelectPartitions(Dest, RangeBegin, CurrValue) < 0) {
	ffscMsg("ParsePartList: error selecting partitions!\n\r");
	return ERROR;
      }
      
      RangeBegin = -1;
      CurrValue  = -1;
    }
    else if (String[i] == DEST_RANGE_SEP_CHAR) {
      RangeBegin = CurrValue;
      CurrValue  = -1;
    }
  }

  /* If there is no current value by the time we make it to here	*/
  /* then there is some kind of syntax error			*/
  if (CurrValue < 0) {
    ffscMsg("Syntax error in module list: \"%s\"", String);
    return ERROR;
  }  
  /* Otherwise, set the current value/range as "selected" */
  return SelectPartitions(Dest, RangeBegin, CurrValue);
}


/*
 * ParseRackList
 *	Parses the given string as a list of racks, and updates the
 *	specified bitmap accordingly. Returns OK/ERROR.
 */
STATUS
ParseRackList(char *String, rackmap_t *Map)
{
	int  i;
	int  CurrValue = -1;
	int  RangeBegin = -1;

	/* Handle the trivial cases */
	if (String == NULL  ||  String[0] == '\0'  ||  Map == NULL) {
		return ERROR;
	}

	/* Loop through each character in the string */
	i = 0;
	for (i = 0;  String[i] != '\0';  ++i) {

		if (isdigit(String[i])) {
			int Digit = (String[i] - '0');

			if (CurrValue < 0) {
				CurrValue = Digit;
			}
			else {
				CurrValue = (CurrValue * 10) + Digit;
			}
		}
		else if (String[i] == DEST_ITEM_SEP_CHAR) {
			if (MapRacks(RangeBegin, CurrValue, Map) != OK) {
				return ERROR;
			}

			RangeBegin = -1;
			CurrValue  = -1;
		}
		else if (String[i] == DEST_RANGE_SEP_CHAR) {
			RangeBegin = CurrValue;
			CurrValue  = -1;
		}
	}

	/* If there is no current value by the time we make it to here	*/
	/* then there is some kind of syntax error			*/
	if (CurrValue < 0) {
		ffscMsg("Syntax error in rack list: \"%s\"", String);
		return ERROR;
	}

	/* Otherwise, set the current value/range as "selected" */
	return MapRacks(RangeBegin, CurrValue, Map);
}


/*
 * ParseShortSyntaxDest
 *	Parse the given string as a short-syntax destination specification,
 *	and append the resulting dest_t to the specified list. Returns 0 if
 *	successful or -1 if not (i.e. the string is not a valid short-syntax
 *	destination).
 *		The short syntax consists of a rack list followed immediately
 *	by a bay list, optionally separated from each other with a "/".
 *	Either portion may be omitted, which results in an appropriate
 *	default being used. Note that to avoid ambiguity when skipping the
 *	rack portion, it is necessary to either use alphabetic bay address
 *	abbreviations or start the string with "/".
 */
STATUS
ParseShortSyntaxDest(char *String,  dest_t *Dest)
{
	baymap_t BayMap;
	char CurrDest[DEST_STRLEN];
	char List[MAX_TOKEN_LEN + 1];
	char *CurrPtr = String;
	int Span;
	rackmap_t RackMap;

	/* Start with fresh bitmaps */
	BayMap  = 0;
	RackMap = 0;
	CurrDest[0] = '\0';

	/* Process the rack portion of the destination, if present */
	Span = strspn(CurrPtr, DEST_VALID_LIST);
	if (Span > 0) {

		/* Isolate the rack list */
		strncpy(List, CurrPtr, Span);
		List[Span] = '\0';

		/* Parse the rack list */
		if (ParseRackList(List, &RackMap) != OK) {
			return ERROR;	/* Invalid list */
		}

		/* Make a note in the human-readable string */
		strcat(CurrDest, "R ");
		strcat(CurrDest, List);

		/* Move past the rack list */
		CurrPtr += Span;
	}
	else if (*CurrPtr == DEST_ABBREV_ALL_CHAR) {
		if (MapAllRacks(&RackMap) != OK) {
			return ERROR;	/* Trouble */
		}
		strcat(CurrDest, "R *");
		++CurrPtr;
	}
	else if (*CurrPtr == DEST_ABBREV_LOCAL_CHAR) {
		RackMap = 1 << ffscLocal.rackid;
		strcat(CurrDest, "R .");
		++CurrPtr;
	}
	else {
		/* No rack portion, use local rack by default */
		RackMap = 1 << ffscLocal.rackid;
		strcat(CurrDest, "R .");
	}

	/* If the separator character is present, simply consume it */
	if (*CurrPtr == DEST_SSD_SEP_CHAR) {
		++CurrPtr;
	}

	/* Process the bay list, if present */
	Span = strspn(CurrPtr, DEST_VALID_BAYLIST);
	if (Span > 0  &&  Span == strlen(CurrPtr)) {

		/* Normal bay list */
		if (ParseBayList(CurrPtr, &BayMap) != OK) {
			return ERROR;	/* Invalid list */
		}

		/* Make a note in the human-readable string */
		strcat(CurrDest, " B ");
		strcat(CurrDest, CurrPtr);

		/* Move past the bay list */
		CurrPtr += Span;
	}
	else if (*CurrPtr == DEST_ABBREV_ALL_CHAR) {
		BayMap = BAYMAP_ALL | BAYID_BITMAP_ALL;
		strcat(CurrDest, " B * ");
		++CurrPtr;
	}
	else {
		/* No bay list at all, so use default */
		BayMap = BAYMAP_ALL | BAYID_BITMAP_ALL;
		strcat(CurrDest, " B *");
	}

	/* At this stage, there should be nothing left in the string. */
	/* If there is, then this is not a valid destination.	      */
	if (*CurrPtr != '\0') {
		return ERROR;
	}

	/* Add the selected racks*bays to the dest map */
	SelectRacksBays(Dest, RackMap, BayMap);
	if (AppendDestString(Dest, CurrDest) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * PrintTokenValue
 *	Print the first token name found for the specified token value.
 *	Useful for debugging.
 */
void
PrintTokenValue(int Value)
{
	int i;

	if (Value == TOKEN_FORCE_DELIM) {
		ffscMsg("TOKEN <DELIM>");
		return;
	}

	for (i = 0;  TokenTable[i].name != NULL;  ++i) {
		if (TokenTable[i].value == Value) {
			ffscMsg("TOKEN %s", TokenTable[i].name);
			return;
		}
	}

	ffscMsg("TOKEN <unknown!>");
}


/*
 * SelectAllModules
 *	Selects all known modules in the specified dest_t.
 *	Returns OK/ERROR.
 */
STATUS
SelectAllModules(dest_t *Dest)
{
	int i;
	int NumModules;
	modulenum_t ModuleList[MAX_RACKS * MAX_BAYS];

	if (Dest == NULL) {
		ffscMsg("SelectAllModules called with NULL dest_t");
		return ERROR;
	}

	NumModules = identEnumerateModules(ModuleList, MAX_RACKS * MAX_BAYS);
	if (NumModules == 0) {
		baymap_t  AllBays;
		rackmap_t AllRacks;

		/* In the unfortunate instance of having no modules */
		/* defined, fall back to "rack all bay all"	    */
		ffscMsg("No modules are defined - defaulting to r * b *");

		if (MapAllRacks(&AllRacks) != OK) {
			return ERROR;	/* Trouble */
		}
		AllBays = BAYMAP_ALL | BAYID_BITMAP_ALL;
		SelectRacksBays(Dest, AllRacks, AllBays);

		return OK;
	}

	for (i = 0;  i < NumModules;  ++i) {
		if (SelectModule(Dest, ModuleList[i]) != OK) {
			return ERROR;
		}
	}

	return OK;
}


/*
 * SelectModule
 *	Selects a single specified module in the specified dest_t.
 *	Returns OK/ERROR;
 */
STATUS
SelectModule(dest_t *Dest, modulenum_t ModuleNum)
{
	bayid_t    Bay;
	identity_t *Ident;

	/* Make sure the input arguments are OK */
	if (Dest == NULL) {
		ffscMsg("SelectModule called with NULL dest_t");
		return ERROR;
	}

	/* Look up this module */
	Ident = identFindByModuleNum(ModuleNum, &Bay);
	if (Ident == NULL  ||  Bay == BAYID_UNASSIGNED) {
		return ERROR;
	}

	/* Select the appropriate rack and bay */
	Dest->Map[Ident->rackid] |= (1 << Bay);
	Dest->Flags &= ~DESTF_NONE;

	return OK;
}


/*
 * SelectModules
 *	Select the specified contiguous range of modules in the specified
 *	dest_t. If the starting module is < 0, only the ending rack is
 *	selected. Otherwise, both module values are checked for appropriate
 *	range values. Returns OK/ERROR.
 */
STATUS
SelectModules(dest_t *Dest, int Start, int End)
{
	int NumFound;
	modulenum_t CurrModule;

	/* A negative start value is equivalent to start == end */
	if (Start < 0) {
		Start = End;
	}

	/* Make sure Start is not out of range */
	else if (Start > MODULENUM_MAX) {
		ffscMsg("Invalid starting module address: %d", Start);
		return ERROR;
	}

	/* Make sure End is not out of range */
	if (End < 0  ||  End > MODULENUM_MAX) {
		ffscMsg("Invalid module address: %d", End);
		return ERROR;
	}

	/* Swap start & end if they are backward */
	if (Start > End) {
	  int Tmp = End;
	  End = Start;
	  Start = Tmp;
	}

	/* Select the appropriate rack/bay pairs. We "sort of" ignore  */
	/* errors in that we allow some modules in the specified range */
	/* to be missing (we already know they must be valid), but we  */
	/* will complain if *none* of the modules in the range exist.  */
	NumFound = 0;
	for (CurrModule = Start;  CurrModule <= End;  ++CurrModule) {
		if (SelectModule(Dest, CurrModule) == OK) {
			++NumFound;
		}
	}

	/* If no modules were found at all, that's an error */
	if (NumFound < 1) {
	  return ERROR; 
	}

	return OK;
}

/*
 * SelectPartitions
 *	Select the specified contiguous range of partitions in the specified
 *	dest_t. If the starting partition is < 0, only the ending rack is
 *	selected. Otherwise, both part values are checked for appropriate
 *	range values. Returns OK/ERROR.
 */
static STATUS
SelectPartitions(dest_t *Dest, int Start, int End)
{
  int i,NumFound;
  modulenum_t partId;
  part_info_t* p = partInfo;
  
  /* A negative start value is equivalent to start == end */
  if (Start < 0) 
    Start = End;
  
  /* Make sure Start is not out of range (255 partitions max) */
  else if (Start > MODULENUM_MAX) {
    ffscMsg("Invalid starting partition address: %d", Start);
    return ERROR;
  }
  
  /* Make sure End is not out of range */
  if (End < 0  ||  End > MODULENUM_MAX) {
    ffscMsg("Invalid partition address: %d", End);
    return ERROR;
  }
  
  /* Swap start & end if they are backward */
  if (Start > End) {
    int Tmp = End;
    ffscMsg("Swapping partition range backwards,swapping.\n\r");
    End = Start;
    Start = Tmp;
  }
  
  /*
   *  Select the appropriate rack/bay pairs. We "sort of" ignore 
   * errors in that we allow some modules in the specified range
   * to be missing (we already know they must be valid), but we  
   * will complain if *none* of the modules in the range exist.  
   * Basically, we go through the entire partition List, and if the 
   * PartitionID matches one of those in the list, then we go through
   * its list of modules and select each one.
   */

  /*
   * Make sure we are coherent 
   * @@: TODO -make smarter so we don't do this all the time.
   */
#if 1
  broadcastPartitionInfo(&partInfo);
  /* Mutex enter  */
  semTake(part_mutex, WAIT_FOREVER);  
#endif
  NumFound = 0;
  for (partId = Start;  partId <= End;  ++partId) {
    while(p != NULL){
      if(p->partId == partId)
	for(i = 0; i < p->moduleCount; i++){
	  if (SelectModule(Dest, (modulenum_t)p->modules[i]) == OK){
	    ++NumFound;
	    ffscMsg("Selecting module[%d]\n\r", p->modules[i]);
	  }
	}
      p = p->next;
    }
  }
#if 1
  /* Mutex exit */
  semGive(part_mutex);     
#endif
  /* If no modules were found at all, that's an error */
  if (NumFound < 1){
    ffscMsg("NOTICE: some or all of the modules may be offline.\r\n");
    /*     return ERROR; */
  }
  
  return OK;
}




/*
 * SelectRacksBays
 *	Selects the specified bays from the specified racks in the
 *	specified dest_t. The bays and racks are in the form of bitmaps.
 *	In the special case of BAY ALL, the *local* rack will only be
 *	set up with bays known to be online.
 */
void
SelectRacksBays(dest_t *Dest, rackmap_t RackMap, baymap_t BayMap)
{
	int i;

	if (RackMap == 0  &&  BayMap == 0) {
		return;
	}

	for (i = 0;  i < MAX_RACKS;  ++i) {
		if (!(RackMap & (1 << i))) {
			continue;
		}

		if (i == ffscLocal.rackid  &&  (BayMap & BAYMAP_ALL)) {
			int j;

			for (j = 0;  j < MAX_BAYS;  ++j) {
				if (!(ELSC[j].Flags & EF_OFFLINE)) {
					Dest->Map[i] |= (1 << j);
				}
			}
			Dest->Map[i] |= BAYMAP_ALL;
		}
		else {
			Dest->Map[i] |= BayMap;
		}
	}

	Dest->Flags &= ~DESTF_NONE;
}


/*
 * TranslateToken
 *	Examines the specified string to see if it is a standard
 *	token name. Returns the token value, or TOKEN_UNKNOWN if
 *	it is not a standard token.
 *	   Note that a return value of TOKEN_*LIST or TOKEN_SSD
 *	simply means the token consists only of valid list/SSD
 *	characters, not that the token is syntactically correct.
 */
int
TranslateToken(const char *Name)
{
	int i;
	int NameLen = strlen(Name);
	int Span;

	/* Check for the trivial case */
	if (Name == NULL) {
		return TOKEN_EMPTY;
	}

	/* Check for keywords */
	for (i = 0;  TokenTable[i].name != NULL;  ++i) {
		if (strcmp(TokenTable[i].name, Name) == 0) {
			if (ffscDebug.Flags & FDBF_PARSEDEST) {
				PrintTokenValue(TokenTable[i].value);
			}
			return TokenTable[i].value;
		}
	}

	/* Check for a potential simple list */
	Span = strspn(Name, DEST_VALID_LIST);
	if (Span == NameLen) {
		if (ffscDebug.Flags & FDBF_PARSEDEST) {
			ffscMsg("TOKEN LIST \"%s\"", Name);
		}
		return TOKEN_LIST;
	}

	/* Check for a potential module list */
	Span = strspn(Name, DEST_VALID_MODLIST);
	if (Span == NameLen) {
		if (ffscDebug.Flags & FDBF_PARSEDEST) {
			ffscMsg("TOKEN MODLIST \"%s\"", Name);
		}
		return TOKEN_MODLIST;
	}

	/* Check for a potential bay list */
	Span = strspn(Name, DEST_VALID_BAYLIST);
	if (Span == NameLen) {
		if (ffscDebug.Flags & FDBF_PARSEDEST) {
			ffscMsg("TOKEN BAYLIST \"%s\"", Name);
		}
		return TOKEN_BAYLIST;
	}

	/* @@@: TODO */


	/* Check for a potential short-syntax description */
	Span = strspn(Name, DEST_VALID_SSD1);
	if (Span == NameLen  ||
	    Span + strspn(Name + Span, DEST_VALID_SSD2) == NameLen)
	{
		if (ffscDebug.Flags & FDBF_PARSEDEST) {
			ffscMsg("TOKEN SSD \"%s\"", Name);
		}
		return TOKEN_SSD;
	}

	/* If we make it this far, we don't know what the token is */
	if (ffscDebug.Flags & FDBF_PARSEDEST) {
		ffscMsg("TOKEN UNKNOWN \"%s\"", Name);
	}
		
	return TOKEN_UNKNOWN;
}



#ifndef PRODUCTION

#include "user.h"

/*----------------------------------------------------------------------*/
/*									*/
/*			      DEBUG FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * destShow
 *	Decode the contents of the specified dest_t
 */
STATUS
destShow(dest_t *Dest)
{
	int i, j;

	if (Dest == NULL) {
		Dest = &userTerminal->DfltDest;
	}

	ffscMsg("dest_t @%p:", Dest);
	ffscMsg("    Flags 0x%08x", Dest->Flags);
	ffscMsg("    String: %s", Dest->String);

	ffscMsgN("    Map:");
	for (i = 0;  i < MAX_RACKS;  ++i) {
		if (!destRackIsSelected(Dest, i)) {
			continue;
		}

		ffscMsgN("  R %d B ", i);
		if (destAllBaysSelected(Dest, i)) {
			ffscMsgN("*");
		}
		else {
			for (j = 0;  j < MAX_BAYS;  ++j) {
				if (destBayIsSelected(Dest, i, j)) {
					ffscMsgN("%c", ffscBayName[j]);
				}
			}
		}
	}
	ffscMsg("");

	return OK;
}

#endif  /* !PRODUCTION */
