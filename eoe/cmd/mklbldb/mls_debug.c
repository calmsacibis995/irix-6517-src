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

/* debugging routines */

#ident "$Revision: 1.1 $"

/*LINTLIBRARY*/
#include <sys/mac_label.h>
#include <mls.h>

/*
 * dump_list
 * For debugging purpose, dump binary file information labelname and 
 * entity database entry information
 */
int
__dump_list(int type)
{
	LBLDBENT	*p;
	DBENT		*ptr;
	int i,j;
	LBLDB_BIN_ENT *llist;
	DB_BIN_ENT *dlist;

	if (type == 0) {
		printf("*** dump_list for :%s\n", __mac_mlsfile[0]);
		if (__mac_bin_lbldblist == NULL) {
			printf("no data to dump\n");
		};
		if (__mac_bin_lbldblist) {
			llist = __mac_bin_lbldblist->list;
			for (i=0; i<__mls_entry_total[type]; i++) {
				printf("llist addr:0x%x\t", llist);
				printf("name:%s, lblsize=%d, mac label at 0x%x\n", 
			   	llist->name, llist->lblsize, &llist->label);
				printf(" mac label contents are:\n");
				__dump_maclabel(&(llist->label));
				llist++;
			};
		};
		return(0);
	};

	if (type > 7) {
		for (i=1; i<7; i++) {
			printf("*** dump_list for :%s\n", __mac_mlsfile[i]);
			if (__mac_bin_mdblist[i]) {
				dlist = (DB_BIN_ENT *) __mac_bin_mdblist[i]->list;
				for (j=0; j<__mls_entry_total[i]; j++) {
					printf("addr:%x\t", dlist);
					printf("name:%s\t", dlist->name);
					printf("value:%d\n", dlist->value);
					dlist++;
				};
			};
		};
	}
	else {
		if (__mac_bin_mdblist[type]) {
			dlist = (DB_BIN_ENT *)__mac_bin_mdblist[type]->list;
			for (i=0; i<__mls_entry_total[type]; i++) {
				printf("addr:%x\t", dlist);
				printf("name:%s\t", dlist->name);
				printf("value:%d\n", dlist->value);
				dlist++;
			};
		};
	};
	return(0);
}


/*
 * dump_db
 * For debugging purpose, dump the link list for labelnames and circular
 * linked list for entity database file information
 */
int
__dump_db(int type)
{
	LBLDBENT	*lptr;
	DBENT		*ptr;
	int i;

	printf("dump type is %d\n", type);
	if (type == 0) {
		printf("*** dump_db for :%s\n", __mac_mlsfile[0]);
		printf("__mac_lbldblist at 0x%x\n", __mac_lbldblist);
		if (__mac_lbldblist) {
			printf("__mac_lbldblist->head at 0x%x\n",__mac_lbldblist->head);
			for (lptr = __mac_lbldblist->head; lptr; lptr=lptr->next) {
				printf("addr:%x\t", lptr); 
				printf("name:%s\t", lptr->name);
				printf("comp:%s\n", lptr->comp);
			};
		};
		return(0);
	};

	if (type > 7) {
		for (i=1; i<7; i++) {
			printf("*** dump_db for :%s\n", __mac_mlsfile[i]);
			printf("__mac_mdblist[%d] at 0x%x\n", i,__mac_mdblist[i]);
			if (__mac_mdblist[i]) {
				ptr = __mac_mdblist[i]->next;
			
				for (; ptr->value != 65535; ptr = ptr->next) {
					printf("addr:%x\t", ptr); 
					printf("name:%s\t", ptr->name);
					printf("value:%d\n", ptr->value);
				};
			};
		};
	}
	else {
		printf("*** dump_db for :%s\n", __mac_mlsfile[type]);
		printf("__mac_mdblist[%d] at 0x%x\n", type,__mac_mdblist[type]);
		if (__mac_mdblist[type]) {
			ptr = __mac_mdblist[type]->next;
			for (; ptr->value != 65535; ptr = ptr->next) { 
				printf("addr:%x\t", ptr); 
				printf("name:%s \t", ptr->name);
				printf("value:%d\n", ptr->value);
			};
		};
	};
	return(0);
}

/*
 * dump_it
 * Dump the mlsfile information after it is unpacked from the binary file
 */
int
__dump_it(int type)
{
	LBLDBENT	*p;
	DBENT		*ptr;
	int i,j;
	LBLDB_BIN_ENT *llist;
	DB_BIN_ENT *dlist;

	if (type == 0) {
		printf("*** dump it for :%s\n", __mac_mlsfile[0]);
		llist = __mac_lbldb.list;
		for (i=0; i<__mls_entry_total[type]; i++) {
			printf("llist addr:0x%x\t", llist);
			printf("name:%s, lblsize=%d,lp at 0x%x\n", 
			   llist->name, llist->lblsize, &llist->label);
			llist++;
		};
		return(0);
	};

	if (type > 7) {
		for (i=1; i<7; i++) {
			printf("*** dump it for :%s\n", __mac_mlsfile[i]);
			dlist = (DB_BIN_ENT *) __mac_db[1].list;
			for (j=0; j<__mls_entry_total[i]; j++) {
				printf("addr:%x\t", dlist);
				printf("name:%s\t", dlist->name);
				printf("value:%d\n", dlist->value);
				dlist++;
			};
		};
	}
	else {
		dlist = (DB_BIN_ENT *)__mac_db[type].list;
		for (i=0; i<__mls_entry_total[type]; i++) {
			printf("addr:%x\t", dlist);
			printf("name:%s\t", dlist->name);
			printf("value:%d\n", dlist->value);
			dlist++;
		};
	};
	return(0);
}
