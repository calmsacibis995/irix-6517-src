
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
#include <ctype.h>

extern LBLDBINFO *__mac_bin_lbldblist;
extern DBINFO *__mac_bin_mdblist[];
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
 * mls_mkdb()
 * load the database file into binary file. this function
 * is called when the system adminstrator decides to reload the binary
 * mlsfiles
 */
int
mls_mkdb(void)
{
	LBLDBENT	*pl;
	LBLDB_BIN_ENT	*llist;
	mac_label	*lp;
	int 		 type, mem_need, strsize, lblsize;
	DBENT		*pd;
	DB_BIN_ENT	*dlist;
	FILE 		*bfp;
	char		label_db_file[MAXPATHLEN];

	if (newroot != NULL)
		sprintf(label_db_file, "%s/%s", newroot, LABEL_DB);
	else
		sprintf(label_db_file, "%s/%s", LABEL_DB_PATH, LABEL_DB);

	if (__mac_lhead.entry_total == 0 || __mac_bad_label_entry == 1 ||
	    __mac_db_error == 1) {
		__inform_user(PACK_FAIL, label_db_file);
		return(-1);
	}

        if ( (bfp = fopen(label_db_file, "w")) == NULL) {
        	__inform_user(OPEN_FAIL, label_db_file);
		return(-1);
	}

#ifdef DEBUG
	printf("__mac_lhead.entry_total = %d\n", __mac_lhead.entry_total);
	printf("__mac_lhead.entries_bytes = %d\n", __mac_lhead.entries_bytes);
#endif
	/*
	 * load to binary file
	 */
	fwrite(&__mac_lhead, sizeof(LBLDB_BIN_HDR), 1, bfp);
	fwrite(__mac_bin_lbldblist->list, __mac_lhead.entries_bytes, 1, bfp);
	if (ferror(bfp))
		__inform_user(LBLWRITE_FAIL, label_db_file);

	for (type=MAC_MSEN_TYPE; type<MAX_MLSFILES; type++) {
#ifdef DEBUG
		printf("db_type = %d\t ", type,__mac_dhead[type].db_type);
		printf("dhead[%d].entry_total = %d, ",type,
			 __mac_dhead[type].entry_total);
		printf("dhead[%d].entries_bytes = %d\n",type,
			__mac_dhead[type].entries_bytes);
#endif
		fwrite(&__mac_dhead[type], sizeof(DB_BIN_HDR), 1, bfp);
		fwrite(__mac_bin_mdblist[type]->list, __mac_dhead[type].entries_bytes, 1, bfp);

		if (ferror(bfp))
			__inform_user(LBLWRITE_FAIL, label_db_file);
	}

	fclose(bfp);
}
