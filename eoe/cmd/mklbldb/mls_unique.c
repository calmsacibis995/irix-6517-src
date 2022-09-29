
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

#ident "$Revision:"

/*LINTLIBRARY*/
#include <sys/mac_label.h>
#include <mls.h>
#include <sys/param.h>
#include <sys/types.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static void __unique_check( int, int, int );
static int __dbnode_cmp( const void *, const void * );
static int __lblnode_cmp( const void *, const void * );

extern LBLDBINFO *__mac_bin_lbldblist;
extern DBINFO *__mac_bin_mdblist[];
extern char *mlsfile;

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
 * mls_unique()
 * check the data field name in all the database files. Each name must
 * unique within labels, msentypes levels, and categorys set or within labels,
 * minttypes,grades and division sets.
 *
*/
int
mls_unique(void)
{
	int ftype;
	int i;

	/*
	 * make sure there is database infomration available for uniqueness
	 * check
	 */

	if (__mac_bad_label_entry == 1 || __mac_db_error == 1) {
		__inform_user(LABEL_FAIL, NULL);
		return(-1);
	}

	for (i=0; i<MAX_MLSFILES; i++) {
		if (__mls_entry_total[i] == 0) {
			__inform_user(INIT_FAIL, mlsfile);
			return(-1);
		}
	}

	/*
	 * the database information must be packed for binary format
	 */
	if (!__mac_bin_lbldblist)
		return(-1);
	for (ftype = MAC_MSEN_TYPE; ftype < MAX_MLSFILES; ftype++)
		if (!__mac_bin_mdblist[ftype])
			return(-1);

	/*
	 * Check the labelname uniqueness within labelnames
	 */
	__unique_check(MAC_LABEL, 0,0);

	/*
	 * Check the entity name uniqueness within the entity files.
	 */
	for (ftype= MAC_MSEN_TYPE; ftype<= MAC_DIVISION; ftype++)
		__unique_check(ftype, 0,0);

	/*
	 * Check the labelname string uniqueness against the MSEN portion and
	 * MINT portion of database file.
	 */
	__unique_check(MAC_LABEL, MAC_MSEN_TYPE, MAC_CATEGORY);
	__unique_check(MAC_LABEL, MAC_MINT_TYPE, MAC_DIVISION);

	/*
	 * compare each msentype name string against the level and
 	 * category names used.
	 */
	__unique_check(MAC_MSEN_TYPE, MAC_LEVEL, MAC_CATEGORY);

	/*
	 * compare each level name string against the category names used.
	 */
	__unique_check(MAC_LEVEL, MAC_CATEGORY, MAC_CATEGORY);


	/*
	 * compare each minttype name string against the grade and
 	 * division names used.
	 */
	__unique_check(MAC_MINT_TYPE, MAC_GRADE, MAC_DIVISION);

	/*
	 * compare each grade name string against the division names used.
	 */
	__unique_check(MAC_GRADE, MAC_DIVISION, MAC_DIVISION);

	return(0);

}

/*
 * __unique_check(ftype, from, to)
 * This subroutine perform the name string uniqueness check for file ftype.
 * It checks the uniqueness within the ftype file itself and against mlfile
 * type "from" to file type "to"
 */
static void
__unique_check(int ftype, int from, int to)
{
	int  i;
	int  j;
	char *key, *table;
	size_t tabmem;
	DB_BIN_ENT *dp;
	LBLDB_BIN_ENT *lp, *llist;
	DB_BIN_ENT *node;
	int dsize,lsize;


	lsize = sizeof(LBLDB_BIN_ENT);
	dsize = sizeof(DB_BIN_ENT);

	for (i=0; i<(__mls_entry_total[ftype]-1); i++) {
		switch (ftype) {
		case MAC_LABEL:
			if (from == 0 && to == 0) {
				/*
				 * compare within the same file
				 */
				key =(char *) &(__mac_bin_lbldblist->list[i]);
				table = (char *) &(__mac_bin_lbldblist->list[i+1]);
				tabmem = (__mls_entry_total[MAC_LABEL] - (i+1));
				lp = (LBLDB_BIN_ENT *) bsearch(key,table,tabmem,
					lsize, __lblnode_cmp);
				if (lp != NULL)
					__inform_user(UNIQUE_FAIL,lp->name);
			} else {
				/*
				 * compare against other files
				 */
				llist = &(__mac_bin_lbldblist->list[i]);
				node = (DB_BIN_ENT *) malloc(sizeof (DB_BIN_ENT));
				node->strlen = llist->strlen;
				strcpy(node->name, llist->name);
				key = (char *)node;
				for (j=from; j<=to; j++) {
					table = (char *) __mac_bin_mdblist[j]->list;
					tabmem = __mls_entry_total[j];
					dp = (DB_BIN_ENT *) bsearch(key,table,
						tabmem, dsize, __dbnode_cmp);
					if (dp != NULL)
						__inform_user(UNIQUE_FAIL,dp->name);
				}
			}
			break;

		case MAC_MSEN_TYPE:
		case MAC_LEVEL:
		case MAC_CATEGORY:
		case MAC_MINT_TYPE:
		case MAC_GRADE:
		case MAC_DIVISION:
			key = (char *) &(__mac_bin_mdblist[ftype]->list[i]);
			if (from == 0 && to == 0) {
				/*
				 * compare within the same file
				 */
				table = (char *) &(__mac_bin_mdblist[ftype]->list[i+1]);
				tabmem = (__mls_entry_total[ftype] - (i+1));
				dp = (DB_BIN_ENT *) bsearch(key,table,tabmem,
					dsize, __dbnode_cmp);
				if (dp != NULL)
					__inform_user(UNIQUE_FAIL,dp->name);
			} else {
				/*
				 * compare against other files
				 */
				for (j=from; j<=to; j++) {
					table = (char *)__mac_bin_mdblist[j]->list;
					tabmem = __mls_entry_total[j];
					dp = (DB_BIN_ENT *) bsearch(key,table,
					     tabmem,dsize, __dbnode_cmp);
					if (dp != NULL)
						__inform_user(UNIQUE_FAIL,dp->name);
				}
			}
			break;
		default:
			break;

		}

	}
}

static int
__dbnode_cmp(const void *p1, const void *p2)
{
        return (strcmp(((DB_BIN_ENT *)p1)->name, ((DB_BIN_ENT *)p2)->name));
}

static int
__lblnode_cmp(const void *p1, const void *p2)
{
        return (strcmp(((LBLDB_BIN_ENT *)p1)->name,
	    ((LBLDB_BIN_ENT *)p2)->name));
}
