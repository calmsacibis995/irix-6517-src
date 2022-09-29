/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision"

/*LINTLIBRARY*/
#include <sys/mac_label.h>
#include <mls.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static void __insert_lbldblist( char *, char * );
static void __insert_dblist( int, char *, int );
static int __strip_whitespace( char * );
static int _chkfield_value( int, int );
static int __is_text( char * );
static int __pack_db( void );

#define NAMES_PATH	"/etc/mac_label/"
#ifdef DEBUG
/* constant define for secadm data base file path */
#define LABELS		"labels.tmp"
#define MSEN_TYPES	"msentypes.tmp"
#define LEVELS		"levels.tmp"
#define CATEGORIES	"categorys.tmp"
#define MINT_TYPES	"minttypes.tmp"
#define GRADES		"grades.tmp"
#define DIVISIONS	"divisions.tmp"
#else
/* constant define for secadm data base file path */
#define LABELS		"labelnames"
#define MSEN_TYPES	"msentypenames"
#define LEVELS		"levelnames"
#define CATEGORIES	"categorynames"
#define MINT_TYPES	"minttypenames"
#define GRADES		"gradenames"
#define DIVISIONS	"divisionnames"
#endif

/*
 * global variables used for Truested IRIX/B library functions
 */
char *__mac_mlsfile[MAX_MLSFILES] = { /* array of database file path */
        LABELS,
        MSEN_TYPES,
        LEVELS,
        CATEGORIES,
        MINT_TYPES,
        GRADES,
        DIVISIONS};

int  __mls_entry_total[] = {
0,	/* total entry count for labels database */
0,	/* total entry entry for msentypes database */
0,	/* total entry count for levels database */
0,
0,
0,
0};

DBENT *__mac_mdblist[MAX_MLSFILES];     /* array of ptr to entity db list */
LBLDBINFO *__mac_bin_lbldblist=0;	/* pointer to binary label db list */
DBINFO *__mac_bin_mdblist[MAX_MLSFILES]; /* pointer to binary entity db list */
LBLDB_BIN_HDR __mac_lhead;		/* header for labelnames binary file */
DB_BIN_HDR __mac_dhead[MAX_MLSFILES];	/* header for entity binary file */
int __mac_mls_init_inprocess;		/* indicate mls_init is in_process */
int __mac_bad_label_entry=0;		/* any bad labelname entry exist */
int __mac_db_error=0;

char mlsfile[MAXPATHLEN];

extern char *newroot;

/* constant define for warning message */
#define OPEN_FAIL       0
#define NAME_FAIL	OPEN_FAIL+1
#define VALUE_FAIL      NAME_FAIL+1
#define SYNTAX_FAIL     VALUE_FAIL+1
#define UNIQUE_FAIL     SYNTAX_FAIL+1
#define LBLWRITE_FAIL   UNIQUE_FAIL+1
#define LABEL_FAIL      LBLWRITE_FAIL+1
#define INIT_FAIL       LABEL_FAIL+1
#define PACK_FAIL       INIT_FAIL+1
#define READ_FAIL       PACK_FAIL+1

extern void __inform_user( int, char * );

/*
 * mls_init()
 * initialize the mls database file, open the file, read in the data entry
 * store into the entry list and packed in binary file format for mls_mkdb()
 * This function will be performed only by the action of system adminstrator.
 */
int
mls_init()
{
	FILE *fp;
	int i,ftype,field_value;
	char line[MAX_MLS_LINE_LEN];
	static char saveline[MAX_MLS_LINE_LEN];
	char *namestr, *valuestr;
	char *colonp, *backslashp, *lastchar, *compstr, *endchar;
	int more_line;

	/*
	 * a flag indicate mls_init is in process
	 */
	__mac_mls_init_inprocess = 1;

        /*
	 * read in labels,msentypes, levels, categorys, minttypes, grades and
         * divisions files, stored the entry to link list
	 */

	more_line = 0;
	saveline[0] = '\0';
	for (ftype=0; ftype< MAX_MLSFILES; ftype++) {
		if (newroot != NULL)
			strcpy(mlsfile, newroot);
		else
			strcpy(mlsfile, NAMES_PATH);
		i = strlen(mlsfile);
		if (mlsfile[i-1] != '/')
			strcat(mlsfile, "/");
		strcat(mlsfile, __mac_mlsfile[ftype]);

		if ( (fp = fopen(mlsfile,"r"))  == NULL) {
			__inform_user(OPEN_FAIL, mlsfile);
			continue;
		}
		while (fgets(line, MAX_MLS_LINE_LEN, fp) != NULL) {
			/*
			 * if the end of line contains backslash char,
			 * treats it as continuation mark for next line
			 */
			if ((backslashp = index(line, '\\')) != NULL) {
				if (strlen(backslashp) > 2) {
					continue;
				}
				*backslashp = '\0';
				strcat(saveline,line);
				for (i=0; i<strlen(line); i++)
					line[i] = '\0';
				more_line = 1;
				continue;
			}
			if (more_line == 1) {
				strcat(saveline, line);
				strcpy(line,saveline);
				more_line = 0;
				for (i=0; i<strlen(saveline); i++)
					saveline[i] = '\0';
			}

                        /*
                         * NULL the newline character. If there
                         * is none go on.
                         */
                        if ((endchar = index(line, '\n')) == NULL)
                                continue;
                        *endchar = '\0';

			/*
			 * ignore empty line
			 */
			if (strlen(line) == 0)
				continue;
			/*
			 * if the line contains space or tab chars at the
			 * beginning or end of line, strip them
			 */
			if (index(line, ' ') != NULL ||
			    index(line, TAB_CHAR) != NULL) {
				if (__strip_whitespace(&line[0]) < 0) {
					__inform_user(SYNTAX_FAIL,line);
					continue;
				}
			}

			/*
			 * If the first char is pound sign, it is a
			 * comment line, ignore it
			 */
			if (line[0] ==  '#')
				continue;


			/*
			 * if the line contains double quote or does not
			 * colon, report syntax error and go to next line
			 */
            		if ( (colonp = index(line, ':')) == NULL ||
			   index(line, '"') != NULL ) {
                		__inform_user(SYNTAX_FAIL, line);
				continue;
			}

        		*colonp = '\0';
			namestr = line;

        	       	/*
         		 * The name length should be within range, and the
			 * name must leading with a ASCII character A to Z
			 * or a to z and the name can not contains slash
			 * character.
         		 */
        		if ((strlen(namestr) >= MAX_MLS_NAME_LEN) ||
			    (__is_text(namestr) < 0) ||
			    (index(namestr, '/') != NULL) ) {
				__inform_user(NAME_FAIL, namestr);
                		continue;
			}

			if (ftype == MAC_LABEL) {
				compstr = colonp + 1;
				if (!__mac_lbldblist) {
					__mac_lbldblist = (LBLDBLIST *)
						malloc(sizeof(LBLDBLIST));
					__mac_lbldblist->head = NULL;
					__mac_lbldblist->tail = NULL;
				}
				__insert_lbldblist(namestr, compstr);
			} else {
				/* check field value validity */
				valuestr = colonp + 1;
				field_value = strtol(valuestr, NULL,0);
				if (_chkfield_value(ftype, field_value) < 0) {
					__inform_user(VALUE_FAIL, valuestr);
					continue;
				}
				__insert_dblist(ftype, namestr, field_value);
			}
		}
	}

	/*
	 * if the mls database has error entry, report init fail
	 */

	if (__mac_db_error == 1)
		return(1);
	__pack_db();
	__mac_mls_init_inprocess =0;
}


/*
 * __insert_lbldblist(name, comp)
 * insert each label/component name string entry into linear linked list
 * also calculate the total entry count and totol byte size for later
 * usage
*/
static void
__insert_lbldblist(char *name, char *comp)
{

	LBLDBENT	*ptr, *vptr;
	LBLDBLIST 	*vlist;

        ptr = (LBLDBENT *) malloc(sizeof(LBLDBENT));
	ptr->name = strdup(name);
	ptr->comp = strdup(comp);
	ptr->next = NULL;


        if (__mac_lbldblist->head == NULL) {
                __mac_lbldblist->head = __mac_lbldblist->tail = ptr;
        }
        else {
                __mac_lbldblist->tail->next = ptr;
                __mac_lbldblist->tail = ptr;
        }

	__mls_entry_total[MAC_LABEL]++;
}

/*
 * __insert_dblist(ftype, name, value)
 * insert each data entry into the entry list, the list is sorted by value
 * the list is a circular link list structure. The data base entry include
 * the name & value information for msentypes, levels, categorys, minttypes
 * grades or divisions entity
 */
static void
__insert_dblist(int ftype, char *name, int value)
{
	DBENT   *vptr, *vlist;
	DBENT	*p, *ptr;
	int i, strsize;


	vptr = (struct dbent *) malloc(sizeof(DBENT));
	vptr->name = strdup(name);
	vptr->value = value;

	if ( __mac_mdblist[ftype] == NULL) {
		vlist =__mac_mdblist[ftype] = malloc(sizeof(DBENT));
		vlist->next = vlist->prev = vlist;
		vlist->value = INVALID_ENTRY_VALUE;
		vlist->name = '\0';
	}
	vlist = __mac_mdblist[ftype];
	p = vlist->prev;
	while (value < p->value && p->prev != vlist) {
		p = p->prev;
	}
	/*
	 * We want to insert vptr after of p.
	 * That is, we want to insert vptr between p and p-next.
	 * insert p into the list
	 */
	vptr->prev = p;
	vptr->next = p->next;
	p->next->prev = vptr;
	p->next = vptr;
	__mls_entry_total[ftype]++;
	vptr = vlist->next;
}

/*
 * __strip_whitespace(line)
 * strip leading or trailing space key and tab key from a line
 */

static int
__strip_whitespace(char *line)
{
	char *s, *p, *t, *newstr;
	int charcnt=0;
	int i;


	s = line;
	p = newstr = malloc(strlen(line));

	while (*s != '\0') {
		if (*s == ' ' || *s == TAB_CHAR) {
			if (charcnt > 0) {
				t = s+1;
				while (*t++ != '\0') {
					if (*t != ' ' && *t != TAB_CHAR)
						return(-1);
				}
			}
		}
		else {
			charcnt++;
			*p++ = *s;
			if (charcnt == 1 && *s == '#')
				break;
		}
		s++;
	}
	*p = '\0';

	strcpy(line, newstr);

	free(newstr);
}

/*
 * _chkfield_value(ftype,value)
 * check the field value validity, the field includes msentype, level,
 * category, minttype, grade & division.
 */
static int
_chkfield_value(int ftype, int value)
{
	extern const char __mac_type_info[];
	switch (ftype) {
	case MAC_MSEN_TYPE:
		switch (value) {
		case MSEN_ADMIN_LABEL:
		case MSEN_EQUAL_LABEL:
		case MSEN_HIGH_LABEL:
		case MSEN_MLD_HIGH_LABEL:
		case MSEN_LOW_LABEL:
		case MSEN_MLD_LABEL:
		case MSEN_MLD_LOW_LABEL:
		case MSEN_TCSEC_LABEL:
			return(0);
		default:
			break;
		}
		break;
	case MAC_MINT_TYPE:
		switch (value) {
		case MINT_BIBA_LABEL:
		case MINT_EQUAL_LABEL:
		case MINT_HIGH_LABEL:
		case MINT_LOW_LABEL:
			return(0);
		default:
			break;
		}
		break;
	case MAC_LEVEL:
		if (value >= MSEN_MIN_LEVEL && value <= MSEN_MAX_LEVEL)
			return(0);
		break;
	case MAC_GRADE:
		if (value >= MINT_MIN_GRADE && value <= MINT_MAX_GRADE)
			return(0);
		break;
	case MAC_CATEGORY:
		if (value >= MSEN_MIN_CATEGORY && value <= MSEN_MAX_CATEGORY)
			return(0);
		break;
	case MAC_DIVISION:
		if (value >= MINT_MIN_DIVISION && value <= MINT_MAX_DIVISION )
			return(0);
		break;
	default:
		break;
	}

	return(-1);
}

/*
 * pack_db()
 * collect the information from the linked list, allocate memory to
 * store the information in an array format, prepare for loading
 * information into binary file format
 */
static int
__pack_db(void)
{
	LBLDB_BIN_ENT *llist;
	DB_BIN_ENT *dlist;
	LBLDBENT *pl;
	DBENT	*pd;
	int i,j;
	mac_label *lp;
	int lblsize=0;
	int ftype;


	/*
	 * get the data from linked list and pack the labels information
	 */
	__mac_bin_lbldblist = (LBLDBINFO *) malloc(sizeof(LBLDBINFO));

	llist = __mac_bin_lbldblist->list = (LBLDB_BIN_ENT *)
		malloc( __mls_entry_total[MAC_LABEL] * sizeof(LBLDB_BIN_ENT) );
	__mac_lhead.entry_total = __mls_entry_total[MAC_LABEL];
	__mac_lhead.entries_bytes = __mls_entry_total[MAC_LABEL] * sizeof(LBLDB_BIN_ENT);

	if (__mac_lbldblist) {
		pl = __mac_lbldblist->head;
		for (i=0; i<__mls_entry_total[MAC_LABEL]; i++) {
			llist->strlen = (unsigned short) strlen(pl->name);
			strcpy(llist->name, pl->name);
			llist->lblsize = 0;
#ifdef DEBUG
			printf("\ndatabase -- name: %s, comp : %s\n", pl->name, pl->comp);
#endif
			if ( (lp = (mac_label *) mac_strtolabel(pl->comp)) != NULL){
				__dump_maclabel(lp);
				if ((lblsize = mac_size(lp)) > 0) {
					llist->lblsize = (unsigned short) lblsize;
					mac_copy(&llist->label,lp);
#ifdef DEBUG
					__dump_maclabel(&llist->label);
#endif
				}
				else
					__inform_user(LABEL_FAIL, llist->name);
			}
			else {
				/*
				 * if bad label name or label component is
				 * defined in the database file, inform the
				 * system adminstrator and don't generate
				 * binary file
				 */
				__mac_bad_label_entry = 1;
				__inform_user(LABEL_FAIL, llist->name);
			}
			llist++;
			pl = pl->next;
		}
	}

	/*
	 * If there is any bad label defined, don't rebuild binary file
	 * don't process any further
	 */
	if (__mac_bad_label_entry == 1)
		return(-1);

	for (ftype=MAC_MSEN_TYPE; ftype<MAX_MLSFILES; ftype++) {
		__mac_bin_mdblist[ftype] = (DBINFO *) malloc(sizeof(DBINFO));
		dlist = __mac_bin_mdblist[ftype]->list = (DB_BIN_ENT *) malloc
		    (sizeof(DB_BIN_ENT) * __mls_entry_total[ftype]);
		__mac_dhead[ftype].db_type = ftype;
		__mac_dhead[ftype].entry_total = __mls_entry_total[ftype];
#ifdef DEBUG
		printf("__mac_dhead[%d].entry_total = %d\n", ftype, __mac_dhead[ftype].entry_total);
#endif
		__mac_dhead[ftype].entries_bytes = __mac_dhead[ftype].entry_total * sizeof(DB_BIN_ENT);

		/*
		 * go through the circular link list and get the value
	 	 * and name string, load them into binary entry buffer
		 */
		if (__mac_mdblist[ftype]) {
			pd = __mac_mdblist[ftype]->next;
			for (j=0; (j<__mls_entry_total[ftype]) && pd->value != 65535; j++) {
				dlist->value = (unsigned short) pd->value;
				dlist->strlen = (unsigned short) strlen(pd->name);
				strcpy(dlist->name, pd->name);
				pd = pd->next;
				dlist++;
			}
		}
#ifdef DEBUG
		/* __dump_list(ftype); */
#endif

	}
	return(0);

}

/*
 * __is_text(str)
 * check whether the input string is a text string
 */
static int
__is_text(char *str)
{
	if ( (str[0] >= 'A'  && str[0] <= 'Z') ||
		(str[0] >= 'a' && str[0] <= 'z') )
		return(0);
	else
		return(-1);
}

/*
 * __inform_user(err_type, ftype, info)
 * inform the user erring messages for mls library function
 */
void
__inform_user(int err_type, char *info)
{
	static char *err_msg[] = {
	    "** Error: can not open file",
	    "** Error: invalid field name",
	    "** Error: invalid field value",
	    "** Error: syntax error for string",
	    "** Error: non-unique field name",
	    "** Error: can not write to binary file",
	    "** Error: invalid mac label for label string",
	    "** Error: mls_init must be invoked in order to perform mls_unqiue",
	    "** Error: can not pack mls database info into binary file",
	    "** Error: can not read from file"};

	printf("%s ", err_msg[err_type]);
	if (info)
		printf("%s\n", info);
}
